# 破線の端点記号を連続した輪郭で描く — #587

破線pathを描くpenを矢頭/円/×にもそのまま使っていたため、端点記号の輪郭に切れ目が出た。
4方向・両端・明暗のnative比較40条件中、記号付き32条件で計536画素の差を製品未変更で再現した。
既存canvas内で端点記号だけ同色・同幅の実線penへ切り替え、同じ試験の差は0になった。

## 根拠と正典経路

最初に`-)`/`--)`、`-x`/`--x`、`->>`/`-->>`の6sequence入力を実rendererで明暗12画像にした。
右向き・左向き・自己messageを各図に含み、全てIMAGE_READY、同対の寸法一致。開矢頭の実線/破線明、×破線暗を視認した。
初期原本は`D:/NeNeFolio/agents/diagram-marker-observe-20261104`、出力も同名のoutputs。
参照版[Mermaid 12.2.0 svgDraw.js](https://github.com/mermaid-js/mermaid/blob/mermaid@12.2.0/packages/mermaid/src/diagrams/sequence/svgDraw.js)の
crossheadはstroke-dasharrayを0,0に設定する。[styles.js](https://github.com/mermaid-js/mermaid/blob/mermaid@12.2.0/packages/mermaid/src/diagrams/sequence/styles.js)では
messageLine1の破線をmarkerと別に定義している。取得本文/blob SHA/URLはupstream-references.jsonへ保存。上流実行画像比較ではない。

main `a70f016`からD側の`587-solid-markers`作業木を作り、native試験で失敗を確認後、製品編集前にADR0089へ方針を追記した。
diagram_canvasのcreate_penは色と線種を受け、既存style/幅/dashの定義を一度だけ使う。
draw_pathは従来のpenでshaftを描き、draw_markersは破線かつ記号がある場合だけ実線penを局所所有する。
記号の形・位置・大きさ・塗りは同じdiagram_native_shape_arrowsを呼び、選択を復元してpenを破棄する。
追加penの確保失敗はIMAGE_OUT_OF_MEMORYをdraw_primitive→canvasへ伝え、途中bitmapは公開しない。

実線/太線/無記号には追加penを作らない。scene/layout/構文/文字/配色/API/schema/版/依存/上限/gate変更なし。
ARC-001/003/010/012、C-002/004/012/016、QLT-008/010/012を自己レビューし、既存adapterの所有・失敗経路に閉じると確認した。
小さなnative修正であり追加の独立レビューは行わない。追加waiverはnone、既存WVR-0001はWinHTTP callbackのみ。

## 対象試験と結果

nativeの`--markers`を追加し、引数なしの通常CIにも含めた。none/open/cross/closed/circleの5種×4方向×明暗40条件で、
両端に同じ記号を置いた160×160のsceneを実線・破線・太線で各一回描く。各版120bitmap。
実線と破線のshaft中心±1pixelを除く全領域を比較し、shaft中央50pixelには同色dashと背景gapが残ることを要求する。
GDI object数は40条件の前後で一致を要求する。これはprocess内の正味GDI数でありheap leak全域や最大使用量の証明ではない。

| run | コマンドと結果 |
| --- | --- |
| 初期link-01 | 不存在toolchain.lock.jsonを入力一覧へ入れたため子起動前に記録側125。訂正前の証跡を保持 |
| 初期link-02 / observe-01 | 正規lld/runtime条件でlinkし6合成入力×明暗12画像、子/記録側0 |
| build-red-01 | 固定Debug/tidy。画素比較の複雑度12/上限10で子1。artifactを誤ったbuild/Debugへ指定したため記録側125 |
| build-red-02 | 画素を一回走査へ整理しCI既定選択/GDI数確認を追加、artifact path訂正。製品未変更、子/記録側0 |
| red-01 | `diagram_render_tests.exe --markers <red-bitmaps>`。120画像後、32条件/536画素差で子/記録側1。shaft判定成立 |
| build-green-01 | 同じbuild-target.ps1で変更adapterをbuild/link、子/記録側0 |
| native-green-01 | 同じ入力/selector、1529checks成功。40条件で差0、shaft判定成立、GDI数復帰、子/記録側0 |
| link-samples-01 / samples-01 | 現行Debug3libと既存native helperで同じ6sequence入力×明暗12画像、子/記録側0 |
| verify-01 | clang-format、conformance0、symbols2library/違反0、git diff --check成功。子/記録側0 |
| audit-01 | 前後marker240fileとsequence24fileを保存SHAへ再照合、子/記録側0 |

対象runはtools/record-run.pyで実行し、入力snapshot・raw stdout/stderr・終了値を保存した。
記録エラーは上記の誤指定2runのみ。成功runのstderrはverify-01のCRLF→LF案内だけで、sanitizer診断なし。
比較用compare-bitmaps.pyは直接実行した診断として保存し、そのscript/report/全bitmapをaudit-01の入力snapshotに収載して再hash照合した。
128等の別件試験は再実行せず、変更したcanvasと直結する画素/資源境界だけを確認した。

markerの実線/太線80画像と無記号破線8画像、計88対照は前後の全SHA一致。32画像の858変更画素は端点±12pixel以内、範囲外0。
修正前の536画素は「実線対破線のshaft外差」、858画素は「前後の全画素差」であり異なる指標。
sequenceは全12組の寸法一致、実線6画像の全SHA一致、破線6画像だけ変更。
修正後の横開矢頭明/縦×暗の2図と、sequence開矢頭明/×暗の2図を視認した。

## 証跡・文書・限界

製品1file、native試験1file、ADR/品質記録/gate-proofs/日報/引き継ぎ/currentの6文書を変更。
恒久原本は`D:/NeNeFolio/agents/587-solid-markers`。初期観測も保全ZIPに含め、全hash照合後に整理する。
GIT-001〜004とQLT-008/012に従い、最終head必須CI/正典統合/通常exe反映/data照合/整理の確定値はintegration-completion.jsonへ収載する。
診断時の不存在path読取、PowerShell globのrg誤指定、toolchain前のclang-format不在、GitHubconnector作成403もjournalへ記録し、製品検証成功には数えない。
GitHub作成は認証済みghとbody-fileへ切り替えた。

同じ色・幅での輪郭修正であり、任意角度/DPI/書体/任意図の衝突や実UIの受理は保証しない。
追加pen確保失敗はコードで伝播を確認したが強制注入していない。破線記号付きpathごとにpen一つを追加取得/解放し、性能は未計測。
実app/実ノートを試験へ使わず、Store再提出/公開は行わない。
