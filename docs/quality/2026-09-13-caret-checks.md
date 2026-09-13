# 編集本文のCtrl＋hjklの確認（#51 / ADR0019）

## 実行した測定

2026-09-13、Windowsの同一プロセス内に非表示の親と製品note_paneを作った部品測定。
既存アプリ・データ・入力デバイスを操作しない。測定用SetKeyboardStateは測定スレッドだけに適用し、製品は修飾状態を読み書きしない。
通常矢印のWM_KEYDOWNで得た選択位置とCtrl＋hjklの位置を比較し、同時に本文が不変であることを検査した。

- ASCII複数行、日本語・絵文字・結合文字を含む本文、180px幅の折り返し、空の本文。
- 先頭・途中・末尾の6位置と全選択、4方向の計112比較、Undo前の1比較。合計113、失敗0。
- Ctrlなし、Shift/Alt併用、別HWND、IME composition記録中、Ctrl+S/Pは翻訳対象外。
- 移動後もUndoが可能で直前の挿入を戻せる。Backspaceは削除、Enterは改行。

初回測定ではBackspace、次の測定ではEnterが失敗した。測定がWM_CHARだけを送っていたため、
通常のキー通知順（WM_KEYDOWN→WM_CHAR→WM_KEYUP）へ測定側を修正し成功。製品のキー経路は変更していない。
折り返しや文字境界の期待値は製品のTOM呼出しを複製せず、同じRichEditの通常矢印から取った。

原本・実測ログは作業ルートの`out/design/2026-09-13/caret-probe.c`と`caret-probe.log`。
`eng/toolchain.ps1`読込後、固定clang-clで測定Cと`src/ui/win32/note_pane.c`を直接結合し、user32/kernel32をリンクして実行した。
結果: `comparisons=113 failures=0`、exit 0。製品ビルド（clang-tidy含む）とCTest 2/2も成功。
Ready直前の正式フルゲートとCIの最終SHA・結果は#51のPRに記録する。

## 実機で残る確認

この測定は物理キー・HHKB配列・IME候補窓・描画やスクロールの目視確認ではない。
Computer Useは直近15:45 JSTにnative pipe unavailable / os error 2。

1. 編集本文のCtrl+h/j/k/lで左右と表示行の上下に動き、長押し・選択後も自然に移動する。
2. 日本語変換中に同じキーを押してもIMEを妨げない。Backspace・Enter・通常のhjklを入力できる。
3. 移動で本文が変わらず、Ctrl+Zで直前の入力を戻せる。Ctrl+SとCtrl+Pが従来どおり使える。
4. 閲覧本文・INDEX・コマンド入力へ追加のCtrl＋hjklが漏れず、ヘルプに4方向が読める。

以前のexeのhide確認を#51の成功として流用しない。Waivers: none。
