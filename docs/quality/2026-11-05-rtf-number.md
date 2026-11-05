# RTF数値出力の一括追加 — 2026-11-05 / #667

## 採用した最小変更

rtf_text_numberは同じstack配列を末尾から埋め、正順の数字列をtext_buffer_appendで一度だけ追加する。
従前は逆順配列から1桁ずつappendしていた。0を含む十進表記、stack容量、API、text_buffer所有者とsticky OOM、RTF全文を維持する。
別formatter・新製品入口・依存・schema・ゲート変更はない。[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)の単一RTF経路内の局所変更である。

独立した数値batchで16値は約27%、59値は約61%短縮したため採用した。
Markdown全体の差は対照条件にも揺れがあり、全体renderや実UIの高速化を示したとはしない。

## 固定計測

正典Releaseのcompile_commandsからrtf_text TUの/O2 /Ob2 /DNDEBUG /MT /C23 /WX等をそのまま派生した。
baseline/candidateの明示rtf_text.objと、同じ凍結nenefolio_core.libをlinkする。MAPでrtf_textの2入口とtext_buffer/markdown_rtfの所属を確認した。
候補以外の実装/旗/依存は同じ。main6301780→7838718の差分はdocsとnative testだけで、src/eng/CMake不変を確認して再利用した。

- number-small: 0/1/9/10/11/99/100/101/180/200/999/1000/1001/9000/9999/10000の16値。
- number-wide: 0、SIZE_MAX、10の冪の前後と一致値（10〜10^19）の59値。
  Pythonの整数十進表記とcomma区切りで独立した全出力期待を作る。
- heading/ordered/table/plain: 正常な日本語/ASCII/絵文字を含む短いblockを16/512回。
  変更前のbaselineで全RTF bytesを候補変更前に凍結する。これは同値比較であり、独立した構文期待は既存unitで確認する。

10条件、20fixture file。各版の全出力を計時前後に比較する。計時内は通常のowner生成/出力/解放と結果長の検査を含み、file読取やmemcmpは含めない。
numberの一操作は16/59値全体とseparator/owner生成解放を含むbatch。Markdownは1文書のRTF生成/解放で、note_textの生成は計時外。
profile02はwarmup1/各5sample（50）。採否はABBA、case順も交互反転、各版各10sample（200）で比較した。
短いbatchの計時を伸ばすためABBAはprofileのnumber反復を16倍、Markdown反復を4倍にし、両版同条件にする。
QPCとthread cycle、affinity mask4、priority不変。親のbuild/archiveは計時中に実行しない。OS全体の無負荷は保証しない。

| 条件 | 旧中央値µs/操作 | 候補µs/操作 | QPC差 |
| --- | ---: | ---: | ---: |
| number-small | 0.351 | 0.255 | -27.29% |
| number-wide | 3.535 | 1.387 | -60.77% |
| heading-16 | 23.821 | 22.417 | -5.89% |
| heading-512 | 673.950 | 661.669 | -1.82% |
| ordered-16 | 41.783 | 42.411 | +1.50% |
| ordered-512 | 1271.844 | 1259.544 | -0.97% |
| table-16 | 50.767 | 47.820 | -5.81% |
| table-512 | 1813.519 | 1741.787 | -3.96% |
| plain-16 | 19.960 | 19.762 | -0.99% |
| plain-512 | 579.631 | 549.644 | -5.17% |

numberのthread cycle差も約-27.40%/-60.72%。plainはこのformatterを使わない対照で約-0.99%/-5.17%のQPC差があり、
小さな全文差を数値出力だけへ帰属させない。全75値/RTF全文/250有効sampleは一致、測定stderr空。

## 恒久回帰と対象検証

既存markdown_testsに0/SIZE_MAX/10の冪前後59値×prefix8長=472組を追加した。
prefixは0/235/236/254/255/256/511/512byte。独立したsnprintf `%zu;`と、全prefix/数値/suffix/終端・長さを比較する。
255byteを入れたbufferの次の成長を既存allocation_probeで失敗させ、OOMと以後の出力停止を確認する。部分bufferは公開しない既存契約のまま。

--markdown-emissionは既存run_markdown_testsをそのまま選ぶ。共通formatterを使う見出し/list/table/inline/plainと既存変換OOMを対象にし、
独立したdiagramやcode-syntax suiteは追加実行しない。全suiteも同じ関数を通る。selectorを分離し既存--markdown-rtfの処理を維持した。

| run | 実行と結果 |
| --- | --- |
| configure-release-01 / build-core-01 | 正典Release configure/core 83actions成功 |
| compile-baseline-01 / expected-01 | 変更前observerと8つのRTF全文期待を凍結、成功 |
| profile-01 | 補助が数値fixtureのCRLFをLFのみとして拒否。最初のcase、計時前、子/記録1/error0 |
| compile-baseline-02 / profile-02 | 補助読取だけLF/CRLFへ対応。fixture/期待/製品不変、全10条件50sample成功 |
| compile-candidate-01 / measure-candidate-01 | 一括append候補、ABBA全10条件200sample成功 |
| configure-debug-01 / build-debug-01 | configure成功。selectorが66行で上限60を超えbuild停止、子/記録1/error0 |
| build-debug-02 / contract-01 | Markdown selector helperへ分離後28actions成功。folio_tests.exe --markdown-emission成功、stderr空 |
| build-measurement-01 / allocation-01 | 正典coverage.build_measurementの85製品/31test TUとDebug由来sanitizerでbuild、同selector成功、numeric OOM active=1 |
| allocation-diagnostic-01 | 同exe/source/selectorにASan verbosity=1だけ追加し通知を特定。子/記録0、全対象成功 |
| symbols-01 / audit-01 | core/application 2library 0違反。全raw/中央値/20hash/MAP/Release vector/同archiveを照合成功 |
| conformance-01 | 補助の参照抽出がADRのコード例から複数段落を誤ってpathに取り、検査前に停止。子1/記録125/artifact不存在error1 |
| conformance-02 | 補助は通過。正典CNF-006も同じ未閉じコード例から追加リンクまでを誤認し1違反。子1/記録125/artifact不存在error1 |
| conformance-03 | ADRのコード例を同じ意味の文章へ直し、対象文書参照、diff/source/build graphを確認 |

全runはtools/record-run.pyで入力snapshot/終了時不変/raw/終了値を保存する。初回失敗のexe/inputs/rawも削除しない。
Debugと測定buildはASan/UBSan/nullability/no-recoverを保持する。測定buildの正規binding proofは正例0、反例は目的のnullability診断と非zero終了を確認した。
ただしallocation-01とproof正例にASan interception_win通知がある。追加診断は`failed to intercept 'frexp'`と初期化完了を記録した。
[固定LLVM19.1.5の公式実装](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.5/compiler-rt/lib/interception/interception_win.cpp)も未知命令の通知を非致命とし、失敗を返す。
当該フックの完全捕捉を保証しない。src/testsにfrexp直接参照は無く、今回の明示realloc失敗はactive=1で実行できた。通知は抑制せず保全し、全sanitizer捕捉の証明とはしない。
初回buildの行数違反も閾値変更せず構成を修正した。文書補助の訂正は文字クラスから改行を除き、コード例の未閉じ構文から後続段落を吸い込まないようにした。
正典側も同じ例から新しいリンクまでを誤認したため、ADRの例を「閉じ角括弧に続く開き丸括弧」という同義の文章に直した。
製品と正典conformanceの規則は変えていない。補助読取の推測path/filename不在はjournalに分離して記録する。

## 完了条件と限界

対象は十進数出力と通常Markdown。一般UI速度、全入力/OS/CRTフックの新保証ではない。API/schema/新依存なし、追加waiver none、既存WVR-0001継続。
規則ARC-001/004/007/008/012、C-008/012/014、QLT-008/009/010/012/013、GIT-001〜004。親単体、追加委任無し、実app/実ノート/Store未使用、#634等保留不変。
最終head必須CI eng/check.ps1成功→tools/merge-pr.ps1統合→正典の通常exe反映/data全bytesと日時不変→全保全/取込tree/clean/ignored/link/稼働参照監査→当該D専用領域整理。
branch/commit保持。原本D:/NeNeFolio/agents/667-rtf-number/、最終結果は同integration-completion.json、全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。
