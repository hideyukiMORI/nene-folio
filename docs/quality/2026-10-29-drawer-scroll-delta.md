# ドロワー差分の加算前飽和 — 2026-10-29

Issue: [#434](https://github.com/hideyukiMORI/nene-folio/issues/434)。base main `0803ff539efd9ed3d178dde804e5859eb665b3ef`。
規則: ARC-001/004/005/011、C-012/016、QLT-008/012/013、GIT-001〜004。
[ADR0009](../adr/0009-drawer-scroll-and-overflow-fade.md)決定3の有効量から数え直す契約を回復する。Waivers: none。

## 修正と範囲

`folio_state_scroll_drawer`は有効位置へdeltaを加算した後で丸めていたため、上限40・有効位置15の正常な小さい索引でも、
公開int deltaにINT_MAXを渡すと加算が先にoverflowする。通常のwheel操作で自然到達したことを確認した問題ではない。

同じ関数で有効位置を求め、deltaを`[-effective, limit-effective]`へ先に丸めてから加算する。
`0 <= effective <= limit <= INT_MAX`で、両境界と最終和はint内にある。INT_MINそのものを負号反転しない。
layout/coreの上限・表示座標・UI変換・状態所有・失敗時の状態保持は維持し、新API/型/owner/確保/依存を加えない。
巨大metrics/行数によるlayout算術は対象外。

## fixtureと旧source red

既存`scroll_metrics`、`scrolled_top`、`verify_scroll_rounding/recount/scroll`を共用する。
通常の15/0/100/10/-1000と、折畳後の有効位置10から数え直す確認、選択/並替で位置5を保つ確認を残した。
新しい`verify_scroll_delta_bounds`は15→INT_MAXで40、INT_MINで0、exact±40、上端で-1/0を確認する。
`run_drawer_scroll_delta_tests`はfinderを初期化し、fullrunと`--drawer-scroll-delta`が同じfixtureを使う。

初回は新規assertを既存roundingへ追加したことで認知的複雑度11となり、上限10を超えてclang-tidyで停止した。
初回自己点検で追加した`&&`の合計を見落とした。build child1、exe/application.libの未生成artifactで記録wrapper125。
selectorは未実行であり、このbuild失敗はUBSanのredではない。閾値/設定/抑制は変更していない。
新規int境界7assertを意味単位へ閉じ、既存roundingを元に戻したfixtureを親が静読受理した。
新runのincremental build/tidyは0で、成功済みobjectを再利用した。

旧productionでselectorを一度実行すると、`folio_state.c:988:59`に
`15 + 2147483647 cannot be represented in type 'int'`のsigned integer overflowとUBSan summaryが出た。
child/wrapper1、recordingErrors0、250指定inputは前後不変。
INT_MIN以降、折畳後の再計算、選択/並替確認は、このredでは最初のUBSan停止により未実行。
親がraw、全input/current/snapshot hash、正典compile vectorを受理してからproductionを編集した。

## 実行記録

全診断は新しいrun-dirの`tools/record-run.py`を使い、#432の`--input-list`で指定入力を渡す。
解析した一覧bytes自身も先頭inputとして保全する。外側stdout/stderr/終了値も分離し、起動失敗にstale LASTEXITCODEを使わない。
File API queryはconfigure前に作り、同じPowerShell processで正典toolchainを読んでからcmakeを呼ぶ。
Pythonは-BとPYTHONDONTWRITEBYTECODEを使う。

| run | 子 / wrapper | 範囲・結果 |
| --- | --- | --- |
| configure-red | 0 / 0 | Debug、codemodel-v2 query先行 |
| build-red | 1 / 125 | unitのcomplexity11で停止、未生成artifact2件、selector未実行 |
| build-red-retry | 0 / 0 | fixture分割後のincremental Debug/tidy |
| red-intmax | 1 / 1 | 旧加算のUBSan、記録error0 |
| format-candidate | 0 / 0 | 非破壊formatter出力を保存後に4fileへ適用 |
| build-green | 0 / 0 | 最終2ifの波括弧を含むDebug/tidy |
| green-scroll-delta | 0 / 0 | 同selector全対象成功、stderr空 |
| format-final | 0 / 0 | 担当C/H 4fileのdry-run/Werror |
| conformance-final | 0 / 0 | File API付き、Conformance 0 |
| diff-final | 0 / 0 | tracked差分check、stdout/stderr空 |

中心となるchild commandは次のとおり。完全なargv/input/artifact/runtimeは各run.jsonとagentsのcommandsへ収載する。

```powershell
pwsh -NoProfile -File D:/NeNeFolio/agents/434-drawer-scroll-delta/canonical-build.ps1 -Stage Configure -BuildDirectory D:/NeNeFolio/outputs/434-drawer-scroll-delta/build-debug
pwsh -NoProfile -File D:/NeNeFolio/agents/434-drawer-scroll-delta/canonical-build.ps1 -Stage Build -BuildDirectory D:/NeNeFolio/outputs/434-drawer-scroll-delta/build-debug
D:/NeNeFolio/outputs/434-drawer-scroll-delta/build-debug/folio_tests.exe --drawer-scroll-delta
```

compile-vector.jsonにはDebug compile_commandsのapplication/test3件をそのまま保存し、全compile_commandsも各runのinputに含めた。
ASan/UBSan/nullabilityとnorecoverを正典Debug設定から使用し、手製のsanitize-only代替は作らない。
旧source/exe、初回build失敗と各snapshot/raw/child-wrapper exitは新名の原本を保つ。
環境は正典toolchainで照合したLLVM 19.1.5、CMake 3.31.6-msvc6、Ninja 1.12.1、Python 3.12.10、MSVC 14.44.35207。

実行結果: 旧redのUBSanに対し、完成候補のDebug/tidyと同selectorは成功。親のhash照合・文書確定後の最終format/File API conformance/diffも全てchild/wrapper0だった。code/testsは凍結し、品質記録の結果欄だけを報告更新した。成功対象は再実行しない。
build-greenは16:22:27〜16:22:48 JST、253inputs/3artifacts、child/wrapper0、recordingErrors0。
green-scroll-deltaは16:23:26〜16:23:27 JST、250inputs、child/wrapper0、recordingErrors0、stderr0bytes。
時刻は各run.json/result.jsonのUTC原値からJSTへ写したもので、性能測定ではない。

## 保全と残る範囲

原本は`D:/NeNeFolio/agents/434-drawer-scroll-delta`、試行とDebugは`D:/NeNeFolio/outputs/434-drawer-scroll-delta`、
作業木は`D:/NeNeFolio/worktrees/434-drawer-scroll-delta`。完成時にcommands/results/hashes/finalsource/self-review/completionを閉じる。
新確保/Win32変更がないためlocal OOM/native/検索/全unit/fullgate/性能試験は行わない。
単位fixtureは公開int引数の合成境界であり、通常UIの自然な障害再現や実機描画の確認とは呼ばない（QLT-013）。

必須CI/統合/通常exe反映/恒久全hash保全/安全監査/整理は親担当。担当はcommitpush/PR/削除をしない。
未統合の出力と旧失敗原本を含むため、親が統合・必要反映・全保全後に絶対path、取込、未保存/ignored/唯一成果物、
稼働参照とリンクを監査するまで新作業物を保持する。branch/commitは保持する。通常data/Nib・削除拒否38pathは不操作。
