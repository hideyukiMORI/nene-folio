# #351 — 面の規則・実装途中のチェックポイント

2026-10-24、hideの区切りと記録の依頼に基づく。完成候補の受理記録ではない。
製品チェックポイント `f1ec547d1ac7de18c4eb7349ff56258f62670d4c`、未統合の `feat/351-surface-rules`。
規則 ARC-001/004/011、C-002/012/017/018、CNF-009、QLT-008/012/013。schema/実行時依存/gate閾値/除外不変。Waivers: none。

## 実行した確認

| コマンド・対象 | 結果と範囲 |
| --- | --- |
| `pwsh -NoProfile -File D:/NeNeFolio/outputs/351-surface-rules/a/check-a.ps1` | exit 0。固定toolchain/CMake Debugでfolio_testsのみbuild、`--surface-rules`成功。theme/UI文言/NAME_UNCHANGED直接境界。無関係全件なし |
| `pwsh -NoProfile -File D:/NeNeFolio/outputs/351-surface-rules/a/check-native.ps1` | exit 0。ui_part/dialog_theme/name_prompt/failure_boxの4TU compile/tidy。A候補76779237 |
| `python D:/NeNeFolio/outputs/351-surface-rules/a/check-io-map.py` | exit 0。音分類63値各1回、IO20件の集合と既存CNF-009対象照合が成功。静的分類であり実音ではない |
| `python D:/NeNeFolio/agents/351-surface-research/checkpoint/probe-cnf009.py` | exit 0。実表成功、メモリ上でNAME_UNCHANGEDの指定を1件除くとCNF-009が拒否。製品sourceを書き換えず既存登録の境界を確認 |
| 合流後の製品build | 固定toolchainの `cmake --build D:/NeNeFolio/outputs/351-surface-rules/a/build --target nenefolio -j 4` はexit 0。合流したDebug compile/tidy/linkが成功。型配置とnullability注記の是正後に影響TUを再確認した。 |
| 差分・規約・format | Conformance 0違反、変更24 C/Hと追加4headerのformat成功、型移動したソースと最後の注記も再確認。`git diff --check 80e2814 HEAD`成功。 |

Aの初回compile/tidyで見つかったmain61行、6引数SUBCLASSPROC、分岐複雑度、include、TrackPopupMenu符号は修正済み。
合流時にCNF-002の型同居3違反を検出し、button_pointer_state/command_band_layout/text_wrap/text_draw_requestを同名headerへ移した。
その際のHDC nullability注記不足1件も修正し、影響UIの再buildが成功した。失敗ログを成功根拠にはせず、初回と最終を別に保存した。
初期共有文言targetでは非致命のASan `interception_win: unhandled instruction` が出ている。後の対象unitは成功したが、全ログを警告なしとは主張しない。
成功した対象unit/専有TUは合流・commit・pushを理由に再実行していない。

## 設計診断との区別

独立GDI計測927理由/72ラベルはfont-onlyで、実OS120DPI・明示font120/144/168。最大理由4行と狭幅配置の設計値を支える。
合法NC除去の合成DialogBox診断は標準キー/Move/native UIAの可能性を示した。いずれも今回の新しい製品実装のGUI成功とは数えない。

## 残る確認

設定/theme/help/historyの外形・Help scope/S2/SelectionItemは未実装。新しいnative面と帯のGUI/画像/実音/fresh native UIA/IME/暗幕失敗復帰は未検証。
独立レビューは準備のみ。#351の最終head `eng/check.ps1` 必須CI・Ready・統合は未実施。文書PR #362（Issue #361）のCIはこの製品チェックポイントの受理を意味しない。
物理IME、実reader、DWM窓外影、全OS/DPIの保証はしない。通常試遊buildと配布候補は#350を維持する。

恒久証跡: `D:/NeNeFolio/agents/351-surface-research/native-implementation/`、`commandlayer-implementation/`、`checkpoint/`。
作業木と `D:/NeNeFolio/outputs/351-surface-rules/` は未完了・次回検証用として保持する。

最終実行記録は`checkpoint/ui-build-checkpoint.json`と`completion.json`。未起動のDebug exe SHA256は `6d9127c2bfe58e61ce76a363cc07d34915e4b425e1b2fac26b88409aaa564b7b`。
