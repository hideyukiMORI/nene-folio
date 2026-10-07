# ごみ箱操作のUI境界の確認（#197・FR-035 / FR-038）

- 実施日: 2026-10-08 JST（実時計で確認）
- 規則: ARC-001 / ARC-003 / ARC-010 / ARC-011、C-002 / C-011 / C-012 / C-018、CNF-009 / CNF-011、QLT-008 / QLT-012 / QLT-013
- 設計: ADR 0039 の補正 4、ADR 0040。Waivers: none
- 製品経路: command 登録表 → `execute_trash_note_command` または行メニュー → `trash_note_at` → `folio_state_trash_note`
- 前提 B1 / B2 の adapter・状態単体の成功は再利用し、この変更のために再測定していない。

## 対象単体とビルド

```powershell
. ./eng/toolchain.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/folio_tests.exe --commands-ui
python eng/conformance.py
git diff --check
```

実行結果はいずれも exit 0。`--commands-ui` はコマンドと文言の単体だけを走らせる入口。
`:trashnote` / `:trash`・引数拒否・未知の綴り・拒否時の出力保持・登録順・listed・3言語のラベルと通知を固定した。
中核の列挙と4登録箇所、文言の表は既存の CNF-009 / CNF-011 で検査し、ゲートを変更していない。
最終の唯一の完了定義 `pwsh -NoProfile -File ./eng/check.ps1` の実行ログと結果は、この変更の PR 本文へ記載する。

## 実Win32 UI probe

Computer Use スキルの全文と `guidance.md` / `api.md` / `confirmations.md` を読んでから、`node_repl` の `@oai/sky` を初期化した。
`list_windows` は native pipe 接続が os error 2 で失敗し、再試行・kernel reset後の再初期化でも同じだった。
そのため、製品 `folio_window.c` そのものを include した独立のprobeで実HWNDとRichEditを起動して確認した。
製品に試験APIを足していない。キーはそのprobeの自分の HWND へ送った Win32 メッセージで、物理キーボードとは呼ばない。

- 場所: `D:/NeNeFolio/design/2026-10-08/trash-ui/`（`ui_probe.c` / `ui_probe.exe` / `data/` / 各 PNG）
- コマンド: `pwsh -NoProfile -File D:/NeNeFolio/agents/197-ui/build-probe.ps1`、`ui_probe.exe <scenario>`
- ログ: `D:/NeNeFolio/agents/197-ui/probe-<scenario>.log`。各完了ログの末尾は `SCENARIO ... passed`、exit 0
- probe は ASan / UBSan を有効にしている。画像は自分の窓の `PrintWindow(PW_RENDERFULLCONTENT)` からBMPを記録し、PNGへ変換した。
- 環境: Windows、実測 120 DPI（125%）、Per-monitor v2。最小 560×360 DIP は画像 700×450 pixel。
- 通常成功・失敗・台帳失敗は port の結果と台帳保存の拒否だけを制御し、UI と application は製品を使う。
  `current-edit-real-new` と `other-unreadable-real` は隔離データへの実adapterを呼んだ。

| 境界 | scenario と観測 |
| --- | --- |
| 現在文書 | `current-view` / `current-edit-real-new` / `current-search-switch`。NONE / VIEW / RichEdit空・面close・索引focus。編集本文がportの前にmdへ保存される |
| NONEの次 | `current-view` はWM_CLOSE、`current-edit-real-new` は新規、`current-search-switch` は別ノートの選択。明示保存はNOTHING_SELECTED、保存前処理はREADY |
| 現在文書の台帳失敗 | `current-stale`。後始末済みで失敗の箱1回、通知なし。NONEの保存前処理で台帳を再試行し、拒否中はLEDGER_UNSYNCEDを保つ。書込を許すとREADY、終了可能 |
| 別ノート | `other-search-small` / `other-replace-small` / `other-history-small` / `other-palette-small` / `other-settings-small` / `other-ex-small`。本文・EDIT・選択・scroll・RichEdit矩形・focus・開いた面・履歴・検索件数・置換下見保持。元の編集をEM_UNDOで戻せる |
| 入力欄 | 検索・Exは欄の内容、caret、Undo可否も同一。置換は第1欄の内容・caret・Undo可否と、第2欄のfocus保持を確認 |
| 読めない未選択md | `other-unreadable` / `other-unreadable-real`。その行を選ぶ前処理なしでportへ渡り、開いている編集本文を保持 |
| 無題の本文 | `other-untitled-search-small`。別ノートの操作で無題・未保存本文・Undo・検索欄を保持 |
| 対象なし・無題コマンド | `none` / `untitled`。portを呼ばず失敗の箱1回。文書の種類とownerのenabledを保持 |
| ごみ箱の失敗 | `failure-busy` / `failure-unavailable` / `failure-failed`。箱1回・通知なし、一覧と現在文書と未保存本文を保持 |
| 別ノートの台帳失敗 | `other-stale-search-small` / `other-stale-replace-small` / `other-stale-history-small`。箱後にも元の有効focus・面・本文・Undoを保持。置換の2欄目から1欄目へ移さない |
| 同期呼び出しの再入 | `other-reenter-search-small`。port内でrunning=true・owner disabledを確認。重複command・WM_COMMAND・対象select・WM_CLOSEとfocuslostを送っても二度目を実行せず、戻るとownerと元focusが復旧 |
| 元のenabled | `disabled-owner`。最初からdisabledのownerは戻ってもdisabled。実ユーザー入力の停止はEnableWindowのOS契約に従う |
| 入力からの実行 | `current-ex-entry-new` / `current-palette-entry-new`。実EDITの語からEnterを渡し、同じ削除と後始末へ到達 |
| 通知の配置と言語 | 上記のsmall各面と `other-search-small-dark`。下端24 DIPを予約、本文HWNDは不変、一覧の選択行は表示内。検索件数/置換下見/履歴を上書きしない。`current-view-small-english` / `current-view-small-chinese` は3言語の下端文言を実画像で確認 |
| 通知の寿命 | 面close・次command・文書selectで消える。タイマーと時刻APIなし |

28個の異なるscenarioが完了した。最初の`current-stale`はprobeの期待値をLEDGER_STALEとして失敗したが、
既存同期契約の実結果はLEDGER_UNSYNCEDなのでprobeの期待値を修正した（製品の同期失敗をREADY化していない）。
`other-stale-history-small`では製品のfocus落ちを再現し、この操作に限定した復帰を実装して3面で確認した。

## 未確認と限界

- 96 DPI / 144 DPI以上 / 別画面へのDPI切替、物理Ctrl+Z・IME中の操作は未測定。
- OSのごみ箱設定変更、容量超過、別Windows版はB1の限界を維持。
- UIAによる主窓の通知の読み上げは未対応。简体中文は母語話者の確認を得ていない。
- `EnableWindow`による通常入力停止とmainの再入拒否を測定した。OSの全Shell条件でメッセージが届かないとは主張しない。
- probeは行メニュー通知の受け口を直接呼ぶ。OSポップアップメニューの物理クリックは未測定。
- 動作中のユーザーアプリと既存データは使っていない。probe終了後にその窓とadapterを破棄した。
