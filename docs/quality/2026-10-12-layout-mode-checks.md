# #290 — 一覧・両方・ノートと窓幅復元の確認

- 日付: 2026-10-12。基点main `7afc01c`、枝`feat/290-layout-modes`。
- 設計: [ADR0047](../adr/0047-three-layout-modes.md)。FR-013/016/018/024/039、ARC-001/002/004/011、C-002/012/018、QLT-008/012/013。
- 証跡: `D:/NeNeFolio/agents/ui-layout-modes-20261012/`。Waivers: none。

## 対象と静的レビュー

applicationの表示状態、共通の領域計算と上部操作、一覧窓の縮幅/復元、可視区画へのfocus、表示変更時の本文保持を対象にした。
source/testは既存14ファイルと新規header2ファイル。保存schema版3・公開済みv0.1.0は変更していない。
設計席が全差分を確認し、別席の読み取り専用レビューを行った（`independent-review.md`）。

- NOTEで閲覧/保存/現在文書のごみ箱操作後に主窓へ戻るfocusを、可視paneへ解釈する共通経路に修正。
  OSがNOTEのrootへfocusを戻す場合も、surfaceが閉じ、実paneが可視/有効なときだけ本文へ正規化する。
- 多画面のwork area選択で横位置だけ補正すると共通headerが全画面から外れる座標例を確認し、headerの縦位置も補正した。
  これはソースと座標による確認で、実複数画面の測定ではない。
- `folio_regions`を専用headerへ分け、CNF-002の型配置を維持した。ゲートの除外・抑制・閾値変更は追加していない。

## 対象検証

| 確認 | 実コマンド/範囲 | 結果 |
| --- | --- | --- |
| 整形/Debug build | `pwsh -NoProfile -File D:/NeNeFolio/agents/ui-layout-modes-20261012/check-candidate.ps1`、続いて`check-final.ps1` | 変更16ファイルformat。File API query付き新規configure後、`cmake --build D:/NeNeFolio/outputs/290-layout-modes/candidate-20261012-1936/build --target nenefolio folio_tests --verbose`成功。clang-tidyを含む |
| 規約 | `python eng/conformance.py --root D:/NeNeFolio/worktrees/290-layout-modes --build-dir D:/NeNeFolio/outputs/290-layout-modes/candidate-20261012-1936/build --today 2026-10-12` | 0違反 |
| 対象unit | `D:/NeNeFolio/outputs/290-layout-modes/candidate-20261012-1936/build/folio_tests.exe --layout-modes` | applicationの文書/本文/port保持、commandの解析/列挙/引数拒否、既存ui_text検査が成功 |
| 通常GUI | `pwsh -NoProfile -File D:/NeNeFolio/agents/ui-layout-modes-20261012/probe-layout.ps1`、`probe-layout-remaining.ps1`、`probe-layout-completion.ps1` | 成功済みの範囲を保持して残部だけを続行。最後のcompletionは80 assertions成功、exit 0。診断と失敗は下記で区別する |
| 最大化GUI | `pwsh -NoProfile -File D:/NeNeFolio/agents/ui-layout-modes-20261012/run-isolated-max.ps1` | 専用の非表示desktopで28 assertions成功、exit 0 |
| 入力 | `candidate-inputs-final.json` | 新規headerも含むsource/tests/eng/CMake/font等のhash。commit/統合時に同じ入力を照合して結果を再利用する |

初回buildは、commandを3つ加えて整形した既存`verify_catalog`が65行となり、C-012の60行上限で失敗した。
catalogの列挙と検索を別の関数に分け、同じ検査内容を維持した。修正後の差分buildと残りの対象検証を行い、
最初のログを`check-candidate-console.log`、修正後を`check-final-console.log`として保持する。

## 実OS確認

Windows build 26300、120 DPI（125%）、日本語/暗色。通常1000×750物理px、一覧400×750物理px。
候補exe SHA256: `763A3420D03B4BC3E276AF0695B39585242E28D53731628354D3EB0275202947`。
PID/HWNDを所有fixtureへ限定し、画像は`tools/capture-window.ps1`のDPI対応PrintWindowで撮影する。

通常GUIの対象確認を受理した。初期の両方、一覧とノート、狭幅設定1列、行保持の前後、無題/空本文のUndo後の画像を設計席が視認した。

- 通常3modeの矩形・可視focus、一覧での320幅、同mode再選択、狭い窓を360 logical pxへ手で変えた後も元の1000物理px幅へ復元することを確認。
- 一覧のsettingsは1列で開閉でき、一覧を維持する。Enterは両方へ、ノート表示のCtrl+Shift+Fは両方のfilterへ移る。
  Ctrl+Shift+Fは既存accelerator IDを所有HWNDへ送る合成入力で、物理キー操作の測定ではない。
- 本文Escは既存保存後に両方の索引へ戻る。NOTEで閲覧へ戻った後のfocusは可視本文となる。
  一覧の単クリック/kによる選択は一覧のまま。Exの3語も同じ表示経路へ入る。
- 長い名前付きdirty本文、無題、空本文でtext/hash、変更印、Undo可否、selection、可視論理行を保持し、
  純切替ではdataのmd/json集合・hash・mtimeが不変。無題の挿入を実Undoで消し、空本文になる削除を実UndoでTEMPへ戻した。

`gui-normal-partial.json`は通常切替など54成功の後に最大化の前面guardで中断した記録。
`gui-remaining-results.json`は正規化した行保持・狭幅設定・Enter/filterなど15成功の後にEsc合成入力で中断した記録。
残部の`gui-completion-results.json`は80成功/0失敗で正常終了し、所有fixtureのstderrは空、前面不変を確認した。
全体の識別・入力286件の一致・終了後の所有プロセス0件は`verification-summary.json`に記録する。

最初のprobeは旧root座標を新drawerのlocal座標として使い、別の短い本文を選んだためfixture検査で止まった。
helperの座標と選択本文の事前検査を修正し、製品sourceは変えていない。
また、EM_LINEINDEXの内部CR座標とWM_GETTEXTのCRLFを混ぜた計測を補正し、
`final-named-lines-before.png` / `after.png`がともにLine16から見えていることを確認した。
Escの最初のPostMessage系列はlParam=0でRichEditへの通知が不十分だった。ENM_KEYEVENTSが有効であることを確認し、
同期WM_KEYDOWNと有効なkey情報で既存保存/索引復帰が成功した（`escape-diagnostic.json`、7 assertions）。
helperだけを直して残部を続け、製品の失敗として修正したとは扱わない。

### 最大化の分離確認

通常デスクトップの初回試行では、WS_EX_NOACTIVATEを付けた所有窓でもSetWindowPlacementによる最大化で前面照合が失敗した。
その方法を中止し、fixtureを終了した。before/直後のHWND数値は初期helperに保存されておらず、後から確定していない。
事後のforegroundはWindowsTerminalのHWND265850、PID37836。TOOLSの2プロセスへ入力や終了操作は行っていない。
`gui-final-console.log`、`max-foreground-diagnostic.json`は成功証跡と分けて保持する。

`pwsh -NoProfile -File D:/NeNeFolio/agents/ui-layout-modes-20261012/run-isolated-max.ps1`はexit 0、28 assertions成功。
CreateDesktopWとSTARTUPINFO.lpDesktopで専用の非表示desktopへworkerとfixtureを置き、SwitchDesktopは定義/呼出ししない。
worker PID56736、通常desktopのforegroundは265850→265850、所有アプリは正常終了、stderrは空。

- 最大化→一覧は通常窓の幅320 logical pxとなり、一覧→両方は最大化へ戻る。
- 退避した通常位置の4辺を保持し、最後の通常復元は元の1000px幅へ戻る。
- 長い名前付きdirty本文のhash/長さ、変更印、canUndo、selection、正規化した可視論理行15（0-based）を保持。
  純切替の前後でdataのmd/json集合・hash・mtimeが不変。

証跡は`max-isolated-results.json` / `max-isolated-launch.json` / `max-isolated-worker.log`。
初回の隔離launcherは不要な同desktopへのSetThreadDesktopでERROR_BUSYとなり、所有childを終了した。
同一desktopなら呼ばないよう補助だけを直して残りを実行した（`max-launch-initial-error.txt`）。
これは非表示desktopでの実Win32挙動の確認であり、表示中desktopの物理操作や実複数画面の測定ではない。
APIの根拠: [CreateDesktopW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createdesktopw)、
[STARTUPINFOW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-startupinfow)、
[SwitchDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-switchdesktop)。

## 統合と範囲

最終headのフルゲートは必須CIへ集約する。成功済みの設定matrix、カテゴリ永続化回帰、local全件unitは再実行していない。
別OS/DPI・物理IME・実複数画面・hide目視受理とは呼ばない。
CI、main、通常exe、成果物保持、作業木と一時出力の整理は紐付くPRと`D:/NeNeFolio/agents/ui-layout-modes-20261012/completion.json`で確認する。
