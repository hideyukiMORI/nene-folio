# WVR-0002: RichEditのOLEコールバックの固定ABI

- Status: active
- Rule: C-012
- Issue: #1195
- Owner: NeNe Folioリナ
- Created: 2026-11-22
- Expires: 2026-12-22
- Scope: `src/ui/win32/note_pane_ole.c#ole_accept`

**hideの判断（2026-11-23、原文）: 「例外＝＞認める。必要な理由を明記。」** 実装の席が、ほかの書き方を確かめたうえで要ると判断して書き、hideが認めた。

**必要な理由（ひとことで）**: 引数の数を決めているのはWindowsの側のインターフェース（`richole.h`の`IRichEditOleCallback`）で、こちらでは減らせない。この入口を実装しないと、編集表示へ落とした画像を、RichEditが埋め込みの物として取り込むのを止められない（#1195）。例外は、その入口の2つの関数の「引数の数」の検査だけで、行数・入れ子・複雑度の上限はそのまま守る。

## 正確な範囲

`src/ui/win32/note_pane_ole.c`の2つの関数に対する`readability-function-size`だけを、関数ごとに抑制する。

- `ole_accept`（`IRichEditOleCallback::QueryAcceptData`。`this`を含めて6引数）
- `ole_menu`（`IRichEditOleCallback::GetContextMenu`。`this`を含めて5引数）

同じcheckerの行数/ネストの診断も抑制されるため、2つの関数は60行/ネスト3以内に保つ（2026-11-23の時点で、`ole_accept`は本体20行以下・ネスト2、`ole_menu`は本体10行以下・ネスト1）。
`ole_accept`は「投下かどうか・入力を止めているか・ファイルの並びかどうか」の振り分けだけ、`ole_menu`は「出さない」と答えるだけにする。
認知的複雑度10は別checkerで引き続き強制する。閾値・除外設定は変更しない。

## 理由

編集できるRichEditはOLEの投下先を自分で登録し、エクスプローラから落とされた画像を埋め込みの物として取り込む（#1195。hideが実機で確認）。
これを断ってファイルを取り込みの道へ回す入口は、RichEditが呼ぶ`IRichEditOleCallback`で、13個のメソッドの引数の数は`richole.h`が決めている。
表（vtable）の欄は省けない。引数の少ない関数を欄へ入れるには関数の型のcastが要り、ABI違反（Cでは未定義の動作）になる。

既存のCOMの実装（`src/ui/win32/accessibility_node.c`）がwaiverなしで通っているのは、そこで実装しているUI Automationのメソッドが、どれも`this`を含めて4引数以下だからである（書き方の工夫ではない）。
5引数以上のOSの入口を実装した前例は[WVR-0001](WVR-0001-winhttp-callback-abi.md)だけで、同じ形の例外である。

## リスクと封じ込め

2つの関数は状態をほとんど持たない（`ole_accept`が「ファイルが上にあるか」を1つ覚えるだけ）。
取り込みそのものは、主窓の1つの道（`folio_message_files_dropped`）へ渡すだけで、ここではパスを読まない。
実窓の試験（CTestの`image_drop_native`）が、RichEdit自身の投下先と、製品と同じ「作る関数」で作ったコールバックへ、データの物を渡して確かめる。コールバックと欄に当てた変異13本は、全部その試験が落ちた（[品質記録](../quality/2026-11-23-ole-drop.md)）。

## 解除条件

2026-12-22までに、OS指定ABIの引数だけを正確に検査対象外にできるlint設定/検査方法を評価し（WVR-0001と同じ課題）、
同等の行数/ネスト検査を保って置換する。追跡: #1195。

## 却下した代替案

- 既存のCOMの実装と同じ書き方に合わせる: 上のとおり、既存の実装は4引数以下のメソッドしか持たない。合わせられる書き方が無い。
- 引数の少ない関数、または可変個の引数の関数を表へcastして入れる: ABI違反。x64では動くが、検査を型の嘘で通すことになる。試していない。
- マクロで関数の定義を包んで検査の目から外す: 検査を弱めるのと同じ（QLT-010）。試していない。
- 自前の`IDropTarget`（`RegisterDragDrop`）: `DragEnter`と`Drop`が`this`を含めて5引数で、同じ抑制が要る。RichEditの文字のドラッグも自分で作り直すことになる。
- `WM_DROPFILES`だけで受ける（#1149の形）: 編集表示では届かない（RichEditの投下先が先に受ける。2026-11-23の実験でも、コールバックが無いと`WM_DROPFILES`も`EN_DROPFILES`も来なかった）。
- RichEditの投下先の登録を外して`WM_DROPFILES`に戻す（`RevokeDragDrop`）: ほかのアプリからの文字のドラッグも受けられなくなり、編集できる状態に戻すたびにRichEditが登録し直す。
- 文字だけのモード（`EM_SETTEXTMODE`の`TM_PLAINTEXT`）: 閲覧表示の書式（見出し・表・リンク）が同じ欄で使えなくなる。

## 参照

- Issue: #1195
- ADR: [0120](../adr/0120-image-drop.md)
- [IRichEditOleCallback](https://learn.microsoft.com/en-us/windows/win32/api/richole/nn-richole-iricheditolecallback)
- ベンチマーク／テストの証拠: [品質記録](../quality/2026-11-23-ole-drop.md)
