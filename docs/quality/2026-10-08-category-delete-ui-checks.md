# 空カテゴリ削除のUI境界の確認（#205・FR-036 / FR-038）

- 実施日: 2026-10-08 JST（実時計で確認）
- 規則: ARC-001 / ARC-003 / ARC-010 / ARC-011、C-002 / C-011 / C-012 / C-018、CNF-009 / CNF-011、QLT-008 / QLT-012 / QLT-013
- 設計: ADR 0039 補正 6。Waivers: none
- 正典経路: command登録表またはカテゴリ行メニュー → `delete_category_at` → `folio_state_delete_category`
- C1のadapter実体削除・B3の通知条件の成功済み測定は再利用し、反復していない。

## 対象単体とビルド

```powershell
. ./eng/toolchain.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/folio_tests.exe --commands-ui
python eng/conformance.py
git diff --check
```

いずれもexit 0。Debugビルドは警告・clang-tidy違反なし。
`deletecategory` / `delcat`・引数拒否時の出力保持・listed・登録順・3言語の表示名と対象なしの文言を固定した。
対象なしの結果をapplication文言・UI分類・状態の期待表へ足し、文言IDも3列と期待表へ足した。
CNF-009 / CNF-011と閉集合の網羅を維持し、schema・adapter・ゲート・閾値を変えていない。
最終 `pwsh -NoProfile -File ./eng/check.ps1` の実行SHA・ログと結果はPR本文・外部reportに記載する。

## 隔離した実Win32 UI probe

既存B3の実窓probeの手順を再利用した。Computer Useのnative pipeが接続不能だった既知の測定を再試行していない。
自分のprobe HWNDへWin32メッセージを送り、製品 `folio_window.c` をそのままincludeしてRichEditと入力面を動かした。
製品へ試験用APIを足していない。通常ユーザーのEXEとdata、既存ごみ箱を使っていない。

- sourceとPNG: `D:/NeNeFolio/design/2026-10-08/category-delete-ui/`
- build: `pwsh -NoProfile -File D:/NeNeFolio/agents/205-ui/build-probe.ps1`（最終compile/link exit 0）
- 実行: `D:/NeNeFolio/design/2026-10-08/category-delete-ui/ui_probe.exe <scenario>`
- ログ: `D:/NeNeFolio/agents/205-ui/probe-<scenario>.log`。34個の異なるscenarioが完了、末尾 `SCENARIO ... passed`、exit 0。
- UIとapplicationは製品。category portの5結果とカテゴリ台帳書込み拒否だけを制御し、カテゴリ削除の実adapterは呼んでいない。
  最後のカテゴリ用はscan portで隔離した1カテゴリを列挙し、新規カテゴリ作成は隔離fixtureへの既存adapterを使った。
- ASan / UBSanを有効化。外部probeのincludeをheaderとして扱う警告とstdioの非推奨警告は、製品ビルドの警告ゼロとは分ける。
- 実測120 DPI（125%）、Per-monitor v2、最小560×360 DIP（700×450画像）。自分の窓のPrintWindow画像をPNGへ変換して確認した。

| 境界 | scenarioと観測 |
| --- | --- |
| 押したカテゴリ | `keep-replace-small`ほか。開いているmiddleカテゴリの前の空カテゴリを直接渡す。現在文書のカテゴリ番号だけ詰まり、本文・EDIT・選択・scroll・HWND矩形・Undoを保持。write_note呼出し0 |
| 残す面 | `keep-search-small` / `keep-replace-small` / `keep-history-small` / `keep-settings-small`。内容・面・focus・検索件数・置換下見・履歴行を保持 |
| 入力欄 | 検索の欄と置換の両欄の内容・caret・Undo可否を観測。置換は第2欄にfocusを置き、処理後も同じ欄であることを確認 |
| 閉じる面 | `keep-ex-small` / `keep-palette-small`。成功時だけ閉じ、その欄のfocusを索引へ。`keep-ex-outside` / `keep-palette-outside`は元focusが本文なら本文を維持 |
| 無題 | `keep-untitled-search-small` / `keep-untitled-replace-small`。別カテゴリ削除でも無題の本文・宛先・Undoと入力欄を保持 |
| 台帳失敗 | `stale-replace-small` / `stale-search-small` / `stale-history-small` / `stale-settings-small` / `stale-ex-small` / `stale-palette-small`。後始末後の箱1回。残す面の元focusを戻し、置換第2欄を第1欄へ変えない |
| 拒否 | `files-replace-small` / `historyfail-replace-small` / `failed-replace-small` / `nonempty-replace-small` / `untitled-target-replace-small` / `filter-replace-small`。箱1回、完了後始末なし、本文・面・置換第2欄focusを保持。非空・無題宛先・filterはportを呼ばない |
| 同期境界 | `reenter-replace-small`。category port内で共有runningとowner disabledを確認。category/note重複command、カテゴリ行、select、WM_CLOSE、focuslostを送っても重ねて実行せず、元面とfocusを維持 |
| 元disabled | `disabled-owner`。元から無効なownerを有効化しない |
| 本文を採らない | `malformed-body`。未保存のUTF-16 surrogateを含むRichEditを変更せず完了し、take_textや保存を通らない境界を確認 |
| 不在 | `absent-search-small`。portのABSENTも完了後始末、本文と検索欄保持 |
| strict対象 | `target-cursor`はカーソルが現在文書より優先。`target-untitled`はカーソルなしで無題の宛先、削除は非空として拒否。`target-named`は絞り込みでcursorが無い名前付き文書の宛先。`target-none`は3カテゴリがあってもcursor・文書なしなら出力添字を触らず対象なしで拒否 |
| 最後→0→再開 | `target-last`。明示cursorを置き、実Ex欄のdelcat→Enterで0件へ。次の削除はカテゴリ専用の拒否、新規カテゴリ作成から再開 |
| 見た目と言語 | `keep-history-small-dark` / `files-replace-small-english` / `failed-replace-small-chinese`。smallのlight/dark・3言語、検索／置換／履歴の併存を実画像で確認。カテゴリ成功のnoticeは全完了条件でfalse |

probe初回compileはname_listの結果名2件を誤りexit 1で、probeだけを訂正した。
無題の初回fixtureはVIEW用renderで編集を初期化しておりUndoが無くexit 2。既存の `show_note` に合わせ、操作前にUndoがあることも確認して再実行exit 0。
最後のカテゴリの初回はcursorを置かず対象なしになりexit 2。明示cursorを置くfixtureへ訂正してexit 0。製品のstrict対象をfallbackへ弱めていない。
このfixture修正で製品sourceは変えていないので、既に成功したUI条件を一律反復していない。

## 未確認と限界

- 96 DPI / 144 DPI以上 / 画面間DPI切替、物理Ctrl+Z、IME中、OSポップアップの物理クリックは未測定。
- UIA通知読み上げは追加していない。简体中文は母語話者の確認なし。
- category port結果は制御した試験。実体削除のOS条件・部分成功・競合はC1の測定と限界を引き継ぐ。
- 同期中のmain再入と通常入力停止を測定したが、全Shell条件のメッセージ配送を保証したとはしない。
