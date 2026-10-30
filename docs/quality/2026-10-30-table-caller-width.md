# 既存caller幅上端の63列表診断 — 停止時点

Issue #466 / 終了文書 #467。記録日2026-10-30。
規則: ARC-001/004/008/011/012、C-008/012/013/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0043/0044/0072。Waivers none。

## 根拠と設計

#462はroot63/indent0/100%・要求90720twipで最初の元GET(cp118)がS_OK、元SETがE_INVALIDARGとなった。
現在のnote_pane.cのcaller幅は31679twipでcapされるが、root63にこの入力域上端を渡した結果は未確認である。
31679はnative上限や最小可読幅ではない。320px/100%の実callerがその値を生成する再現でもなく、現在製品の不具合とは扱わない。

調査は17入力の開始/終了hash一致、終了時追加2入力を区別し、報告等25identityを親照合した。
初読19:52:55まで不変を遡らず、hash checkpoint19:56:20〜20:01:39の範囲を記録した。
原本は`D:/NeNeFolio/agents/quality-post464-table-width/`の`minimal-width-diagnostic-plan-v1.md`、
`input-hashes.json`、`parent-research-acceptance-v1.json`、`parent-primary-api-review-v1.json`。

診断案は同じroot63/indent0/100%/client320×600/120DPIの非製品fixtureから、元note_pane_table_layoutへ31679を一回だけ渡す。
両行cp118/374・63列のoracleは先頭53列503twip＋残り10列502twip、総和31679、indent0。
元table TU3432bytesの保持・一回forward・LRESULT/LastError透過・32record・安全なGET意味条件・TOM終了後dumpを維持する。
READYなら全列と本文/選択/dirty/非空UndoRedo flags/style/DPI/通常段落wrapを先に照合し、その後だけ既存seed Y/Xへ実Redo→Undoを確認する。
FAILEDなら最初の実失敗枝とnative3を保持する。0/2/3・記録failure・外shellを区別し、診断完了と幅成立を分離する。
連続幅探索・旧成功root3/旧90720拒否の再実行・製品最小幅/水平方式/別renderer採用は対象外。

## 停止した範囲

20:14 JSTにhideが区切りでの停止と日報・引き継ぎを依頼した。
SOL担当はIssue/旧helperの読取と設計整理までで停止した。担当作成helper/fixture/manifest0、parse/format0。
Configure/Build/Inspect/Compile/Probe/Auditはすべて0。幅成立・native結果・新たな性能値は得ていない。
製品/test/config/API/schema/版変更0、通常exe起動/data操作0、所有session/process0。

#466はopen、branch `test/466-table-caller-width`、base `e4e182313042f1cb533aef829631918dfbcf64ac`。
`D:/NeNeFolio/worktrees/466-table-caller-width`はclean、`D:/NeNeFolio/outputs/466-table-caller-width`は空。
未完taskの再開先として保持し、削除拒否51箇所とは分ける。削除要求は出していない。
原本は`D:/NeNeFolio/agents/466-table-caller-width/`のIssue本文・設計・停止checkpoint。

再開時はhideの新指示、現main/Issue/PR、対象sourceと入力hashを照合する。静的helper/fixture/監査/driverと完全patch・manifestを作成し、
自己レビューと親受理を済ませるところから再開する。今回の文書は未実行stageへの自動再開許可ではない。
#467でこの停止記録・日報・引き継ぎ・current・#464完了補記を収載する。新たな製品診断や通常exeの再buildは行わない。
