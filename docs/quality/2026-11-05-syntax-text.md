# 通常文字tokenの空装飾追加を省く — 2026-11-05（#675）

code_syntax.emitは通常文字にも空のstyle文字列をtext_buffer_append_textへ渡していた。
その追加だけをCODE_TOKEN_TEXTの場合に省く。色を付けるtokenの開き/閉じ、原文escaping、
字句境界、code_syntaxの状態/寿命、text_bufferの成長とsticky OOM、公開API/schemaは変えない。
製品変更はsrc/core/code_syntax.cの条件一つ。tests/unit/folio_tests.cへ既存run_code_syntax_testsを
単独で選ぶ--code-syntaxを加え、既存期待/fixtureの変更や重複テストの追加はない。

## 比較条件と採否

main547d745から正典Release coreを作り、source/vector/archiveを凍結した。#673の文書統合85d6644へFFし、
src/eng/CMake不変を照合してbuildを再利用。明示code_syntax objectと同corearchiveを同Release flagsでlinkする。
/O2 /Ob2 /DNDEBUG -MT C23 /WXを含むvectorは同一、保護設定/抑制/依存は変更しない。
直接syntaxとfull Markdownの2observer×2版のMAPで14symbol所属を照合した。

plain/C/C++/JavaScript/TypeScript/Python/JSON/Shell/PowerShell/SQLの通常snippetを16/1024行にし、
直接syntax20条件、fenced Markdown20条件、通常文章2対照、計42条件/84UTF8fileを使う。
codeは実行しない。syntaxの期待は明示手書きRTF fragment、full Markdownの期待は基準全出力の凍結bytes。
後者は独立Markdown oracleではない。直接/全文とも計時前後に完全bytesを比較する。

profile42条件×5sample、初回ABBAはprofileの反復16倍、確認BAABは64倍。
初回に変更しない短plainが-14.33%となったため、順序と反復を変えて確認した。候補source/exeは同一。
各比較/各版/各条件10sample、全378raw/1890sample。条件順も交互反転、warmup1、thread affinity mask4、priority不変。
通常の生成/追加/解放と長さ確認を計時内に含め、親のbuild/archiveは計測中に実行しない。

BAABでは装飾ありfull Markdown18条件が約1.25〜7.46%短縮し、同じ方向が初回の大部分でも続いたため最小変更を採る。
一方短plain直接は確認+3.87%、長い通常文章は+1.77%、C/SQLの長い直接syntaxは+0.08/+0.93%である。
対照の揺れやcode layoutを分離した測定ではなく、全経路・一般UI・全入力の速度改善は主張しない。

下表は確認BAAB自身の中央値で、初回ABBA差を別列にする。基準を比較間で混ぜない。
1operationあたりµs、差は候補/基準−1。

| 条件 | 初回QPC差 | 確認基準 µs | 確認候補 µs | 確認QPC差 | 確認cycles差 |
| --- | ---: | ---: | ---: | ---: | ---: |
| syntax-0-16 | -14.33% | 0.647 | 0.672 | +3.87% | +2.22% |
| markdown-0-16 | -0.14% | 3.958 | 4.072 | +2.88% | +1.96% |
| syntax-0-1024 | -2.24% | 31.357 | 30.891 | -1.49% | -2.36% |
| markdown-0-1024 | -2.61% | 116.783 | 113.436 | -2.87% | -2.81% |
| syntax-1-16 | -3.40% | 7.475 | 7.299 | -2.35% | -2.53% |
| markdown-1-16 | -4.56% | 10.948 | 10.728 | -2.01% | -2.84% |
| syntax-1-1024 | +0.79% | 465.000 | 465.363 | +0.08% | -0.45% |
| markdown-1-1024 | -1.03% | 555.885 | 546.150 | -1.75% | -2.54% |
| syntax-2-16 | -2.88% | 10.972 | 10.897 | -0.68% | -1.17% |
| markdown-2-16 | -2.43% | 14.597 | 14.405 | -1.31% | -2.37% |
| syntax-2-1024 | -3.71% | 685.090 | 670.905 | -2.07% | -2.21% |
| markdown-2-1024 | +0.19% | 788.752 | 778.891 | -1.25% | -1.58% |
| syntax-3-16 | -5.21% | 6.357 | 6.182 | -2.76% | -2.88% |
| markdown-3-16 | -0.47% | 9.820 | 9.487 | -3.39% | -3.41% |
| syntax-3-1024 | -8.32% | 398.974 | 383.820 | -3.80% | -4.48% |
| markdown-3-1024 | -4.49% | 493.375 | 469.930 | -4.75% | -5.12% |
| syntax-4-16 | -0.09% | 12.374 | 12.081 | -2.36% | -2.22% |
| markdown-4-16 | -2.56% | 16.067 | 15.504 | -3.50% | -3.75% |
| syntax-4-1024 | -1.87% | 769.933 | 756.073 | -1.80% | -2.06% |
| markdown-4-1024 | -3.21% | 883.113 | 841.956 | -4.66% | -4.97% |
| syntax-5-16 | +1.09% | 4.220 | 4.084 | -3.22% | -3.79% |
| markdown-5-16 | -7.40% | 7.565 | 7.227 | -4.47% | -4.10% |
| syntax-5-1024 | -5.29% | 258.469 | 243.863 | -5.65% | -5.67% |
| markdown-5-1024 | -6.11% | 348.062 | 328.012 | -5.76% | -6.01% |
| syntax-6-16 | -7.19% | 5.742 | 5.418 | -5.65% | -7.16% |
| markdown-6-16 | -5.35% | 9.280 | 9.025 | -2.75% | -3.22% |
| syntax-6-1024 | -8.37% | 358.192 | 336.173 | -6.15% | -6.10% |
| markdown-6-1024 | -6.38% | 446.730 | 414.920 | -7.12% | -7.32% |
| syntax-7-16 | -9.53% | 3.669 | 3.462 | -5.65% | -6.43% |
| markdown-7-16 | -5.04% | 7.059 | 6.768 | -4.13% | -4.52% |
| syntax-7-1024 | -6.52% | 219.111 | 205.266 | -6.32% | -6.48% |
| markdown-7-1024 | -4.69% | 307.277 | 288.774 | -6.02% | -5.85% |
| syntax-8-16 | -4.08% | 4.618 | 4.434 | -3.99% | -3.98% |
| markdown-8-16 | -4.38% | 8.112 | 7.692 | -5.18% | -5.37% |
| syntax-8-1024 | -4.44% | 284.470 | 270.326 | -4.97% | -4.45% |
| markdown-8-1024 | -7.37% | 377.248 | 349.087 | -7.46% | -7.46% |
| syntax-9-16 | +0.57% | 17.187 | 17.129 | -0.33% | -0.30% |
| markdown-9-16 | -2.83% | 21.269 | 20.554 | -3.36% | -3.98% |
| syntax-9-1024 | -1.01% | 1100.898 | 1111.110 | +0.93% | +0.68% |
| markdown-9-1024 | -3.37% | 1212.657 | 1155.677 | -4.70% | -4.17% |
| prose-16 | -0.59% | 25.872 | 25.422 | -1.74% | -1.99% |
| prose-1024 | +2.44% | 1115.870 | 1135.580 | +1.77% | +1.78% |

## 対象検証と途中失敗

全実行はtools/record-run.pyの新run directoryに入力snapshot/hash、stdout/stderr、子/記録終了値を保存した。
専用作業木はD:/NeNeFolio/worktrees/675-syntax-text、buildは同outputs、原本は同agents。

- configure-release-01 / build-core-01（83actions）、reference-01、baseline/candidate compile、fixtures-01、profile-01、compare01-01、confirm-01、audit-01成功。
- configure-debug-01 / build-debug-01（119actions）は正典Debugと変更2fileのclang-format検査で成功。
- contract-01: debug/folio_tests.exe --code-syntaxと--markdown-emission成功、stderr空。全言語/継続token/escaping/fence境界/状態寿命と既存Markdown期待を再利用。
- symbols-01: 正典eng/symbols.py、2libraries/0違反。
- build-measurement-01はhelperの引数名を-Phaseと誤記し、-Flavor必須の入口で停止（子/記録1、error0）。製品compile/test未実行。原rawを保全し、正しい-Flavor greenのbuild-measurement-02成功。
- allocation-01: 同じ2selectorを既存coverage.build_measurementの確保注入exeで成功。RTF numeric OOM active=1、Markdown各失敗点の拒否とその後の成功を確認。

測定buildはDebug由来ASan/UBSan/nullability/no-recover vectorを維持し、独立nullability binding正例0/反例の目的診断と非zero終了も成功。
allocation-01 stderrには2processそれぞれinterception_win unhandled instruction通知1行（同命令列0f 29 74 24 30 0f 28 f0）が出た。
#667で診断した通知と同じ命令列だが、今回のfunction同定は追加していない。全hook捕捉の保証にはせず、rawと限界を記録する。
抑制・ASAN設定変更・ゲート弱体化はない。通常Debugの対象stderrは空である。

auditは全84filehash/手書きsyntax期待/全rawのQPC-cycles/中央値/14MAPbinding/同flags-archive/最終候補sourceを照合した。
対象成功後は製品/testsource不変ならpush/mergeのために反復しない。文書conformance/参照/最終SHA自己レビュー後、最終headの必須CIに正典eng/check.ps1を要求する。

## 規則と限界

ARC-001/004/008/012、C-002/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。親単体、追加委任なし。公開API/owner/schema/依存/CMake/gate不変。
通常app/実ノート/Storeと#634の保留試験は対象外。補助読取の推測path不存在と引数失敗も全体journalへ記録する。
最終CI/統合/必要な通常反映とdata不変/全保全・専用整理はD:/NeNeFolio/agents/675-syntax-text/integration-completion.jsonへ。
branch/commitと全試行の原本は保持する。[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。

## 18:54〜18:56 統合・通常反映・全保全整理完了

#675はPR676/head46ccf260f17b35503306457a0989b289929e03e0、最終CI42005269935 full-gate/check成功。
main6ab2539c6814da8ea15ce42e985373c5edadfe28へ正典統合、Issue675 CLOSED。通常版反映は3actions成功、data32file bytes/hash/作成更新日時不変。
通常exeSHA5ab23f3244c45c78335e05d8ff6e22772ed59541c9a67e8f46733bb0088ce187、通常app起動なし。
1696file全保全ZIP再hash/取込tree/clean-untracked/ignored/link/稼働参照監査後、675専用worktree/outputを削除。
branch/commitと全途中失敗/測定通知保持。原本D:/NeNeFolio/agents/675-syntax-text/integration-completion.json。
