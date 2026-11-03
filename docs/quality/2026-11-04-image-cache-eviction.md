# 画像cacheの満杯時退去 — #544

2026-11-04、hideの継続的な品質改善依頼。既存の`image_cache`が小さい5件目を登録すると、
次の挿入位置を退去のたびに動かして既存4件すべてを消していた。画像/図の再表示で不要な再生成が生じる。

## 正典経路と変更

`src/adapters/win32/image_cache.c`の同じowner内で挿入先を固定し、まずその1件を退去する。
32MiBに収まらない場合だけ、別cursorで登録順に追加退去する。容量4件・32MiB、key/encoded bytesの照合、
bitmap/要求の複写所有、workerの取消・寿命は変えない。byte不足でできた空き枠も次の登録で再利用する。

`tests/win32/image_cache_tests.c`を既存adaptersへ結ぶ`win32_tests`の対象としてCMakeに登録した。
実通信・window・実ノートを使わず、合成bitmapと画像/図の交互キーで同じcache APIを確認する。
新しい公開API、保存schema、版、依存、方針変更はない。

## 試行と結果

作業木は`D:/NeNeFolio/worktrees/544-image-cache-eviction`、出力は同名の`outputs/`。
各試行は`tools/record-run.py`で入力snapshot、子コマンド、raw出力、終了値、生成exeを記録した。
buildは固定toolchainのDebug/Ninja、clang-tidyとASan/UBSan/nullability付き。実時間の速度比較ではない。

| 試行 | 内容 | 実結果 |
| --- | --- | --- |
| build-red-01 | production未変更の`ab7b732`、追加cache対象だけbuild | 子/記録側0 |
| red-01 | 要求列0,1,2,3,4,1,2,3,4 | hit1/miss8で期待hit4/miss5を満たさず、子/記録側1 |
| build-green-01 | #543統合の`932bc18`へff後、退去修正を含む同じ対象 | 子/記録側0 |
| green-01 | 同じ要求列と容量・byte・所有境界 | hit4/miss5、273checks、子/記録側0 |

上記runの`recordingErrors`は空。greenのstderrも空。
9要求でcache missが8から5へ減った事実であり、ネットワークや描画時間の倍率は測っていない。
#543はparserだけの変更でcache入力に影響せず、その成功試験を再実行していない。

対象の境界は、小さい5/6件目で最古1件だけを退去、byte不足で必要な複数退去、空き枠再利用、
encoded/keyも含めた32MiBちょうど、単品過大の不採用、元encoded bytes変更/長さ変更のmiss、
呼出し元・cache破棄後も取得済みbitmapが生存すること。
完了前のbuild result参照は未生成で外側の読み取りを中断した。子buildの失敗としては扱わず、
完了情報を確認してからgreenを実行した。

07:40:15の`media_lifecycle_review`による独立R/Oレビューは追加指摘なし。
親と同じモデル設定で静的読取だけを担当し、製品編集・build・実行検証は主担当が実施した。
`verify-01`は整形/conformance（違反0）が成功し、diff確認が引き継ぎ末尾の余分な空行を拒否した。
空行と文書内のtarget表記を直し、規約検査の再確認を`verify-02`に保存する。
成功したcache対象試験は再利用し、最終headの必須CIを統合条件とする。

## 規則と残る限界

ARC-001/003/004/008/012、C-004/005/008/012/013/016、QLT-008/010/012/013、GIT-001〜004、
ADR0085/0089。既存のcacheだけを修正し、所有者・依存方向・終了契約を維持した。
抑制・閾値・除外の追加はなく、追加waiverはnone。既存WVR-0001はWinHTTP callbackのみで継続する。
登録順の有界cacheでありLRUへの変更ではない。実通信速度・物理実機受理の証明は含まない。

最終head/CI/main/通常反映/data照合/恒久保全/整理の確定原本は
`D:/NeNeFolio/agents/544-image-cache-eviction/integration-completion.json`へ収載する。
