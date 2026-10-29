# 現在ノート検索の比較量 — #448

2026-10-30。対象はcore `note_search`。全ノート索引#446とは別の経路である。
単一Two-Way走査の完成候補で対象build・検索unit・固定比較診断が成功した。
最終headの必須CI・統合・通常exe反映・保全整理は別の完了条件として追跡する。

## 契約と採用判断

公開next前後とcountが同じ昇順一致列を消費し、UTF-16整形式、ASCII限定fold、明示長内NUL/CR、重複、巡回、ordinal、
拒否順序と失敗時出力を維持する。語を借用する呼出し内の固定数scalarだけを持ち、確保・再帰・長さ比例table・旧matcherのfallbackはない。
算術と所有の決定は[ADR0023の補足](../adr/0023-in-note-search.md#2026-10-30-の実装補足448)。

根拠はCrochemore/Perrin, *Two-Way String-Matching*, Journal of the ACM 38(3), 1991, pp.651–675。
保存PDFのSHA256は`D0EB61AE8F875C43B5EB85999581201785AED0E1A09D8411626CB1171E9D967D`。
原論文の図17/19/21、算術補足、独立静読を照合した。製品コードは論文コードの複写ではない。
`2N+5m`は前処理込み文字対比較の診断oracleとし、実時間の保証に読み替えない。

## 変更前baseline

本文長N={4096,16384}、語長m={17,255}について、無一致・末尾一致・大量重複のnext前後とcountを36call実行した。
小fixture `Ab`/`aB` の3callは各4fold、固定36callは全て結果/範囲/件数/旧式と一致し、counterは正の偶数だった。
全走査24callが固定上限を超過し、早期返却12callは語長m比較だった。

| N | m | 旧全走査比較 `(N-m+1)m` | 固定上限 `2N+5m` | 旧早期返却比較 |
| ---: | ---: | ---: | ---: | ---: |
| 4096 | 17 | 69360 | 8277 | 17 |
| 4096 | 255 | 979710 | 9467 | 255 |
| 16384 | 17 | 278256 | 32853 | 17 |
| 16384 | 255 | 4113150 | 34043 | 255 |

全走査は無一致F/B/count、末尾一致F/count、大量重複count。末尾一致Bと大量重複F/Bに全走査式を要求しない。
旧diagnosticの成功は旧挙動と上限超過の確認で、新実装の回帰試験成功ではない。

正典Debug configureの実compile/link vectorから4TU限定診断を派生した。ASan/UBSan/nullability、recover禁止、/MT、
4つのsanitizer runtimeを保持し、note_search TUだけのfold名差替えと実objectのU/T参照で転送先を確認した。
製品に計数口を追加していない。親の監査はraw39行・41snapshot hash・4TU条件を照合してexit0、担当保全監査は14record/errors0。
22凍結fileと848fileのhash一覧を保存した。

旧raw/exe/sourceを保全したうえで、新harnessにold-formula/linear-boundの明示modeだけを分けた。
固定入力生成・fold wrapper・公開結果oracle・上限を保持する。前後で同一harness bytesを実行したとは呼ばず、
旧raw24件の整数上限違反を負側の根拠として再利用し、旧sourceを再build/runしない。

## 完成候補の結果

同じ固定36callは公開結果/範囲/件数が旧rawと一致し、すべて固定上限内だった。小fixtureの3callは前処理込みで各4比較/8fold、上限14以内。

| N | m | 候補の最小比較 | 候補の最大比較 | 固定上限 |
| ---: | ---: | ---: | ---: | ---: |
| 4096 | 17 | 49 | 4128 | 8277 |
| 4096 | 255 | 763 | 4604 | 9467 |
| 16384 | 17 | 49 | 16416 | 32853 |
| 16384 | 255 | 763 | 16892 | 34043 |

各行は3pattern×3operationの9call。無一致は4112/4350/16400/16638比較、大量重複forwardは49/763/49/763比較。
末尾一致の全operationと大量重複backward/countが表の最大値となる。旧早期返却のm比較より増えたcaseもあり、全caseの実時間非退行とは呼ばない。

候補側の計装はnote_searchと新mode harnessの2TUだけを新compileし、hash不変の元ascii_fold/probe objectを再利用した。
正典の完全sanitizer vector、4runtime、note_search→probe→元foldの配線を維持した。
親の`parent-candidate-audit.py`は11record・3252入力/成果物snapshot hash、最終4候補hash、raw39行、旧負側24件と候補上限0件を確認してexit0。
親による製品test/probeの再実行は0。記録側もexit0/errors0。

## 対象検証と修正過程

- `cmake --build D:/NeNeFolio/outputs/448-linear-note-search/debug --target folio_tests -j 2`: R2 Debug/tidy成功。
- 同debugの`folio_tests.exe --note-search`: exit0、`note search tests passed`。既存検索unitと4719短queryの独立naive oracle、
  奇数長周期/fold周期/非周期/memory破棄/257単位語と768重複/NUL/CR/surrogate/拒否順序/実SIZE_MAX anchor境界を確認。
- `folio_tests.exe --note-search-unknown`: 期待exit2、test出力なし、未知selector診断を確認。
- `candidate-bin-01/note-search-candidate.exe --linear-bound`: exit0、3fixture/36固定call、結果一致・比較上限違反0。
  同exeの`--unknown`は期待exit2、query出力なし。正の受理結果と期待拒否2を区別する。
- `python -B eng/symbols.py --build-dir D:/NeNeFolio/outputs/448-linear-note-search/debug --require core application`: exit0、2library/違反0。

最初の独立静読R1は内部struct名のCNF-002違反。`struct note_search`への改名で修正した。
初回Debug/tidyはsuffixのnested depth4を拒否し、同値guard/continueへ平坦化した。次のbuildはunit main64行を拒否し、
既存full-runの41関数を同じ順・各一回のprivate helperへ抽出した。coverage-negativeのtextのみと専用selectorを維持した。
両失敗はchild1、未生成artifactによるwrapper125として保存し、期待失敗に読み替えていない。
独立R1/R2追補は旧snapshotとの差分・最終hashを照合し、新重大/要修正/任意0。原レビューのネスト見落としも追補で訂正した。
閾値・除外・抑制・waiverは変更していない。

担当の自動pipelineは親の文書固定待ち連絡と前後して規約/File API/diffまで進みexit0だったが、docs/fonts等の入力snapshotが不足していた。
この規約結果を正式な完成証拠へ採用せず、親が最終文書と全inventory・File APIを固定した別recordを残す。
製品test/probeの入力は不変なので再実行しない。無関係な全unit、UI/native、性能計時、新OOM sweep、ローカルfull gateは追加しない。
統合には最終PR headの必須CI `pwsh -NoProfile -File ./eng/check.ps1` の成功を要求する。

原本は`D:/NeNeFolio/agents/448-linear-note-search/`の`phase-a-completion.md`、`parent-baseline-acceptance.json`、
`phase-a-evidence-audit.json`、`phase-b-count-results.json`、`parent-candidate-acceptance.json`、
`independent-candidate-review.md`とR1/R2追補・対応hash。
各runの入力snapshot・raw・終了値は`D:/NeNeFolio/outputs/448-linear-note-search/`にある。
v2抽出helperの外側例外は専用rawファイルがなく、会話出力と静読記録だけが残る制約を明記した。
誤った`--vs-link-exe`判定を実際の`-E vs_link_exe`へ訂正した補助処理の失敗で、製品の期待失敗や成功に数えない。
最終文書/規約受理は`parent-final-acceptance.json`、CI・統合・通常反映・全hash保全・安全整理の確定値は
同taskの`integration-completion.json`へ収載する。未取得のCIや通常反映の成功をこの対象検証から推定しない。

## 保証範囲

比較回数は文字対の照合だけを測り、UTF-16検証・整数分岐・CPU命令数・経過時間を含まない。
固定標本の実測を任意入力の形式証明、実時間非退行、UI応答改善の実測へ広げない。後方近傍の定数費用は変わり得る。
通常data/save、公開beta、他案件の操作はしていない。Issue #448、ARC-001/003/004/005/007/010/011/012、
C-002/003/005/008/011/012/014/016、QLT-008/010/012/013。公開API/schema変更なし、Waivers none。
