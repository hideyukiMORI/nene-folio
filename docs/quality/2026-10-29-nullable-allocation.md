# 確保失敗とnonnull binding — 2026-10-29

Issue: [#407](https://github.com/hideyukiMORI/nene-folio/issues/407)。基点main
`5c1240edf06842681e5ae773ab202c8f23dadd6e`。
規則: ARC-001/003/004/008/010/012、C-004/005/007/008/011/012/016、
QLT-001/007/008/009/010/012、GIT-001〜004、ADR0004/0044。Waivers: none。

## 不具合と契約回復

#404の測定で、`text_buffer_create`がmallocの失敗をnonnull memberへ代入した時点で
`runtime error: _Nonnull binding to null pointer`となり、その後のnull検査へ到達しなかった。
既存の失敗注入はASanだけで、DebugのUBSan/nullabilityとは分かれていた。
このredは`D:/NeNeFolio/agents/404-markdown-containers/initial-extra-sanitizer-*`を再利用し、再実行しない。

同型の7factory（text_buffer/json_reader/json_writer/note_text/replace_edit/replace_template/replace_preview）を、
nullable localへ受ける→成功を検査→nonnull memberへcommitする順に揃える。
構築前の配列・template・previewはlocalとして所有し、失敗時は同moduleの構築用destroyで片付ける。
公開destroyは全memberの確定後だけに使う。previewのmatches引取りは全準備成功後の既存adopt_matchesだけで行う。
公開factory型・typed outcome・out不変・原文/BOM/終端・matches所有契約は変更しない。

## 検出経路と対象回帰

`eng/coverage.py`はcanonical Debugの`compile_commands.json`から、全対象core/application/testの
sanitizer vectorを一度抽出する。ASan/UBSan/nullability・recover禁止の欠落、不一致、無効化指定を拒否し、
同じvectorを測定compile/linkとbinding検出proofへ渡す。Python独自のASan-only既定値は持たない。
proofは正例成功と、反例の該当binding診断・非zero終了の両方を要求する。proofのobjectは測定objectへ混ぜない。
source inventory、/MT、失敗probe、分岐90%下限と負例profile判定は維持する。

対象selector`--nullable-allocation`は、既存reader/writer/plain_rtf/replace scenarioを共通entryから使い、
全体allocation runもその同じentryを呼ぶ。BOMとtemplate文法も既存testを再利用する。
既存scenarioのnonnull out保持を補い、text_bufferの直接out保持とpreviewのheap matches所有回帰を加える。
previewは各OOM後に同じmatchesで再試行し、BAD_TEMPLATE時のout/matches不変とREADY時の移転・destroyを確認する。
stack matchesを所有移転へ流用しない。probe inactiveは対象selectorの成功に数えない。

## 証跡と適用範囲

実コマンド・入力hash・初回失敗・最終結果は`D:/NeNeFolio/agents/407-nullable-allocation/`へ集約する。
作業木は`D:/NeNeFolio/worktrees/407-nullable-allocation`、build/runtimeは
`D:/NeNeFolio/outputs/407-nullable-allocation/`。

## 完成候補の結果と初回失敗

初回canonical Debug buildは、追加preview fixtureのcalloc後に`require`を通したnonnull検査を
clang-analyzerが推論できず、fixture内のNullDereferenceでexit 1だった。factoryの新たな診断ではない。
logと変更source全体は`build-attempt1.log`、`attempt1-sources/`、`attempt1-source-hashes.json`へ保全した。
test runnerの既存失敗処理を`[[noreturn]] test_failure`へ集約し、fixtureは明示したNULL枝からそこへ終了する。
`require`も同じ失敗処理を使う。抑制・severity・annotationの緩和は行っていない。

修正後の実行は次のとおり。`run-target-attempt2.ps1`と`run-selector-attempt2.py`へ実コマンドを保存する。

| 対象 | 実行と結果 |
| --- | --- |
| canonical Debug configure | `cmake -S D:/NeNeFolio/worktrees/407-nullable-allocation -B D:/NeNeFolio/outputs/407-nullable-allocation/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、exit 0 |
| 対象build/tidy | `cmake --build D:/NeNeFolio/outputs/407-nullable-allocation/build --target folio_tests`、exit 0 |
| 変更toolの正負例 | `python -m unittest discover -s tests/conformance -p test_coverage_sanitizers.py -v`、8 tests / exit 0 |
| vector抽出 | 上記Debugのcore/application 48 sourceとunit 24 sourceの全recordで一致。`-fsanitize=address -fsanitize=undefined -fsanitize=nullability -fno-sanitize-recover=all` |
| 同vectorのbinding proof | 正例exit 0、反例exit 1と`binding.c`自身の`_Nonnull binding to null pointer`診断を確認。`measurement-attempt2/nullability-proof/results.json`へ記録 |
| 計装build/link | 既存`coverage.build_measurement`を上記vector・既存probe・/MT・profile/coverageで実行、exit 0。proof objectは別directory |
| 対象selector | 同測定exeの`--nullable-allocation`、exit 0。previewは7失敗点から成功まで、各OOM後のcaller matches再利用・out不変・READY移転を確認 |

上記build targetの依存をcompileしたが、unitの実行は対象selectorに限定した。
`run-static.ps1`のformat/diff/conformanceは`static-result.json`へ、入力・最終結果とhashは
`completion.json`、`final-inputs.json`、`hashes.json`へ集約する。
関係入力が変わらない担当交代・commit・push・mergeで対象成功を再実行しない。

Debugと測定/proofの実行にASan runtimeの`interception_win: unhandled instruction`警告があり、stderr原本に保持した。
期待したbinding反例以外にnullability/UBSan診断は出ていないが、この結果を全環境の保証へ広げない。
全体coverageの90%判定と負例profileはローカルで実行せず、最終headの必須CIへ残す。

## 引き継ぐ範囲

local full gate、無関係UI・Markdown描画・検索性能は実施しない。通常data/Nibと旧削除拒否済み作業物も操作しない。
別moduleで新たな診断が出たら設定を緩めず親判断へ返す。
対象成功は最終PR headの必須CI `pwsh -NoProfile -File ./eng/check.ps1`の代わりではない。
commit/push/PR/Ready/統合・必要反映と、その後の監査cleanupは親担当へ引き継ぎ、現時点の作業物は保持する。
