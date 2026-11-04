# 破線端点記号の描画費用 — #589

#587は破線の端点記号ごとに実線penを追加取得/解放する。96messageを持つ3種の破線と実線対照を
固定Releaseで交互6組測定し、その範囲の費用を記録した。開矢頭は減少、×印/塗りつぶしと実線対照は増加した。
費用ゼロ・速度改善・任意図の最悪値を主張せず、追加の製品修正は行わない。

## 比較対象と正典経路

旧版は#573 `df6ba8b729a5a95d459c06332873bad29537b343`の保全ZIP内のRelease3libを、archive全体と各entryのSHA照合で取得した。
現行はmain `265b36a3be607498e76bd189e4bcfb1a5e8b42f7`から正典toolchain/CMake Releaseでcore/application/adaptersをbuildした。
旧worktreeの全srcと現行をhash比較し、差分はdiagram_canvasとdiagram_flow_layoutの2fileと確認した。
入力はsequenceだけなのでflow labelの変更は選択経路の外。公開header/書体/依存/版を変更せず、両版に同じ現行font7fileを置いた。
機械語全体が一関数だけ異なる比較ではなく、CPU配置/他process負荷も固定していない。原因の完全な分離とは扱わない。

同じ外部benchmark-native.cを各3libへlinkし、既存note_text→mermaid_diagram→layout→native rasterと既存cacheを通した。
原本は`D:/NeNeFolio/agents/marker-performance-20261104`、全snapshot/raw/bitmapは同名outputsへ保存。
source-context.json、prior-files.json、performance-inputs.jsonが比較根拠である。

## 入力と測り方

参加者A/B、同じmessage文字、交互方向96本の4入力を使った。`--)`/`--x`/`-->>`と、実線対照の`-)`。
全画像232×4858。背景や文字を取り除いた人工的なpen操作時間ではなく、完成画像の生成時間を測った。
旧→現行と現行→旧を組ごとに交互にし、6組/12processで実行。各process/入力は生成21回とcache複製21回。
初回は分けて全値を保存し、残る20回の中央値を取り、表のmsはその6組の中央値である。
組差は同組の生成中央値比を6組で集約するため、表のms同士の単純比とは異なる。
QPCはdiagram_render_readとcache取得を囲み、既存phase通知からparse/font・文字計測/layout・rasterを分けた。
process起動、font登録、worker queue、UI timer、bitmap保存は計時外。最終cache取得後だけbitmapを保存した。
GetSystemTimesの負荷はprocess全体の前後であり、単一描画のCPU率ではない。

## 結果と判断

| 入力（各96message） | 旧生成ms | 現行生成ms | 組差中央値 | 遅い組 | 旧/現行raster ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| open-dashed.mmd | 16.589 | 16.405 | -1.77% | 2/6 | 16.140 / 15.917 |
| cross-dashed.mmd | 16.612 | 16.921 | +1.72% | 4/6 | 16.160 / 16.460 |
| closed-dashed.mmd | 16.353 | 16.571 | +1.33% | 6/6 | 15.901 / 16.134 |
| open-solid.mmd | 15.623 | 15.724 | +0.61% | 5/6 | 15.167 / 15.197 |

生成/cache各1008sampleは全件正の有限値、phase和とtotal差は0.0001ms未満。48bitmapを再hash照合した。
全入力で前後寸法一致、実線対照は全12画像同SHA。3種の破線は各版6枚が安定し、版間のSHAは異なる。
現行の96本塗りつぶし図を視認し、全メッセージと両端の参加者を確認した。
CPU busyは7.04〜15.57%。初回最大27.1824ms（旧open、pair2）、非初回最大25.6826ms（旧open、pair0/sample10）も除外せず保持。
cache中央値は各版各入力で約0.56〜0.59ms。画像copyは同じ寸法でありpen追加の処理を通らない。

塗りつぶしは6/6組で増加し、生成中央値は約0.218ms増、組差中央値+1.33%。一方、実線対照も5/6組で+0.61%、
開矢頭は2/6組だけ増加し-1.77%だった。小さな増加傾向と対照の変動を併記し、単一のpen費用へ断定しない。
今回の有限な96message入力では追加の資源所有変更やpen共有を導入せず、#587の局所所有を維持する。
性能差なし/全図高速化という評価や、95percentile/統計的有意差の主張は行わない。

## 実行・規約・限界

| run | 実行と結果 |
| --- | --- |
| build-release-01 | build-release.ps1、正典CMake Releaseの3lib成功。子/記録側0 |
| link-prior-01 / link-current-01 | 同じ外部harnessを各3libへlink、子/記録側0 |
| paired-01 | paired-measure.py paired-01、12process/48bitmap、子/記録側0 |
| audit-01 | audit.py、raw1008生成+1008cache/phase和/48画像/対照を照合、子/記録側0 |

全runはtools/record-run.pyの入力/成果物snapshot・raw stdout/stderr・終了値とともに保存し、stderr/recordingErrorsは全空。
製品/test/ADR/API/schema/版/依存/gate変更なし。文書5件を更新し、差分・相対リンクと最終head必須CIを通す。
ARC-001/003/010/012、QLT-008/010/012、GIT-001〜004、ADR0089、開発ワークフロー§8を適用。
成功入力が不変ならpush/統合だけで再実行せず、通常exeは#587との製品入力/実hash一致で再利用する。
統合/通常exe照合/保全整理の確定値はintegration-completion.jsonへ収載する。追加waiverなし、WVR-0001既存範囲のみ。
単一host/明theme/4入力の観測であり、任意図・最大負荷・全DPI/書体/実UIの性能や確保失敗を保証しない。
実app/実ノートを試験へ使わず、Store操作なし。
