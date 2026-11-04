# 各参加者の見出しへ接続する生命線 — #557

2026-11-04の継続品質改善で、見出しの高さが異なるシーケンス図に生命線の空白を確認した。
`emit_lifelines`が全参加者の最大見出し高から線を始めるため、短い見出しの直下に線が無かった。
製品未変更の実font/GDIで、actor/participant混在の32px、複数行の名前との混在の41pxの空白を再現した。

## 正典経路と変更

`diagram_sequence_layout.c`の生命線開始yを、`header_y + header_height`から
`header_y + actors[actor].height`へ変更する。見出しの描画に使う既存の個別寸法を参照し、
同じscene pathを延ばす。変更する製品値は1か所、周辺の改行はclang-formatによるもの。
共通イベント行には最大見出し高を使い続け、message、note、activation、frame、下部見出しを動かさない。

既存の計測→sequence layout→scene→native経路に収まり、新しいowner/API/経路が不要なため
ADR0089の範囲内と判断した。保存schema・版・依存・上限・ゲートの変更はない。

## 試行・結果

作業先は`D:/NeNeFolio/worktrees/557-sequence-lifelines`、出力は同名`outputs/`、恒久先は同名`agents/`。
固定Debug/Ninja、clang-tidy、ASan/UBSan/nullabilityを使用し、`tools/record-run.py`で入力snapshot、
実行exe、argv、raw出力、終了値を保存する。実bitmapとrun一式は全hashを照合して恒久保全する。

| 試行 | コマンド・条件 | 結果 |
| --- | --- | --- |
| build-red-01 | `cmake --build build --target diagram_render_tests folio_tests --parallel 2`、main735575f・製品未変更 | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --lifelines <red-bitmaps>` | 12枚保存、24本中8本不接続、子/記録側1 |
| build-green-01 | maince34489取込・製品修正とcore対象を追加 | 配列内の隣接文字列が曖昧としてコンパイル拒否、子/記録側1 |
| build-green-02 | 1つの文字列要素であることを括弧で明示 | 子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --lifelines <green-bitmaps>` | 206checks、24本すべて接続、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 7組の開始位置/共通footer/message行を含む対象成功、子/記録側0 |
| verify-01 | `clang-format --dry-run --Werror`、`eng/conformance.py --build-dir build`、`eng/symbols.py --build-dir build --require core application`、`git diff --check` | 整形成功、conformance 0違反、symbols 2libraries/0違反、diff成功、子/記録側0 |

全runのrecordingErrorsは空。最終build/対象試験のstderrは空、verifyはCRLF→LFのgit案内のみ。
build-green-01の修正で入力文字列・期待値・警告設定は変えていない。ゲートの除外や抑制を追加していない。
verify-01完了前に未生成のresultを一度読んだ失敗は、検証の成功/失敗に数えず、終端と結果で成功を確定した。

nativeは6組×明暗。添字0は3参加者のactor/participant混在とtitle/loop/activation、1は複数行participant、
2は複数行actor、3は同高participant、4は同高actor、5は単独actorの自己message/activation。
`lifeline-<添字>-<theme>.bmp`のthemeは0が明、1が暗。各sceneで実際のheader形状を中心xで対応付け、
生命線の始点が上部headerの下端、終点が下部headerの上端、線種が破線であることを確認する。
node数と生命線数も一致する。試験へ最大高の配置計算を複写しない。

修正前の混在/複数行の3枚と修正後の同条件の明暗6枚を視認した。空白が消え、名前へ線が重ならない。
対照の添字3〜5の明暗6枚は修正前後で全SHA256一致、変更は添字0〜2の6枚だけ。
照合原本は`bitmap-comparison.json`。破線の位相は延ばした開始位置から描かれる。
coreは18pxの決定的metricsで、同高、actor/participant混在と順序反転、3行、titleの7組を確認した。
14本の開始位置に加え、共通の下端とmessage行が既定の位置を保つことを検査する。

## 統合時の判断・限界

GitHub接続のIssue作成は403で失敗したため、既存Issue検索で重複がないことを確認してCLIで#557を作成した。
#554の統合後、未commit差分をpatch・native原文・stashSHAへ保全してmainをffした。
stash適用はnative試験の追加位置とmainのselectorで競合した。統合mainへ保存済みlifeline関数群を
無変更で挿入し、attachmentsとlifelinesの両入口を保持して解消した。core/unitは自動適用され、
最終入力でgreen build/対象実行を行った。保存したstashは削除しない。

主担当が単体で実装・自己レビュー・検証した。個別寸法の参照1か所の修正に追加の独立レビューは使わない。
規則: ARC-001/003/004/008/012、C-002/008/011/012/014/016、QLT-008/009/010/012/013、
GIT-001〜004、ADR0089。追加waiverはnone、既存WVR-0001はWinHTTP callback限定で継続する。
実app/実ノートの操作、Store再提出/公開は行わない。実font/GDIの結果を全DPIやhideの実機受理とは扱わない。
成功済みの不変検証を再利用し、最終head必須CI・正典統合・通常反映/data照合・保全整理へ進む。
確定原本は`D:/NeNeFolio/agents/557-sequence-lifelines/integration-completion.json`へ収載する。
