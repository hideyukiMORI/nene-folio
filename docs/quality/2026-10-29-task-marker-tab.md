# task印の後続tab — 2026-10-29

Issue: [#430](https://github.com/hideyukiMORI/nene-folio/issues/430)。base main `ac4b1ad34af5969e3490f776807babe8d26a671f`。
規則: ARC-001/012、C-016、QLT-008/012/013、GIT-001〜004。Waivers: none。
[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)決定1のtaskと境界に従う既存契約の修復。

## 修正と範囲

リスト先頭の`[ ]` / `[x]` / `[X]`直後を半角空白または水平tabで区切る。
従来はtabを後続させると通常bulletと`[x]`等の本文になった。
`markdown_blocks.c:task_marker`の後続byte判定一箇所をSP/HTへ揃えた。
4byte以上、mark3種、括弧、paragraph未開始、leaf itemのpending、4byte消費は維持する。
追加確保・別parser・UI・保存形式・公開API・gateの変更はない。

単体fixtureは既存`expect_body`/`expect_fragment`/`expect_same_rendering`を共用する
`run_markdown_task_tests`へまとめ、全体runと`--markdown-tasks`から同じ期待を呼ぶ。
taskに直接関係する既存10 helper呼出しだけを移し、無関係なlist/quote/table fixtureは整理していない。
利用者向け区切り説明とcurrent/日報/handoffは親担当。

## 旧source red

実tabを含む`- [x]\ttext`一件の厳密RTF期待だけを旧sourceで実行した。
期待は完了印`\u9745?\tab `と本文`text`。
旧結果は通常の`\bullet\tab `の後に`[x]`、実tab、`text`が残り、`FAIL: rtf body`となった。
子終了値1・記録wrapper終了値1、recordingErrorsは0、入力は全て実行前後不変。
red時のunitは一件だけであり、後から追加した完成境界を旧版で実行したとは扱わない。
旧source/exe/raw/入力snapshotは`red-task-tab`と`build-red`に残し、greenで上書きしていない。

## 完成fixtureの対象

- mark3種のSP正例とHT同値、実HT一件の厳密RTF。
- tabだけで区切った空本文の未完了印、最初の区切り以降の空白/tab/末尾空白の保持。
- ordered taskから通常itemへ戻る連番8、quote内listと混合containerのleaf item。
- 既存のquote leaf/継続本文、非list、区切りなし本文・末尾、mark/括弧不正、内部tab。
- 内部および後続Unicode NBSP、escape、inline codeをtaskへ拡張しないこと。

本文なしで区切りもない`- [x]`、内部tabやUnicode whitespaceは採用対象外。
期待は既存表示契約と制御経路から決め、green結果へ追従して変更していない。
空本文でもitem本文はtask認識前に非空であり、認識後の空spanから印を持つ段落を出す。

## 対象検証

各実行は新しいrun-dirを使う`tools/record-run.py`で、指定source/test/config/driver/exeの実行前snapshot、
成果物、raw stdout/stderr、子と記録wrapperの終了値を分けて保存した。
正典`eng/toolchain.ps1`を実commandと同じPowerShell processで読み、configure前にFile APIの`codemodel-v2` queryを作った。
Pythonは`-B`を用い、toolchainも`PYTHONDONTWRITEBYTECODE=1`を設定する。

| run | 子 / wrapper | 結果 |
| --- | --- | --- |
| configure-red | 0 / 0 | Debug、File API query先行 |
| build-red | 0 / 0 | folio_tests、常設clang-tidyを含む |
| red-task-tab | 1 / 1 | 唯一の厳密RTF期待で旧不一致を再現 |
| format-candidate | 0 / 0 | 原sourceを変えずformatter出力を別artifactへ保存後、4fileへ適用 |
| build-green | 0 / 0 | 完成source/fixtureのDebug build/tidy |
| green-task-boundaries | 0 / 0 | `--markdown-tasks`、stderr空、全対象期待成功 |
| format-final | 0 / 0 | 担当C/H 4fileのdry-run/Werror |
| conformance-final | 未起動 / 未起動 | 長いargvの起動前失敗、resultなし |
| conformance-final-retry | 0 / 0 | 同じ739元inputとFile API、Conformance 0 |
| diff-final | 0 / 0 | tracked差分のcheck成功、親文書の改行予告warningのみ |

実コマンドの中心は次のとおり。完全なargvと指定入力は各`run.json`およびagentsの`commands.json`へ収載する。

```powershell
pwsh -NoProfile -File D:/NeNeFolio/agents/430-task-marker-tab/canonical-build.ps1 -Stage Configure -BuildDirectory D:/NeNeFolio/outputs/430-task-marker-tab/build-debug
pwsh -NoProfile -File D:/NeNeFolio/agents/430-task-marker-tab/canonical-build.ps1 -Stage Build -BuildDirectory D:/NeNeFolio/outputs/430-task-marker-tab/build-debug
D:/NeNeFolio/outputs/430-task-marker-tab/build-debug/folio_tests.exe --markdown-tasks
```

環境はWindows NT 10.0.26300.0、PowerShell 7.6.9、Python 3.12.10、LLVM 19.1.5、
CMake 3.31.6-msvc6、Ninja 1.12.1、MSVC toolset 14.44.35207。版はtoolchainの照合を通した。
selectorはMarkdown→RTFの単体確認であり、実機表示を確認した主張ではない（QLT-013）。

実行結果: 対象Debug build/tidy・selector・最終format/File API conformance/diffが成功した。
conformance初回は739指定inputだけでargvが62,518文字となり、PowerShellがpythonを起動できなかった。
実診断は`StandardOutputEncoding is only supported when standard output is redirected`であり、長さ制約が原因という説明は推定である。
run-dir/resultはなく、子・wrapper終了値はない。外側exit.txtの0は直前gitのLASTEXITCODE残存で、成功を意味しない。
redirectしたstdout/stderrは0bytesで、PowerShell診断はtool返却textから取得できた範囲を別記した。
小さいD側driverでJSONのargvをsys.argvへ渡し、runpy.run_pathで同じrecord-run CLIを呼んだ。
元739inputや検査scopeを減らさず、driver/argv JSON自身もinputへ加えた新runで成功した。
diffは親report/handoffのCRLF→LF予告warningを残した。新規qualityは末尾1newlineに保ち、親のstage後check対象になる。
探索時に存在しないbuild/format script名とCMakePresetsを読もうとしたread errorは、正典経路を確認して解消した。
試験失敗ではない。開始済み試行の記録errorは全て0で、唯一の子非zeroは意図した旧redである。起動前失敗も原本を保持する。

## 保全と残る範囲

作業木は`D:/NeNeFolio/worktrees/430-task-marker-tab`、原本は`D:/NeNeFolio/agents/430-task-marker-tab`、
試行とbuild一時物は`D:/NeNeFolio/outputs/430-task-marker-tab`。完成時にcommands/results/hashes/completionとsource snapshotを閉じる。
新規確保・native境界・検索を変えないためlocal OOM/native/検索/全件・性能試験は実施しない。
必須CIは親が最終headで行う。対象成功を統合済み・通常反映済みとは扱わない。

通常data/Nib・既存削除拒否36pathは不操作。担当はcommit/push/PR/通常反映/cleanupを行わない。
新作業物は未統合の証跡を含むため保持し、親が統合・必要反映・恒久保全後に絶対path、取込、
未保存/ignored/唯一成果物、稼働参照、リンクを監査して整理する。ブランチとcommitは保持する。
