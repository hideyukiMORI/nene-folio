# 全ノート検索を小さい全組合せと独立oracleで照合する — #632

全ノートmatcherのtail選択とprefix継続について、短い対象列と語の全組合せを、独立した素朴な全候補比較と照合した。
9,438queryの名前/本文それぞれを5種類のwork配分で確認し、全件で一致集合・総workの配分不変・上限が成立した。
今回の範囲で製品欠陥は検出しなかった。変更は既存filter_tests.cへの回帰追加と文書だけである。

## 根拠と正典経路

Issue #632。ARC-001/004/007/010/012、C-008/012/016、QLT-008/010/012/013、GIT-001〜004。
[ADR0073](../adr/0073-bounded-index-matching.md)の有限step/所有契約と
[ADR0074](../adr/0074-linear-index-matching.md)のprefix継続/比較work上限を変更しない。

既存試験には周期4条件、単文字/phraseの具体work、fallback/UTF8/span/所有/失敗境界がある。
現在ノート検索の#448には小さい全組合せoracleがあるが、全ノートmatcherは代表例を中心としていた。
同じ種類の決定的な照合を既存run_index_filter_testsへ追加し、既存verify_entry_workを再利用する。
製品matcher/公開API/owner/永続化schema/依存/規則/ゲートの変更はない。追加waiver none、既存WVR-0001継続。

## 独立した期待値と固定入力

期待値はoracle_index_containsの素朴な全候補走査で求める。候補ごとに全term byteを比較し、ASCIIのA〜Zだけをa〜zへ写す。
製品のascii_fold_byte、tail表、prefix表、検索関数を期待値計算に呼ばない。製品と同じ最適化を再実装しない。
UTF8のbyteはunsigned charとして比較する。Unicode大小変換や正規化を追加せず、非ASCIIの完全一致を守る。

| 入力群 | alphabet | 対象単位数 | 語単位数 | query数 |
| --- | --- | --- | --- | ---: |
| ASCII | a / A / b | 0〜4 | 1〜3 | 4,719 |
| UTF8 | a / A / 猫 | 0〜4 | 1〜3 | 4,719 |

対象は1+3+9+27+81=121列、語は3+9+27=39列で各4,719query。合計9,438queryを生成数assertionでも固定する。
順番は固定3進列挙で、乱数・時刻・外部入力を使わない。対象は最大12byte＋終端、語は最大9byte＋終端の固定領域を使う。
生成入力は妥当なUTF8かつ終端付き。任意byte列や任意長を網羅したとは扱わない。

各queryを、空name＋対象bodyと、対象name＋空bodyの2通りへ分ける。両方ともbodyは存在し、期待結果は0または1件。
name/bodyを結合して検索する期待値は作らない。body欠損、name/body非連結、非終端span、非ASCII大小/正規化の既存具体試験も維持する。

## workと完了の確認

既存verify_entry_workを使い、予算1、2、不均等1/3/2/7、4096、65536の5配分で同じqueryをdrainする。
9,438query×2source×5配分＝94,380回のowner生成と照合である。

- 最初のwork0でMORE/NOT_READYを保つ。
- 正workでは残量が減り、MOREとNOT_READYが一致する。既存の有限反復上限を維持する。
- 完了時はALL_DONE/READY、期待件数と正確なref(category3/note7)を確認する。
- 5配分で消費総workが一致する。一般語の2+4L+3N上限を、単文字にも緩い上限として使用する。
  body経路はL0、name経路はN0。ここでの長さはUTF8 byte数である。

代表入力の既存結果も維持された。周期N4096/m17=8180、N16384/m17=32756、N4096/m257=7940、
N16384/m257=32516。単文字65538、末尾phrase5057（既存上限5058）だった。
これらは比較workであり、実時間・CPUサイクル・UI待ち時間を測った値ではない。

## 実行と原本

基点main1dc20301のD専用test/632-index-matcher-oracleを作り、製品/文書に触れる前にfilter_tests.cだけを編集した。
#630統合後のmain23397d7をffしてから対象buildを実行した。

| run / コマンド | 結果 |
| --- | --- |
| configure-debug-01 / configure-debug.ps1 | 正典Debug設定、子/記録0 |
| build-debug-01 / build-debug.ps1 | 変更testのformat、Debug/tidy/警告error、119actions成功 |
| index-matching-01 / folio_tests.exe --bounded-index-matching | 9,438queryと既存core/application境界が成功、子/記録0、stderr空 |

ASan/UBSan/nullabilityとrecover禁止を維持し、全runの入力終了時不変・記録error0を確認した。
selectorには既存準備/lookup/cache-retry/容量境界が含まれる。表示されたallocation probe inactiveとmaximum-fit checks0は、
正典Debugで確保注入していない意味であり、OOM試験の成功件数とは扱わない。製品/確保経路は変更していない。
フルゲートは最終head必須CIのeng/check.ps1へ集約し、成功済み対象を工程の違いだけで再実行しない。
軽微なtest追加は主担当が自己レビューし、新しい独立委任を行わなかった。

調査ではrgへ直接渡したfilter globがPowerShell環境でOS123になり、実file一覧からfilter_tests.cを指定して訂正した。
製品や試験の失敗ではない。全試行はD:/NeNeFolio/agents/quality-20261105/journal.mdにも記録する。
恒久原本D:/NeNeFolio/agents/632-index-matcher-oracle/に実argv/source/exe/config/stdout/stderr/終了値をrecord-run.pyで保持する。

## 限界と完了経路

固定2alphabet・有限長の検証であり、任意長に対する形式証明、任意Unicode、実UIの応答性やディスク障害の保証ではない。
実ノート・通常app・Storeを試験に使わない。製品変更がないため通常exeの再buildは行わない。
文書検査/最終head必須CI/統合/#630後の通常exe/data不変/全保全と専用worktree/output整理の確定は
同taskのintegration-completion.jsonへ。branch/commitは保持する。
