# 置換factoryの終端付き容量 — 2026-10-29

Issue: [#411](https://github.com/hideyukiMORI/nene-folio/issues/411)。基点main
`f6d7f889c569a10147d6af763fccd0cc0088af6e`。
規則: ARC-001/003/004/008/010/012、C-004/005/007/008/011/012/016、
QLT-001/007/008/009/012、GIT-001〜004。Waivers: none。

## 拒否不足と修正

旧`replace_edit_create`へ本文bufferを要しない`capacity=SIZE_MAX`だけを渡すと、
`capacity + 1`がwrapしてREADY（値0）、nonnull objectを返した。拒否期待の診断はexit 1。
appendや巨大fitの確保を行わず、返ったobjectだけ解放した。
これはcore APIの宣言容量境界であり、通常UI操作から自然到達したという結果ではない。
置換出力400万単位、置換欄255単位等の上流制約は変更しない。

既存3factoryの入口で、`n >= SIZE_MAX / sizeof(element)`を加算前に拒否する。
editはUTF-16容量、templateはliteralとprivate piece列の両方、previewは本文UTF-16長を検査する。
edit/templateはowner確保前、preview本文はowner確保・置換文法解析より前に既存OOMを返す。
replacement長の拒否はtemplateの同じguardと既存`from_template`によるOOM写像を使う。

nullable local、全memberの準備後commit、部分生成物の構築用cleanup、最後のmatches引取りは
[#407](2026-10-29-nullable-allocation.md)から不変。拒否時outとcaller matchesを保持する。
新しいAPI/enum/共通算術helper、private layout公開、依存・上限・schema・ゲート変更はない。
[#311](2026-10-17-size-overflow-checks.md)の表現可能性と製品上限を区別する方針を再利用する。

## 対象検証

既存`size_overflow_tests`とpreview所有fixtureを共用する。実在するnonnull out sentinel、
heap matchesのpointer/capacity/count/内容、同じmatchesによる通常入力retryと成功時移転を確認した。
SIZE_MAX、char16_tの最初のoverflow、template pieceも確実に表現不能な個数を拒否する。
piecesの検証値は`SIZE_MAX / sizeof(size_t)`とし、不透明なprivate structを複製しない。
空入力・小容量・終端・template文法・通常置換は同じselectorの既存回帰を使った。
非表現な本文長は、不正な置換文法より先にOOMとなる直接境界も確認した。

最大fitはeditとpreview本文で各1件、probe有効時に最初の小さなowner確保を故意に拒否し、
guardの許可だけを観測した。巨大配列の実確保や、小さいbacking bufferの参照へ進めていない。
exact piece最大fitはprivate layoutを公開せず、通常countと確実な拒否値・実装式で照合する。

| 対象 | 実行と結果 |
| --- | --- |
| 固定toolchain Debug configure | `cmake -S D:/NeNeFolio/worktrees/411-replacement-size -B D:/NeNeFolio/outputs/411-replacement-size/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、exit 0 |
| 対象build/tidy | `cmake --build D:/NeNeFolio/outputs/411-replacement-size/build --target folio_tests -j 4`、exit 0 |
| canonical対象 | `folio_tests.exe --replacement-size`、exit 0。probe inactive、最大fit 0件。OOM注入成功には数えない |
| 正典測定build | 既存`coverage.sanitizer_vector`/`build_measurement`を使用。Debug全対象recordと同じASan/UBSan/nullability・recover禁止vectorでcompile/link成功 |
| binding proof | 同vectorの正例exit 0、反例exit 1と該当`_Nonnull binding to null pointer`診断を確認 |
| 計装対象 | `folio_coverage.exe --replacement-size-allocation`、exit 0。既存replace scenarioとpreview 7失敗点から成功まで、最大fit計2件を確認 |

format/diff/conformanceはいずれもexit 0。最終値は
`D:/NeNeFolio/agents/411-replacement-size/static-result.json`へ保存した。
実コマンド・入力hash・raw log・最終manifestも同directory、build/runtimeは
`D:/NeNeFolio/outputs/411-replacement-size/`へ保持する。

## 初回失敗と残る範囲

最初のconfigureは診断PowerShellの`$output`がtoolchain内部の同名変数に上書きされ、
build pathがPython版文字列になったため失敗した。`configure-attempt1.log`に原本を保持し、
task固有名へ修正後configure成功。製品コードの失敗やゲート緩和ではない。
canonical build/tidy、対象unit、計装対象の初回実行は成功した。
初回conformanceは対象buildのCMake File API reply未生成で1違反だった。
正典checkと同じcodemodel-v2 queryを置いて同じDebug設定を再configureし、検査入力を生成して0違反を確認した。
原本は`conformance-attempt1.log`/`static-attempt1-result.json`に保持する。

red/canonical/計装stderrには既存ASan runtimeの`interception_win: unhandled instruction`起動警告がある。
期待するbinding反例以外のUBSan/nullability診断は出ていないが、全interceptorの保証とはしない。
カテゴリ/ノート名strlen経路、全core確保監査、無関係UI/検索、#410の表修正は今回対象外。
local full gate・全体coverage閾値判定は実行しない。最終head必須CI、commit/PR/統合/通常反映と
保全後の安全監査・整理は親担当。通常data/Nibと削除拒否済み23箇所は操作しない。
