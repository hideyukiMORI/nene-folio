# 子窓が覆う窓縁の入力経路（2026-10-30）

Issue [#451](https://github.com/hideyukiMORI/nene-folio/issues/451)。hideの左端resize報告を、朝の品質改善再開依頼に基づき調査・修正した。
親Astraが設計/受理、SOL `window_edge_investigation`が静読・fixture・実装・限定検証を担当した。
製品差分は`folio_window.c`、`drawer_window.c`、`folio_message.h`の3file、34追加/3削除。
規則はARC-001/004/011/012、C-002/008/012/017、QLT-008/012/013、GIT-001〜004、ADR0011/0064。Waivers none。

## 原因と正典経路

親のWM_NCHITTESTはsigned screen座標とDPIのsystem borderで8方向の縁を判定していたが、
drawerとsettings/nativeのbackdropは外周を覆い、自身のWM_NCHITTESTでHTCLIENTを返していた。
非input desktopの実製品UI threadからWindowFromPointを呼ぶと、子で探索が止まり、親にはqueryが届かなかった。
最大化中には別の境界として、見える左client点で親がHTLEFTを返すことも確認した。

既存edge計算を一つの`resize_hit`へ抽出し、IsZoomedならHTNOWHEREとした。親hit_testはこの関数を使い続ける。
子用queryはcaptureが無いときだけ同じ判定から通過可を答え、drawer/backdropの該当点だけHTTRANSPARENTを返す。
drawerはWM_APP+20の同期queryで親へ問い合わせる。子にborder、方向分類、最大化状態、別のresize開始処理を持たせない。
行drag/押下のcapture、通常client、card、内容のguardとmodalの寿命は既存経路を維持した。
公開application API、保存schema、CMake、依存、gate、閾値、除外、抑制に変更は無い。

## 変更前の証拠と失敗

旧base f167ceeで正典Debug configure/buildを一度行い、実compile_commands・Ninja・File APIからfixtureのvectorを派生した。
元の警告/define/include/CRT/sanitizerと4runtime、resource、library順を保持し、fixtureの主TU・console entry・出力/map/verboseだけを替えた。
製品Cをincludeする最初のderiveは、header扱いになったWin32 typedefのnullability completeness診断で停止した。
child1/wrapper125、未生成4成果物の記録errorを保持し、警告を緩めず製品sourceのbytesを主TUへそのまま展開して修正した。
成功した旧製品buildは再実行していない。

baseline全体は**73 checks / 1 failure**。8点の遮断成立を全体の合格と扱わない。
各点は親への直接hitをgeometry対照に使い、その後traceをresetした独立WindowFromPointで子の遮断を観測した。

| 点 | 旧OS query | 候補OS query |
| --- | --- | --- |
| BOTHの左・下・左下角 | drawer HTCLIENT、親query0 | drawer HTTRANSPARENT、親の対応edge |
| DRAWERの右・右下角 | drawer HTCLIENT、親query0 | drawer HTTRANSPARENT、親の対応edge |
| settings backdropの左・下 | backdrop HTCLIENT、親query0 | backdrop HTTRANSPARENT、親の対応edge |
| 実native名前入力loop中の左 | backdrop HTCLIENT、親query0 | backdrop HTTRANSPARENT、親HTLEFT |
| 最大化の見える左client | 親HTLEFTで失敗 | 親HTCLIENT、子は透過しない |

最大化の点は実120DPI、screen `(0,250)`。旧GetWindowRectの値は未記録で、画面外frameとborderの特定px差を実測として断定しない。
親の独立監査は80代表snapshot/hash、8query、1failure、展開TUを照合した。追加製品runは0。

## 完成候補の限定確認

baseをmain6f20dccへffし、3候補fileのhash不変を確認した。#448のcore検索変更はこの更新に含むが、縁の製品差分ではない。
候補は3fileのformat/差分検査、正典Debug/tidyのincremental nenefolio build、派生fixture、新runtimeのgreen queryを実行した。
`static-format-01`、`build-green-01`、`derive-green-01`、`probe-green-01`はchild/wrapper0・記録error0。
buildは12actions、実OS120DPIのprobeは**73 checks / 0 failures**、既知遮断0。

対象は上記8点、左帯に隣接する内側1px、NOTEの左対照、drawer capture保持/解除、settings card内部、
実DialogBoxの取消/guard復帰、最大化/復元、synthetic負座標と96/120DPIのgeometry対照。
captureはfixtureのSetCapture契約、synthetic96DPIは実displayの96DPI操作と区別する。
製品build由来library/resource、4runtime、展開前後bytes、mapのTU単一性を照合し、元folio_window.c.objを重複抽出していない。
旧失敗attempt/runtime、旧/新raw/input snapshotとvectorを別に保持し、成功済みbaselineや無関係なunit/性能/OOMを再実行していない。

親は候補の54代表snapshotと8query/raw/展開bytesを独立照合し受理した。追加製品runは0。
成功greenの1334filesとfixture/driver bytesを凍結し、製品buildと73項目を繰り返さず次の開始境界へ進んだ。

## 合成操作からのnativeリサイズ開始・取消

別fixtureは復元BOTHの左1点だけを使い、実WindowFromPointでdrawer HTTRANSPARENT→root HTLEFTを確認してから、
返されたrootへWM_NCLBUTTONDOWNを一度送った。SC_SIZE/ENTER/EXITはfixtureから直接送っていない。
既存WndProc/DefWindowProcを通る実SC_SIZE(61441)→WM_ENTERSIZEMOVE→一回のtimer取消→WM_EXITSIZEMOVEを順に観測した。
25ms TimerProcはrootへWM_CANCELMODEとroot宛のqueued WM_LBUTTONUPを送り、native loopがcaptureを解放して同期呼出しへ戻った。
root生存、製品資源破棄、threadのdesktop復帰と専用desktop解放まで成功した。

`derive-resize-start-01` / `probe-resize-start-01`はchild/wrapper0・記録error0。実120DPI、**16 checks / 0 failures**。
成功した正典build/library/RCを再利用し、既存driverに明示fixture pathだけを加えた。同じ完全vector・4runtime・TU単一性を維持した。
親の40代表snapshot、rawの6event順序、capture非null→null、展開bytes、製品3hashの監査も成功した。
ENVは`syntheticWindowMessages=true`、`sendInput=false`、`sharedCursorApi=false`、旧73項目再実行falseを明記する。
物理mouse down/drag/upの配送や実寸法変更の測定ではない。

最終文書/File API確認、必須CI・統合はこの対象検証から別途確認する。
原本は`D:/NeNeFolio/agents/451-window-edge/`の`baseline-results.md`、`parent-baseline-acceptance.json`、
`parent-design-decision.md`、`candidate-01-self-review.md`、`parent-green-acceptance.json`、
`resize-start-plan.md`、`parent-resize-start-acceptance.json`とcandidate hash。各runは同名`outputs/451-window-edge/`。
確定後の統合/通常反映/保全整理は同taskの`integration-completion.json`へ収載する。

## 限界

専用非input desktopの同thread queryであり、共有cursor・SendInput・SwitchDesktop・foreground変更を使っていない。
親hitの直送だけを子を通る入力の証拠にせず、OS queryも物理buttonの配送と呼ばない。
実reader、物理IME、実96DPI display、monitor跨ぎ、通常data、公開betaは本検証の対象外。

## #451の統合・通常反映・保全

最終文書24参照、候補3hash、全inventory/File APIによるconformance違反0を親が受理した。
head`d3abd696f3ea6216d98cf11cab330624ca83944d`のPR #454は必須CI`40971869636`のfull-gate/check成功後、main`3ef671f`へ統合済み。
成功済み対象73checks/16checksは再実行していない。正規prepare-real-machineで08:23:35 JSTに通常exeへ反映し、起動/data操作は0。
5536files/855,482,924bytesを恒久archiveへ全hash照合し、未保存/未追跡/ignored/link/稼働参照なしを監査した。
worktree削除成功、branch/commit保持。出力削除は自動承認レビューが実行前`blocked by policy`で拒否し、追加理由は無かった。
再試行せず47箇所目として保持。確定原本は`agents/451-window-edge/integration-completion.json`、
SHA256 `31E4FB3ED3453722D1C8643C9F6F868A95367E71B2183F0BBE8BAA291573FCF3`。Waivers none。
