# sequence activationの入れ子と横幅 — #548

2026-11-04、継続的な品質改善でactivationの描画位置と幅予約を照合した。
短い参加者名では右側40 logical pxを予約する一方、16段目のactivationは生命線から右へ96pxまで広がる。
これは既存寸法（幅12、段差6）から導いた値で、実時間の性能測定ではない。
幅を増やさないまま描画すると、範囲外のprimitiveを既存`diagram_scene_seal`が正しく拒否するため、
上限内の短い原文でも`DIAGRAM_TOO_LARGE`になっていた。

## 正典経路と変更

`diagram_sequence_layout.c`で既存`place_events`をactorの幅計算より先に行い、
そこで確定した`depth[index]`を`measure_event`にも渡す。activationの右側を
`diagram_activation_width / 2 + depth * diagram_activation_step`で予約し、描画と同じ深さ・寸法を使う。
深さを数える別処理・新しい状態owner・描画時補正は追加しない。
`place_events`はactorのx位置を参照せず、先行する`measure_actors`のheader寸法で縦配置を確定できる。

16段上限・sceneの範囲検査・失敗時の非公開と破棄は維持する。公開API、保存schema、版、依存、
メッセージ経路、style定数は変更しない。

## 試行・入力・結果

作業木は`D:/NeNeFolio/worktrees/548-sequence-activation-width`、出力は同名の`outputs/`。
各試行は`tools/record-run.py`で入力snapshot・子コマンド・生成exe・raw出力・終了値を保存した。
固定toolchainのDebug/Ninja、clang-tidyとASan/UBSan/nullabilityを使用した。
再現原文は`D:/NeNeFolio/agents/548-sequence-activation-width/reproduction.mmd`へ保存した。

| 試行 | 内容 | 実結果 |
| --- | --- | --- |
| build-red-01 | main`786d6d5`のproduction未変更、対象fixture追加 | 子/記録側0 |
| red-01 | 単独A、16activate、Note left、16deactivate | parse成功、layout outcome3、子/記録側1 |
| build-green-01 | 幅予約と順序を修正 | 子/記録側0、実行前に次のmain取込へ |
| build-green-02 | #546統合のmain`61dbfbb`を取り込んだ最終入力 | 子/記録側0 |
| green-01 | `folio_tests.exe --diagram` | 解析・配置・遅延media対象成功、子/記録側0 |

全runのrecordingErrorsは空。修正後のbuild/対象実行のstderrも空。
#546の取込ではown changesを名指しstashへ保全し、mainをffしてからapplyした。
競合なくquote試験とactivation試験を保持した。退避commitは`base-sync-stash.txt`へ記録して保持する。
ソースと試験が増えたため再buildし、修正後の対象試験は最終入力で一度だけ行った。

追加対象は1〜16段、単独/複数参加者の最後のactor、終了済み/継続中の64例とframe付き2例。
各sceneで全activationの個数と右端が外余白の内側にあることを確認し、17段は拒否する。
注記と複数参加者間のmessageを含め、既存のself activation/frame/title等の直接関連試験も確認した。
固定metricsによるcoreの契約試験で、物理実機や実フォント全体の証明とは区別する。

08:19:25の`mermaid_parser_review`による独立R/Oレビューは追加指摘なし。
親と同じモデル設定で静的読取のみ。編集/build/実行は主担当が行った。追加成果物なし。
整形・conformance・symbols・diff確認は`verify-01`へ記録し、最終headの必須CIを統合条件とする。

## 規則・限界

ARC-001/003/004/008/010/012、C-002/005/008/011/012/014/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
既存の配置ownerと深さ計算を使い、抑制・閾値・除外を追加しない。追加waiverはnone。
既存WVR-0001はWinHTTP callbackだけへ継続する。新しい図種やMermaid.jsと同じ配置は保証しない。

最終head/CI/main/通常反映/data照合/保全整理の確定原本は
`D:/NeNeFolio/agents/548-sequence-activation-width/integration-completion.json`へ収載する。
