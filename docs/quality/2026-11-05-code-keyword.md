# 通常コードの予約語照合 — 2026-11-05 / #663

## 結論

**候補1は不採用。製品・テスト・build設定は元と同じ。**
正常な語/コードの30条件で、全70alias、語照合、全RTF bytesは一致した。
ただしABBA各版10sampleの中央値で、非plainの予約語batchは約8〜32%増、着色も全20条件で増加した。
変えていないplain着色にも約12〜15%の差があるため、差を全てlist処理に帰属させず、CPU/外部負荷/配置等の原因も断定しない。
この条件で採用を支える改善が無かったため、凍結した元sourceへ完全復帰した。UI全体の速度や他環境の一般比率は主張しない。

## 比較対象と正典

[ADR0086](../adr/0086-fenced-code-syntax.md)のcode_languageが単一表でalias/予約語/字句規則を所有する。
基準は空白区切りlistを順に走査して各語の長さを数え、同じ長さだけsame_wordで比較する。
候補1は同じ表の語区切りをNULへ変え、末尾に空語sentinelを置き、既存許可symbolのstrlenで各語長を得る。
語集合・順序・fold規則・same_word・公開API・allocation/所有者・lexerは変えない。新依存/索引/cache/並行処理/ゲート緩和なし。
候補の全表・末尾sentinel・MAP実装所属を監査し、凍結sourceとexeを原本に残した。

mainbc00acdで正典Release coreをbuildし、#661のmain2eb20d8へffした。
src/eng/cmake不変、CMakeはnativeCTestの2行追加だけと確認し、Release core/source/compile vectorを再利用した。
固定clang-cl /O2 /Ob2 /DNDEBUG -MT C23 /WXの実compile_commandsを使用。明示code_language objectを先にlinkし、同じ凍結core archiveを使う。
最終のrepo差分は文書だけ。Debug/既存unit/OOM/nativeを変更のない製品へ追加反復しない。

## 入力・計時・正しさ

- 10言語の全予約語と大小variant、普通のASCII/日本語識別子5種。元の表から得た語に対しPython setで期待membershipを作る。
  SQL/PowerShellだけcase foldする既存契約を比較する。全aliasの大小variantは70件、各版で全結果を照合する。
- 各言語の短い通常snippetを16/1024行。日本語commentとplainの絵文字を含み、JSONも正常textだけ。
  手書きの期待RTF fragmentを反復した全bytesと比較する。入力コードは実行しない。
- UTF8 fixtureは51file、計30条件。warmup1、runごと5samples。基準profile150sample、ABBA比較600sample。
  ABBAはcase順も交互に反転。各版/条件は10sample、全rawから中央値を再計算した。
- keyword計時は公開照合のbatch反復と結果合計。各語の結果照合は計時前後。
  syntax計時は公開create/append/finish/destroyと長さ確認を含み、file読取/行分割/全文比較は計時外。
  全出力bytesは計時前後に確認する。QPCとQueryThreadCycleTime、thread affinity mask4、優先度変更なし。
  計測中に親のbuild/archive/hashを重ねていない。外部セッション等の完全idleは保証しない。

## 実測中央値

予約語は表記された全queryを1巡するbatchのµs、着色は1,024行の生成/破棄のms。
両版は同じ正常入力・正典vector・coreを使用。値はこのrunの観測であり一般性能ではない。

| 言語 | query/batch | 語 基準 µs | 語 候補 µs | 着色 基準 ms | 着色 候補 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| plain | 11 | 0.0290 | 0.0285 | 0.03125 | 0.03585 |
| C | 127 | 16.4705 | 17.8573 | 0.46125 | 0.49140 |
| C++ | 197 | 42.5750 | 50.2268 | 0.69350 | 0.72950 |
| JavaScript | 99 | 9.5190 | 11.8458 | 0.38790 | 0.42550 |
| TypeScript | 159 | 26.0910 | 30.7675 | 0.75800 | 0.84050 |
| Python | 88 | 6.7712 | 8.6907 | 0.24425 | 0.28310 |
| JSON | 17 | 0.1673 | 0.1988 | 0.32855 | 0.34475 |
| Shell | 69 | 4.2248 | 5.5668 | 0.21070 | 0.23655 |
| PowerShell | 91 | 7.6245 | 8.7265 | 0.27510 | 0.30400 |
| SQL | 167 | 23.7205 | 28.2980 | 1.08295 | 1.22130 |

16行条件とcyclesを含む全rawはmeasure-profile.json/measure-candidate01.jsonに保存。
通常の小さい語ではstrlen呼出し等の費用が相殺する可能性はあるが、assembly/CPU要因を分離していないため原因認定はしない。

## 実行・保全

configure-release-01、build-core-01(83actions)、fixtures-01、reference-01、compile-baseline-01、profile-01、
compile-candidate01-01、compare01-01、audit-01が全て子/記録0・error0、入力終了時不変。測定stderr空。
audit-01は全filehash/UTF8、全表の語集合と順序/sentinel、70alias、raw750sample/中央値、MAP4symbol/2実装所属、
固定Release flags/同core、元source復帰と製品/test/build差分0を照合した。候補を成功として採用扱いしない。
補助readの旧driver名推測は不存在で実一覧へ訂正し、製品試験と分離して全体journalへ残した。
7docsの参照/diff/source/build graph、最終docSHAを確認し、最終head必須CI eng/check.ps1→tools/merge-pr.ps1で文書統合する。

通常exeは#651反映後のまま再build/起動せず、data32fileとともに不変確認。実app/実ノート/Storeなし、#634保留。
親単体・追加委任なし。規則ARC-001/003/004/008/012、C-008/009/012/014、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。schema/版/依存不変。
原本D:/NeNeFolio/agents/663-keyword-scan/。最終CI/統合/通常不変/全保全/専用整理は同integration-completion.jsonへ。
全保全/hashと取込tree/dirty/ignored/link/稼働参照監査後、当該D専用worktree/outputだけ整理しbranch/commitは保持する。

## 最終受理・保全整理（16:56）

PR664 head20973e7acf973a5c3d5990294d307720e1be7f23、必須CI41989646963 full-gate/check成功、main6301780へ統合済み。
Issue663 CLOSED、通常exe/data32file不変。1364file/29028912bytes全保全ZIP/hashと取込tree/clean/ignored/link/稼働参照を監査し、専用領域を整理。
不採用候補source/exe/rawを保全しbranch/commitも保持。D:/NeNeFolio/agents/663-keyword-scan/integration-completion.json。
