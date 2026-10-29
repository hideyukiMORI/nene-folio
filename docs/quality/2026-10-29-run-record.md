# argv試行の記録と原本保持 — 2026-10-29

Issue: #424。基点main `88cdf79d24b8da10a9c4455602ab646054206750`。
受理したdesign-decisionは`D:/NeNeFolio/agents/424-run-record/design-decision.md`。
規則: ARC-001/008/010/012、QLT-007/008/012。Waivers: none。
製品API/依存/schema・成功判定・ゲート閾値は変更しない。記録wrapperのJSONはschemaVersion1。

## 変更と記録の意味

`tools/record-run.py`はPython標準libraryだけで、一つのargvをshellなしで起動して試行を記録する。
run-dirは親が存在する未作成の絶対directoryで、mkdirの排他作成が所有境界となる。
同じdirectory・file・symlinkへの二度目の実行は子開始前に拒否し、既存記録を置換しない。
cwdは既存の絶対directory、input/artifactは明示された絶対fileだけを受け取る。
`--input`/`--artifact`は各fileごとに繰り返す。自動git/env/data列挙・glob・再帰snapshotは行わない。

run.jsonへUTC開始時刻/cwd/実argv/指定input/artifactを排他作成する。
inputsの実行前snapshotとartifactsの終了後snapshotは、番号付き名でbasename衝突を避ける。
copyとSHA-256をstreamで処理し、copy後のsource hash照合でcopy中変化を検出する。
子終了後にもinputを再hashし、unchanged/changed/missing/errorを区別する。

子はstdin=DEVNULL、WindowsではCREATE_NO_WINDOW。環境変数は子へ継承するが、wrapperが収集/保存しない。
子自身がstdout/stderrへ書いた値は通常の子出力であり、環境の自動記録とは別である。
stdout.log/stderr.logはbinaryで分離し、JSONのchildExitCodeとwrapperExitCodeを別fieldにする。
記録が成立した子の非zeroはそのまま返す。準備/起動/記録失敗・input変化はwrapper125で、実child終了値を保持する。
CLI構文エラーはargparseの非zeroで子開始前に拒否する。
Windowsのhigh-bit DWORD終了値はJSONへraw値を保持し、最後のSystemExitへ渡すときだけsigned32へ写す。
PythonのC int変換による0xFFFFFFFFへの置換を避け、実processの32bit patternを子と揃える。
result.jsonを最後に排他作成し、保存できなければstderrへ明記して125を返す。
欠落/partial/読めないresultは中断/未完了であり成功と扱わない。

snapshot間で一瞬変化して戻った内容、未指定依存、artifactの生成由来・build成功は認定しない。
artifactは終了時に存在したbytesの記録に限る。中断時の耐久性や、意図的な悪意ある子からのdirectory隔離も主張しない。
再試行/選別/削除はwrapperの仕事に含めない。

## 対象検証と証跡

`tests/conformance/test_run_record.py`を既存unittest discoveryへ加える。別framework/runnerは作らない。
対象は初回9件と追加Windows終了値1件。argvの空白/Unicode/引用符/空引数、raw binary stdout/stderr、環境継承と非収集、子非zero、
pre input/post artifact、同basename、同directory再実行拒否と全bytes不変、input変更/消失、
missing inputで子未実行、launch失敗、missing artifact、malformed/relative/empty argv、
copy後source digest差異と最後のresult書込失敗を確認する。
Windows限定の追加caseはctypesのExitProcess(0xC0000005)で明示終了する合成子を使い、実crashを起こさない。

local test rootは`NENEFOLIO_RUN_RECORD_TEST_ROOT=D:/NeNeFolio/outputs/424-run-record/tests`を明示する。
CI/既存discoveryの既定はrepo内`out/run-record-tests`で、system Tempを使わない。
各caseは別directoryを作り、失敗/成功とも親の保全監査まで保持する。
Pythonは-BとPYTHONDONTWRITEBYTECODE=1を使用する。

自己レビュー後、完成したrecord-run自身で最初の9件を次の対象commandへまとめた。
`python -B -m unittest discover -s tests/conformance -p test_run_record.py -v`。
その外側のconsole stdout/stderr/exitもagentsへ分離保存し、wrapper故障時の部分記録を確認できるようにする。
結果と実コマンド/runtime、source/test hash、全試行は`D:/NeNeFolio/agents/424-run-record/`、
新しいrun-dir/test作業領域は`D:/NeNeFolio/outputs/424-run-record/`へ置く。
最終状態はcompletion.json、入力/出力hashはhashes.jsonを参照する。

| 試行 | 実行範囲 | 外側unittest child / recording wrapper | 結果 |
| --- | --- | --- | --- |
| attempt1 | 初回9件 | 0 / 0 | 全成功、入力2fileの前後hash不変 |
| attempt2-high-bit | 追加Windows1件だけ | 1 / 1 | inner child raw3221225477、JSON wrapperも同値だが、実wrapper bitpattern4294967295で失敗 |
| attempt3-high-bit | 同じWindows1件だけ | 0 / 0 | signed32変換後、raw child/JSON wrapper/実wrapperのbitpatternが0xC0000005で一致 |

Windows追加caseの初回失敗は別run-dirに固定し、source/test snapshot・raw log・実argv/runtimeを保持した。
high-bitの変更はCLI最下部でWindowsの0x7FFFFFFF超だけを変換し、mainや記録内容、通常終了値には触れない。
この変更境界1件だけを再確認した。通常終了値と記録処理には影響しないCLI末尾だけの変更であり、先に成功した9件の結果を再利用した。
最終CIでは同じdiscoveryが10件を対象とする。今回localで最終版10件を一括実行したとは主張しない。
正常17と異なりhigh-bitは初回試行で保持できておらず、その差を成功扱いしない。

## 範囲と引継ぎ

この記録はtool自身の対象検証であり、通常C/UI/検索・native・性能計時やlocal fullgateは実行しない。
最終head必須CI、commit/push/PR/統合/反映・恒久保全/安全監査/整理は親担当。
通常data/Nibと既存拒否33pathへ操作せず、拒否を再試行しない。
未統合のworktreeと証跡/出力は親監査まで保持する。
