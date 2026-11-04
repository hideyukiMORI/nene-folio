# 受付時の拒否と先行画像の保持 — #581

#578では拒否後の次要求を確認したが、正常要求を先に受け付けた状態での同期拒否は未確認だった。
`image_job_diagram`は書体名と原文長を確認してからjobを作り、`image_adapter`はその成功時だけsubmit/cancelを呼ぶ。
今回は同じworkerへ先行する正常要求を出し、受付時の拒否がそのticketと結果を保つかを合成入力で確認した。

## 入力と観測境界

空の書体名、64byteの書体名、262145byteの原文の3種類を用いた。前2種類の期待値はIMAGE_UNSUPPORTED、
原文上限超過はIMAGE_TOO_LARGEであり、いずれも非同期受付前の拒否となる。
正常入力は既存native試験のflow/sequenceを明暗で使う。各正常原文の末尾へ条件番号の独立コメントを付け、cache keyを分離した。
元原文の直接描画4枚を画像基準とする。

- 描画待ち: 正常要求の受付直後にpollし、IMAGE_PENDINGを観測してから無効要求を出す。
- 結果未受取: 正常要求の受付後、phaseだけをIDLEまで待ち、pollせず無効要求を出す。

3拒否×2図種×明暗×2状態×2回の48条件。それぞれで拒否に渡したticket変数のUINT64_MAXが不変、
先行ticketがIMAGE_READYで基準と全bytes一致、受取後の再pollがIMAGE_CANCELLED/bitmap非公開であることを確認する。
保存したbitmapもSHA256で基準と比較する。受取前のphase IDLEは、正常要求のsubmitがPARSINGを設定し、
workerがpublish時にIDLEへ戻す既存実装と組み合わせて判定する。

描画待ちの直前pollから拒否呼出しまでworkerを停止しないため、拒否の瞬間も必ず処理中とは保証しない。
任意の競合順序、複数呼出thread、確保失敗、実UI通知をこの試験の保証へ含めない。

## 実行原本

main7230e884098209a2413f17b19772eac07de164b8と#577成功head63853f10c0c832ed6e2781d7f2b22b47a69ccec1の
src/tests/eng/fonts/CMake入力一致を確認し、成功したDebugのcore/application/adapters 3libとfont7fileをhash照合して再利用した。
外部harnessは既存native helperをincludeして同じport/worker/rendererを呼ぶ。製品用の入口は追加しない。
原本はD:/NeNeFolio/agents/diagram-admission-20261104、実行出力は同名outputs。tools/record-run.pyで入力snapshot、raw、終了値を保持する。

初回link-01は子/記録側0。check-01はnative子0で48条件/52画像を出したが、ASanの
`interception_win: unhandled instruction`がstderrへ1行出たため評価は子/記録側1となった。記録エラーは空。
24件の直前PENDING、24件の未受取IDLE、48件のticket変数不変と基準画像一致は観測値として残す。
試験全体を成功へ書き換えず、診断も抑制しない。

LLVMの[interception実装](https://llvm.googlesource.com/llvm-project/+/refs/heads/main/compiler-rt/lib/interception/interception_win.cpp)には
interceptorの命令長を解釈できない場合の同名診断がある。ただしこれは一般的な診断位置の確認で、当該exeの発生関数は未同定。
正規CMakeが使うlld-linkと明示runtime/wholearchiveに外部linkを合わせ、同じsource/期待値を別exeで確認する。
初回exe/raw/52画像を保持する。製品compiler版、ASan設定、規約gateは変更しない。

## 判定と限界

link-02/check-02は子/記録側0、記録エラー/stderr空。check-02のnative子も0で、48条件すべてが成立した。
PENDING直前観測24件、未受取IDLE24件、拒否側ticket変数不変48件、先行画像の基準一致48件、二重受取0件。
check-01/02の各52画像は対応するSHA256が全一致し、全104ファイルを再hash照合した。成功時の基準2画像も視認した。
リンク条件の複数差を同時に正規条件へ揃えたため、どの単一flagが診断を解消したかは切り分けていない。
初回の診断を製品のメモリ破壊と断定せず、再確認の成功も全heapの健全性保証へ拡張しない。
製品不具合は再現せず、製品/test/schema/版/依存/ADR/gateの変更は行っていない。
ARC-001/003/010/012、QLT-008/010/012、GIT-001〜004、ADR0089、開発ワークフロー§8に従う。
追加waiverはnone、既存WVR-0001はWinHTTP callbackの範囲で継続する。
実app/実ノート/Storeを操作せず、性能の比較や全heapの健全性を主張しない。


| run | コマンド・結果 |
| --- | --- |
| link-01 | record-run → link.ps1。初回の外部Debug link、子/記録側0 |
| check-01 | record-run → `python check.py` → admission.exe。native0、ASan診断1行を理由に評価/記録側1 |
| link-02 | record-run → link-02.ps1。lld-link/正規runtime条件へ訂正、子/記録側0 |
| check-02 | record-run → `python check.py 02` → admission-02.exe。48条件成功、子/評価/記録側0 |

原文・期待値・製品libは2回で同じ。2回目は別exe/bitmapdirectory/summaryを指定するため評価scriptへ引数を加えただけで、
判定式は変えていない。初回script bytesはrecord-runの入力snapshotへ保存している。
全runのrecordingErrorsは空。出力file名の推測による読取失敗、Windowsのrg glob指定誤りは試験成功に算入しない。
品質記録/gate-proofs/日報/引き継ぎ/currentの5文書を更新し、文書リンク/範囲/差分を確認する。
最終head必須CI、正典統合、通常exeのhash照合、全証跡保全・追加作業木/output整理の確定値は
`D:/NeNeFolio/agents/diagram-admission-20261104/integration-completion.json`へ収載する。
