# NeNe Folio アプリアイコン

#533、2026-11-03。hideの指定でClaudeのdesign:design-critique/design-handoffを使用し、
[比較canvas](https://claude.ai/artifact/BcA7CRegGBHzcQrZco58Lc)のB案を採用した。
Clock/Loupeとの並列比較と明暗の16〜256px表示を確認した。制作モデルはclaude-fable-5-1。

![採用アイコン](../../packaging/windows/Square44x44Logo.targetsize-256_altform-unplated.png)

正本は[app-icon.svg](../../packaging/windows/app-icon.svg)。256グリッド、外周角丸24。
Clockと同じアンバー#D08C3F、クリーム#F5F2EB、チャコール#1A1917を使う。
紙面・背表紙・3本の罫線・右側の分類タブでノートを表す。外側余白は設けない。
明るい背景では細い縁、暗い背景ではアンバーの面で輪郭を保つ。

16〜32pxの角画素は丸みの面積に応じた部分alpha、48px以上の角画素はalpha 0となる。
16pxで細い罫線は弱くなるが、紙面と背表紙を残す。サイズ別の別作画は使わない。
通常exeのICOとMSIXのscale/targetsize/既定/unplated/lightunplatedを同じSVGから派生させる。
生成・所有・PRIの方針は[ADR0088](../adr/0088-application-icon-assets.md)、確認結果は[品質記録](../quality/2026-11-03-app-icon.md)。

canvas原本・比較・Claudeの引き継ぎは`D:/NeNeFolio/design/app-icon-20261103/`。
今回の通常反映と保全整理は`D:/NeNeFolio/agents/app-icon-20261103/integration-completion.json`を参照する。
Storeで公開済み/申請済みの画像は今回のローカル実装だけでは更新されない。
