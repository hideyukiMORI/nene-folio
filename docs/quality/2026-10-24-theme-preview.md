# #366 — テーマ候補の即時プレビュー

2026-10-24。hideの「フォーカスした場所のテーマが一時的に反映されていない」という実機報告への対応。
#364は候補枠の修正だけで、ライブプレビューは未実装だった。[ADR0061](../adr/0061-theme-live-preview.md)を先に受理した。
規則: ARC-001/004/008/011、C-005/012、QLT-008/009/012/013。Waivers: none。

## 変更

保存値は`folio_settings`だけが所有し、applicationの表示choiceは候補を含む。
`folio_state_preview_theme`とOS外観更新が同じ非永続palette/RTF採用を共有する。確保失敗は変更前を保持する。
UIの矢印/wheel/検索/実pointer移動/UIA Focus・Selectが既存再着色へ合流し、Enter/クリック/Invokeだけが保存する。
Esc/戻る/×/面変更は直近確定へ戻す。復元やnative再着色に失敗した場合は面と理由を残し、同じpaletteでも再試行する。
Ctrl+Sは本文だけを保存する。pointerの初回MOVEはscreen座標の基準として記録し、以後の差分を移動とする。
設定schema・実行時依存・gate閾値/除外は不変。案内を3言語でプレビュー/確定/取消へ更新した。

## 実行した対象検証

証跡: `D:/NeNeFolio/agents/theme-preview-20261024/`。製品にテスト用入口は追加していない。

| コマンド | 結果 |
| --- | --- |
| 固定toolchainの`cmake --build D:/NeNeFolio/outputs/366-theme-preview/build --target nenefolio folio_tests` | Debug compile/tidy/link成功。`build-final.log` |
| `D:/NeNeFolio/outputs/366-theme-preview/build/folio_tests.exe --themes` | theme catalog/settings/state/Ex/UI text成功 |
| `folio_probe_tests.exe --themes`（既存allocation_probeへcore/applicationの確保を接続した測定ビルド） | preview取消の確保失敗を含め成功。`allocation-build.log`/`allocation-themes.log` |
| `clang-format --dry-run --Werror --style=file`（変更C/H）、`git diff --check` | 成功 |
| `python eng/conformance.py --build-dir D:/NeNeFolio/outputs/366-theme-preview/build` | File API queryを配置してconfigure後、0 violations |
| `pwsh -NoProfile -File D:/NeNeFolio/agents/theme-preview-20261024/run-probe.ps1 -Label verified` | 158 checks / 0 failures、exit 0 |

native fixtureは専用desktop・専用dataで製品`folio_window.c`とlibraryを使い、実WM_KEYDOWN/WM_CHAR/WM_MOUSEWHEEL/MOVEとUIA action境界を通す。
全体のpalette、実RichEdit文字色、候補枠画素、選択/論理行/dirty/Undo/native focus、検索0件、Enter確定後の取消、
本文保存、設定書込失敗、native再着色失敗・同色再試行、取消失敗時のCtrl+N阻止、別面への遷移、3言語の狭幅を確認した。
書込失敗はfixtureのsettings.jsonを置換不可で開き、再着色失敗は外部wrapperだけで注入する。
保存検査の初回1失敗はfixtureがRichEditのEM_GETMODIFYを保存状態の正本と誤認したもの。実ファイルには本文が保存されていた。
正本`folio_state_unsaved`と実画面の未保存印へ検査を直し、初回ログ`probe-candidate.log`も保持する。
初回起動はfixtureにfontsを用意しておらず通知箱で待機したため、fixture所有processだけ終了して同梱fontsを配置した。

500行相当（250本文行＋空行、約14KB）の24候補を連続移動し、入力処理開始から同期再描画終了まで計測した。
閲覧: 平均54.396ms / 最大59.792ms。編集: 平均10.750ms / 最大13.652ms。
Debug・実OS120DPI・この文書での値であり、全サイズ/全環境の上限ではない。JA/EN/ZH・320幅の実描画画像を確認した。

application unitは確定A→候補B→B確定→候補C→取消B、同じ保存値の確定、重複候補でRTF非生成、書込失敗、
取消確保失敗、HC中の候補移動と解除、同色のSYSTEM/固定choiceの区別を確認する。既存allocation sweepにもpreview/取消を追加した。

読み取り専用の独立レビューで、取消のnative pending確認、初回検索通知と初回合成MOVEの扱いを修正。最終の確定指摘なし。
記録は`review/review.json`。設計・実装・検証は対話席が単体で行い、前件のモデル分担は継承していない。

## 制限と統合条件

物理IME・実reader・実OSのHC切替・物理DPI移動は今回未測定。HCはapplicationの注入snapshotで確認した。
native stderrにASanの`interception_win: unhandled instruction`起動警告がある。完走したが全interception成功とは主張しない。
最終PR headの必須CI `pwsh -NoProfile -File ./eng/check.ps1` が統合条件。CI/merge/通常試遊反映/起動/整理は
`D:/NeNeFolio/agents/theme-preview-20261024/completion.json`と#366のPR Checksを正本とする。
