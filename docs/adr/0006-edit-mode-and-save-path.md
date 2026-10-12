# ADR 0006 — 編集中の本文は RichEdit だけが持ち、編集モードを抜けるときと Ctrl+S で元の形に揃えて書き戻す

- 状態: 受理
- 日付: 2026-09-09
- Issue: #11
- 影響する規則: ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-008 / ARC-009 / ARC-010 / ARC-011 / C-002 / C-003 / C-005 / C-006 / C-014 / CNF-002 / QLT-009 / QLT-013

## 文脈

4 本の縦切り（#3 / #5 / #7 / #9）で閲覧まで通ったが、ノートを書けない。FR-006 は「編集モードで md を編集し、保存すると
同じファイルに書き戻す。同じ RichEdit をプレーンテキストで使う」と定め、ARC-004 は「編集中の本文は UI（RichEdit）が
所有し、保存の意図で application へ渡す」と定めている。この隔離区画（ARC-005）を初めて使うので、保存の時機・本文の
所有・改行の扱い・UI と application の境界の型を、実測で裏を取って固定する。

RichEdit（`RICHEDIT50W` / Msftedit.dll / Windows 11 10.0.26200）の実測（2026-09-09・Folio実装リナ）:

- `EM_STREAMIN`（`SF_TEXT|SF_UNICODE`）に LF / CRLF / CR を流すと、すべて 1 つの段落区切りに潰れる。内部表現は CR 1 個で、
  `EM_STREAMOUT` と `GT_USECRLF` は CRLF を返す。**LF は取り出せない**
- `EM_GETTEXTEX` の `CP_UTF8` は UTF-8 を直接返すが、孤立サロゲートを**黙って U+FFFD に置き換える**。
  `EM_STREAMOUT` の UTF-16 は孤立サロゲートをそのまま返す
- `EM_SETCHARFORMAT`（`SCF_ALL`）は既定の文字書式も更新し、その後の `SF_TEXT` の流し込みはこの既定で描かれる。
  `SF_RTF` の流し込みは本文の書式を RTF で上書きするが既定は壊さない。どちらの向きの流し込みも本文と書式を全置換する。
  `EM_SETBKGNDCOLOR` は流し込みで消えない
- `ES_READONLY` で作った控えは `EM_SETREADONLY` で往復できる。`EM_STREAMIN` は読み取り専用でも通る
- `ENM_KEYEVENTS` を立てると親の `WM_NOTIFY` に `EN_MSGFILTER` が `WM_KEYDOWN` / `WM_CHAR` / `WM_KEYUP` で届き、
  親が非 0 を返すと既定処理が止まる
- `EM_STREAMIN` は文字数上限を流し込んだ長さちょうどまでしか広げない。超える分は 1 文字も打てない（`EM_EXLIMITTEXT` が要る）
- `EM_STREAMIN` の先頭に BOM を付けると本文の U+FEFF になる
- 既存の `file_bytes_store`（一時ファイル＋`MoveFileExW`）は、読み取り専用の宛先・排他で開かれた宛先・無いディレクトリで
  `PERSISTENCE_UNWRITABLE` を返し、元のファイルも `.tmp` も残さない

## 決定

**編集中の本文は RichEdit だけが持つ。application は読んだ本文（`note_text`）と表示モード（`pane_mode`）を持ち、
「編集モードを抜ける」意図と Ctrl+S の意図で UTF-16 の本文を受け取り、core で UTF-8 に検証・変換し、元の本文の改行の形に
揃え、同じなら書かず、違えば `persistence_port.write_note` で原子的に書き戻す。**

具体的には:

1. **保存の時機は 2 つ。** 編集モードを抜けるとき（「閲覧」札・別ノートの選択・窓を閉じる）と Ctrl+S（抜けない）。
   破棄（保存せずに抜ける）は持たない。Escape は編集モードでは何もしない（破棄の意味を先取りしない）
2. **同じなら書かない。** application が読んだ本文と、受け取って正規化した本文を core の `note_text_equals` で比べる。
   dirty flag を application に持たない（本文を 2 か所に持つことになり ARC-004 に反する）
3. **改行の形は元の本文に合わせる。** core の `note_text_line_ending`（最初の改行で LF / CRLF を決める。無ければ LF）と
   `note_text_from_editor`（CR / CRLF / LF をその形に畳む）で正規化する。末尾改行の有無は触らない。**BOM は書かない**
   （読みで捨てている。BOM は本文ではなく、無いのが現在の既定）
4. **UI から application への本文は UTF-16 の単位列で渡す。** C-014 は「変換は ui / adapters の境界で」と書くが、
   この 1 本の意図に限り application が core の `utf8_text_create` を呼ぶ。孤立サロゲートは `FOLIO_STATE_NOTE_MALFORMED`
   として保存しない。理由は 2 つ: 壊れた本文をどうするかの判断を UI に置かない（ARC-011）、`EM_GETTEXTEX` の `CP_UTF8` は
   黙って直すので使わない（ARC-009）。変換の実装は引き続き core の 1 か所
5. **ドロワーは別ノートを選ぶ前に主窓へ同期メッセージで「編集中なら先に保存」を頼む。** UI 内の通知 1 つ
   （`folio_message_edit_flush`）で、主窓が本文を取り出して「抜ける」意図を出し、結果を返す。READY でなければ選択を進めず、
   失敗の 1 行を出す。`WM_CLOSE` も同じ手順で、失敗なら閉じない
6. **失敗は状態を変えない。** 書き戻せなければ `FOLIO_STATE_NOTE_STORE_FAILED` を返し、mode は EDIT のまま・読んだ本文も
   そのまま。RichEdit の本文は UI が持っているので消えない（FR-015）
7. **札は 2 つ描く。** 「閲覧」「編集」を頭の右端に並べ、有効な側だけ面塗り、無効な側は頭の文字色。矩形は 1 つの関数が
   描画と当たり判定の両方に与える。無効な側のクリックが意図になる。札の上は `HTCLIENT`

## 強制

- モジュール境界（core が `windows.h` を含まない、ui が adapters を呼ばない）は ARC-002 / ARC-003 が **active**
- 1 ファイル 1 型定義（`line_ending.h` / `pane_mode.h`）と外部関数の接頭辞は CNF-002 が **planned** だが、`eng/conformance.py` は
  走って落ちる（2026-09-09 実測）
- `write_note` の関数ポインタと文脈の不完全型は C-006 のレビュー事項。テストは `tests/unit/state_tests.c` の偽物で定義する
- 「application に UTF-16 を入れるのはこの意図だけ」はレビュー事項（C-014）。2 本目が要るなら新しい ADR
- 分岐 90% は QLT-009 が **active**。新しい確保経路は `tests/unit/allocation_tests.c` のシナリオで全部失敗させる
- 実機の確認は `docs/quality/gate-proofs.md` 第 5-e 節（QLT-013）。IME は無人で測れないので施主の実機確認事項

## 結果

得る: 閲覧と編集が同じ控えで往復し、保存は「抜けるとき」と Ctrl+S の 2 つに固定される。改行の形が保たれるので、
外部のエディタや git と併用しても差分が汚れない。壊れた入力は黙って直さず、型のある失敗で利用者に見える。

失う・残る:

- 🔴 BOM 付きの md を開いて保存すると BOM が落ちる。本文は変わらない
- 破棄が無いので、編集モードを抜ける操作はすべて保存になる。取り消したい変更は保存前に RichEdit の Undo（Ctrl+Z）で戻す
- IME（かな漢字変換の未確定文字列と Ctrl+S の競合）は実機でしか確かめられない
- 外部で変わった md は検知しない（仕様の非要件）。保存は最後に読んだ本文との比較なので、外部の変更は上書きされる
- 文字数上限は `EM_EXLIMITTEXT` で毎回広げる。`EM_STREAMIN` が上限を縮めるかは実装時に測る

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 打鍵ごとに自動保存する | 打鍵ごとにファイルを置き換えることになり、`.tmp` の作成と `MoveFileExW` が入力の遅延になる。保存の失敗も打鍵ごとに出る |
| 保存ボタンを置く | デザイン正本に無い。「抜けるとき保存」と Ctrl+S で足りる |
| Escape で編集を抜ける（保存する） | 利用者は Escape に「取り消し」を期待する。破棄を持たないこの縦切りでは Escape に意味を与えない |
| 破棄（保存せずに抜ける）を持つ | 確認ダイアログか二段の意図が要り、今回の縦切りより大きい。要るなら Escape と一緒に別 Issue |
| `EM_GETTEXTEX` の `CP_UTF8` で UTF-8 を直接取る | 孤立サロゲートを黙って U+FFFD に置き換える（実測）。ARC-009 に反する |
| UI で UTF-8 に変換してから application へ渡す | C-014 の字面には沿うが、変換に失敗したときの判断（保存しない・1 行を出す）が UI に落ちる（ARC-011） |
| application に dirty flag を持つ | 「変わったか」を本文とは別に持つと同じ事実を 2 か所に持つ（ARC-004）。本文の比較で足りる |
| 末尾改行の有無を元の本文に合わせる | RichEdit は末尾の段落記号を勝手に足したり落としたりしない（`EM_STREAMOUT` の実測）。利用者の意図した空行を消す規則になる |
| BOM を覚えて書き戻す | `note_text` が本文以外の事実を持つことになる。BOM は本文ではなく、無いのが現在の既定 |
| 編集用の RichEdit を別に作る | 実測で同じ控えの流し直しが本文と書式を全置換する。控えを 2 つ持つ理由が無い |
| ドロワーが `folio_state_pane_mode` を見て選択を拒む | 判断が UI に落ちる。本文を持つ主窓に頼むほうが正典経路が 1 つになる |

## 参考

- [ADR 0004](0004-first-drawer-slice.md)（ポートの文脈の不完全型・`_Nonnull`）/ [ADR 0005](0005-rigid-design-and-os-theme.md)（札の見た目）
- [EM_STREAMOUT](https://learn.microsoft.com/en-us/windows/win32/controls/em-streamout) /
  [EN_MSGFILTER](https://learn.microsoft.com/en-us/windows/win32/controls/en-msgfilter) /
  [EM_EXLIMITTEXT](https://learn.microsoft.com/en-us/windows/win32/controls/em-exlimittext)

## 2026-10-09 の補正（#227 — 保存できる本文の上限）

coreの単一目的 `persisted_size_limit.h` が持つ `persisted_size_limit`（16 MiB = 16*1024*1024 bytes）を、
`note_text` と `file_bytes` の唯一の上限にする。note_textはBOM除去後・終端NULを除くUTF-8本文、
file_bytesはBOM/JSON escape/書式を含むraw serialized fileを数える。依存はadapter→coreの既存方向だけ。

`note_text_create` / `note_text_from_editor` は不正UTF-8を先に `NOTE_TEXT_INVALID_UTF8` で拒否し、
妥当な本文の正規化後byte数が上限を超える場合は `NOTE_TEXT_TOO_LARGE`。失敗時outは変えない。
from_editorは改行を元の形へ揃えた長さを確保前に数え、raw入力長だけで拒否しない。
UTF-16→UTF-8変換自体の実OOMは従来どおりOOMとする。

applicationは専用 `FOLIO_STATE_NOTE_TOO_LARGE` と3言語の既存失敗表示へ写す。
通常/初回/別名保存と変更問い合わせ（`:q`を含む）はarchive/render/create/write前に拒否し、
mode/所有済みbody/対象/本文入力/選択/Undoを保つ。`:q`は既存失敗経路で退出を中止する。
新しい確認面や退出取消の文言は追加しない。正常same-body/no-opの意味は変えない。
既存の先行synchronizeの順序は維持し、以前の台帳/検索写し/改名保留の修復は今回本文の拒否と区別する。
変更問い合わせのサイズ判定は名前付きEDIT。無題は従来どおり先にCHANGEDを返し、VIEWはSAMEとなる。

file_bytesのread保護は現16 MiBのまま。store/createも共通store入口でraw長を検査し、
temporary fileを作る前に `PERSISTENCE_UNWRITABLE` とする。generic persistence enumは増やさない。
canonical本文16 MiBはBOM無しで保存/再読込可能。外部BOM付きraw16 MiB+3は従来どおり読込拒否。
上限を緩めたり、別の巨大本文経路を作ったりしない。保存schema/依存/gateは不変。

## 2026-10-09 の補正（#232 — NULを本文として受理しない）

main `41f9b4e` の実Win32 probeで、U+0000を含む本文は通常編集のstream往復で保持されず、
履歴復元の `EM_REPLACESEL` ではNULより前だけになることを確認した。NULは妥当なUnicodeだが、
この製品の本文として安全に編集・復元できないため、次の受理条件を採る。

- `note_text` は長さの内側にNULを含まない。共通の生成時検証が、不正UTF-8を先に
  `NOTE_TEXT_INVALID_UTF8`、次に埋め込みNULを `NOTE_TEXT_EMBEDDED_NUL` で拒否する。
  BOM除去・上限・改行正規化の経路は維持し、拒否時にoutを変更しない。検証用の確保は増やさない。
- ノートと履歴の読取は、読んだバイト列を同じ生成経路で検証する。NULは
  `PERSISTENCE_MALFORMED` とし、既存の読めないノート/履歴の表示へ写す。元ファイルは変更しない。
- 編集本文の保存・初回保存・別名保存・名前付きEDITの変更問い合わせは、既存の
  `edited_text` から同じ検証を通る。NULは `FOLIO_STATE_NOTE_MALFORMED` とし、
  その3言語の理由を「壊れている、または扱えない文字」を含む意味へ揃える。
  今回のarchive/create/writeは行わず、既存のbody/モード/宛先/履歴とUI入力を保持する。
  先行synchronize、無題のCHANGED、VIEWのSAMEと保存済み本文の複製は従来どおり。
- 汎用のUTF-8/UTF-16 codecはU+0000を受理し続ける。本文以外へこの制約を広げず、
  NULの自動削除や置換、別の保存経路、既存ファイルの一括変換は追加しない。

これはARC-009（黙って欠落させない）とC-007（生成時の不変条件）を本文型で守る補正である。
保存schema・依存・ゲートは変わらず、waiverは無い。対象core/application/実OSの回帰検証を残す。

## 2026-10-10 の補正（#234 — 本文を変換せず取り出す）

main `09a198f` の実Win32 probeで、`EM_STREAMOUT` は本文中のU+FFFDを空白、
U+000BをCRLFへ変えていた。`EM_GETTEXTEX` の `GT_DEFAULT` もU+FFFDを空白へ変える。
一方、`GT_RAWTEXT` / codepage 1200は両文字・補助平面文字・孤立サロゲートを保持した。
CP_UTF8を使わない決定4は維持し、UTF-16の取得を次の1経路へ揃える。

- `note_pane_text` は `EM_GETTEXTEX(GT_RAWTEXT, 1200)` で内部のUTF-16単位列を返す。
  段落区切りはCR 1個で、保存時のLF/CRLF化は既存の `note_text_from_editor` が担う。
  `GT_USECRLF` は付けない。空本文と末尾改行の有無を保持し、孤立サロゲートは既存の拒否へ渡す。
- 保存・変更問い合わせ・検索・置換はこの公開入口を共用し、`note_pane_display_text` と
  stream-out専用の状態を削除する。行番号は同じ内部取得関数を専用bufferで使い、借用本文を無効化しない。
- 読取は本文・選択・Undo・変更印を変えない。確保失敗時は出力引数と既存bufferを保つ。
  schema・依存・ゲート・NUL拒否は不変。U+0007とU+2028/U+2029の流し込み時の変換は別の境界で、
  この補正では変更しない。

APIの根拠: [GETTEXTEX](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-gettextex)。
対象検証は保存/検索/置換/行番号の直接の境界に限り、物理IMEや無関係な画面操作は繰り返さない。

## 2026-10-12 の補正（#242 — 編集で保てない文字を含む本文）

RichEdit は平文の流し込み（`EM_STREAMIN` の SF_TEXT|SF_UNICODE・`EM_SETTEXTEX`・`EM_REPLACESEL`・TOM）で
U+0007→U+0020、U+2028→U+000B、U+2029→U+000D に変え、RTF に替えても製品の face では同じに変わる（2026-10-12 実測）。
変更判定は元の本文との比較なので、該当ノートは編集に入って何も打たずに切り替えるだけで md が書き換わっていた。

- core の `note_text_editability` が 3 文字の表 1 か所で `NOTE_TEXT_EDITOR_ALTERS` を返す。確保しない。
- application は名前のある文書の `begin_edit` と、選んだ版の `restore_history` を `FOLIO_STATE_NOTE_UNEDITABLE` で断り、
  本文・mode・一覧を変えない。編集のまま該当ノートへ切り替えたときは、その本文を閲覧で開く。
- 閲覧・検索・別名保存・改名・ごみ箱は従来どおり。NUL（#232）と違い、読めなくはしない。入力経路は変えない。
- 編集のまま該当ノートへ切り替えて閲覧で開いたときは、application が `folio_state_select_notice` で事実を持ち、ui が右ペイン下端の 1 行（ADR 0039 補正 4 の置き場）で知らせる（#258）。

## 2026-10-12 の補正 2（#245 — 編集本文も閲覧と同じ RTF で流し込む）

2026-10-12 の実測（`D:/NeNeFolio/agents/render-perf-1012/`・`242-rtf-edit-probe/`）で、編集へ入るときの平文の流し込み
（`EM_STREAMIN` の SF_TEXT|SF_UNICODE）は閲覧より重く（512 KiB で 0.8〜1.7 秒）、同じ本文を閲覧と同じ UTF-8 生バイトの
RTF で流すと 1 MiB で約 5 倍速かった。取り出した本文・face・高さ・Undo の段数・変更印は平文と同じで、違いは無い。

- core の `markdown_rtf_plain`（#250）が編集用の RTF を作る。頭（fonttbl・colortbl・段落開始）は閲覧と共有し、本文は
  Markdown を解釈せず `\`・`{`・`}` だけを escape して UTF-8 の生バイトで書き、改行（LF / CRLF / 孤立 CR の各 1 つ）を
  `\par ` 1 つにし、末尾に `\par ` を 1 つ足す。`rtf_palette.text` は ui の編集色と同じ値なので `\cf1` のままでよい。
- application の `folio_state_pane_edit_rtf`（現在の本文・無題は空）がその RTF を所有し、ui は `folio_state_edit_rtf` で
  借りる。UTF-16 への変換は編集の入口から消える。履歴の版を戻すときは、閲覧中なら保存済みの本文をこの RTF で流してから、
  版の本文を従来どおり UTF-16 の全文置き換え（`note_pane_replace`・Undo 1 単位）で入れるので、#250 で足した
  `folio_state_history_edit_rtf` は使い手が無く、#245 で取り除いた。
- ui の `note_pane_edit` は RTF（UTF-8・長さ付き）を受け、`note_pane_render` と同じ `EM_STREAMIN`
  （`(CP_UTF8 << 16) | SF_USECODEPAGE | SF_RTF`）で流してから読み取り専用を解き、変更印を消す。流し込みの経路は閲覧と
  編集で 1 本になる（決定 2 の「平文で流し込む」はこの補正で置き換える）。
- 取り出し（`note_pane_text` の GT_RAWTEXT・#234 補正）・保存時の改行正規化・NUL 拒否・#242 の編集拒否は変えない。
  RichEdit が平文でも RTF でも変える 3 文字（#242）は、この補正の前に application が断っている。

対象検証は #234 の実 Win32 90 項目（保存・検索・置換の位置・Undo・変更印・改行・実保存と履歴）と #242 の RTF 編集 probe
（往復 12 ケース・打鍵後の保持・書式・時間）を製品の経路で再測し、編集開始の時間を平文と比べる。
結果は [確認記録](../quality/2026-10-12-rtf-edit-checks.md)。ASCII の本文では編集開始が平文の約 0.21〜0.24 倍になったが、
CJK（3 バイトの字）の本文では 0.52〜0.75 倍にとどまる。