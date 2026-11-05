# Markdown段落の分布別構築費用 — #622

同じ1MiBでも、空行のない短いsoft lineを大量に連ねるとRTF構築費用が大きい。
固定Releaseの中央値は、ASCIIの改行なし11.4459ms、79文字+LF59.7733ms、1文字+LF838.4193msだった。
この作業は現実装の基点計測と文書化だけで、製品の実装・API・cache・依存・schema・版・gateを変更しない。

## 範囲と正典

基点はmain `6107a267cf17edd5abf98c27447c92d3cd4cd7a9`（#620統合後）。
`note_text_create` → `markdown_rtf_create` → `markdown_blocks` → `markdown_inline_tokens` → `markdown_view_at`を使う。
note_text生成は計時外で、RTF構築だけをQPCで測る。実UI、RichEdit投入、レイアウト、描画、I/O、peak heapは測らない。
#597の未閉じ構文系列と今回の段落分布は別であり、古い計測値を基点として再利用しない。

規則: ARC-001/003/004/007/008/012、C-008/012/016、QLT-008/012/013、GIT-001〜004。
追加waiver none。既存WVR-0001はWinHTTP callbackだけで、今回へ適用しない。新ADRや規範変更はない。

## 再現条件

正典toolchain/CMakeのRelease core libraryを83actionsで構築した。同じcompile_commands.jsonの実vectorから、
O2/Ob2/DNDEBUG/MT/C23と警告を維持したrepo外observerをリンクする。製品に計測用窓口を加えない。
実argv・source/object/library/exeのsnapshot・stdout/stderr・終了値はrecord-run.pyで保持する。

Windows 11 Pro 10.0.26300、Core i9-10850K、20 logical processors。observer threadだけaffinity mask4、優先度不変。
既知のbuild/probeが無いことを開始前に確認し、計測中に親のbuild/hash/archiveを重ねない。
OS全体のidle、排他的CPU、一定クロック/cacheは保証しない。QPCは経過時間、QueryThreadCycleTimeは別の補助指標とする。
QPC自体にaffinity固定は不要で、今回は実験条件として固定した。cycleにはuser/kernel両方が含まれ、経過時間へ換算しない。
根拠はMicrosoftの[QPCの説明](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)と
[QueryThreadCycleTime](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime)。

決定的な6patternを1KiB/16KiB/256KiB/1MiBへ繰り返し、不足末尾はASCII Xで埋める24入力。
全入力のbytes/SHA256/pattern/改行数をmanifest.jsonへ保存し、UTF8妥当・NUL無しを確認した。
各caseを別processで実行し、30秒timeoutを設ける。入力のread/hash/note_text生成と出力保存は計時外。
1回のwarmup出力を保持し、5回のmarkdown_rtf_createの前後だけQPCを取る。破棄・全bytes比較も計時外。
全sampleがwarmupと同一bytes、原文不変、RTF envelope妥当を確認した。構文の完全な正解oracleや実UI表示の証明ではない。

## RTF構築の結果

各case5sampleの中央値ms。全標本/range/cycleはmeasurements.jsonに残す。小さい入力ではtimer/呼出し費用の影響もある。

| 入力 | 1KiB | 16KiB | 256KiB | 1MiB |
| --- | ---: | ---: | ---: | ---: |
| ASCII・改行なし | 0.0171 | 0.1849 | 2.7360 | 11.4459 |
| ASCII・79文字+LF | 0.0298 | 0.6874 | 13.4385 | 59.7733 |
| ASCII・1文字+LF | 0.6792 | 11.5146 | 201.4483 | 838.4193 |
| ASCII・78文字+空行 | 0.0215 | 0.3235 | 4.7754 | 18.9413 |
| 日本語・改行なし | 0.0134 | 0.1835 | 2.6824 | 11.0836 |
| 日本語・81byte+LF | 0.0298 | 0.9691 | 13.3122 | 59.6149 |

1文字+LFは単一の大きな段落を作る。空行で小段落へ分かれる入力は1MiBでも18.9413msで、
単に改行の総数や入力byte数だけではこの差を説明できない。一般的な時間上限や計算量の実証とは扱わない。

## 直接lookupの切り分け

静読ではmarkdown_view_atが複数spanのoffsetを二分探索する。上の差だけから原因を断定せず、
ASCII3分布×4サイズについて、LFから借用spanを組み立てる独立observerを追加した。
markdown_view_validの後、各byteをmarkdown_view_atで順に読み、元入力と全byte比較する。
core libraryとfixtureを再利用し、製品を変えず12caseを計測した。各5sample、別warmup無し。
1sampleの一巡反復は1KiB256回/16KiB16回/256KiB2回/1MiB1回。一巡あたりへ正規化する。
callerのloop・byte検査込みであり、RTF処理のprofileや寄与率ではない。

| 1MiB入力 | span数 | 一巡の中央値ms |
| --- | ---: | ---: |
| ASCII・改行なし | 1 | 3.1845 |
| ASCII・79文字+LF | 13,108 | 20.4610 |
| ASCII・1文字+LF | 524,288 | 47.7530 |

全値はlookup-measurements.json。spanが多い場合のlookup費用増は観測できたが、838msの全てを説明するとは言わない。
次はこの根拠を別Issueの修正候補と対照入力で検討する。不均一spanや短い入力の不利な費用、空span/境界を含めて判断し、
新cache/別経路/APIを根拠なく増やさない。この計測だけで製品改善済みとは報告しない。

## 試行と検証

configure-release-01、build-core-01、fixtures-01、compile-observer-01、measure-01、
compile-view-01、measure-view-01は全て子/記録0、記録error0、全input終了時不変。
audit-01も子/記録0。generatorをimportしない監査で24fixtureと24RTFを再hashし、UTF8/NUL、
raw QPCから全中央値、lookupのspan/query数を再計算した。36case/180timed samplesの整合が成功した。
補助PSのparam配置は実行前読取で直し、失敗runとは扱わない。準備時の誤path/rg引数と表示切れはjournalへ分離記録する。

文書の差分・参照・conformanceを対象確認し、最終head必須CIのeng/check.ps1と正典mergeで統合する。
製品/test変更がないため、既に成功した製品検証や通常exeの再build・実app起動を繰り返さない。
文書検査/CI/統合/通常exe不変/保全整理の確定は同taskのintegration-completion.jsonと実runへ記録する。

恒久原本: `D:/NeNeFolio/agents/622-markdown-paragraph-scale/`。
全体journal: `D:/NeNeFolio/agents/quality-20261105/journal.md`。
D側の専用worktree/outputを全保全/hash照合し、取込/未保存/ignored/リンク/稼働参照の監査後に整理する。
branch/commitと計測入力/出力/補助sourceは保持する。実ノート・通常app・Storeを試験に使わない。

## 文書確認の確定

conformance-01は補助driverに前taskの未構築debug pathが残り、File API reply不足で拒否された。
子1/記録125と未生成docs-check.jsonの記録error1を保持する。製品やゲート設定の不良とは扱わない。
文書だけの対象に合わせて任意build-dir引数を外し、conformance-02でsource規約0違反・6文書111local参照・diff成功。
これはbuild graph検査成功の主張ではなく、graphを含む完了定義は最終head必須CIで実行する。
追加後の本文差分確認を行い、成功済み製品計測は再実行しない。

## 09:00 — #622統合・保全整理完了

PR623最終head aab06aaeの必須CI41936962814 full-gate/check成功、mainbd8359eへ正典統合、Issue622 CLOSED。
通常exe/data32fileは#620完了時とbytes/hash/作成更新日時が不変、再build/app起動無し。
1,314fileを全保全/hash照合後、取込/dirty/ignored/リンク/稼働参照を監査して専用worktree/outputを削除した。
branch/commit保持。原本D:/NeNeFolio/agents/622-markdown-paragraph-scale/integration-completion.json。
