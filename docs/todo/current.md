# いまの状態 — NeNe Folio

## 11月3日夕方の続き

hideの追加指定で、#532のコード言語ラベルを実装する。
fence info先頭語をコード面の左上に本文書体9pt、既存code_comment色で表示し、本文との間は4pt。
未知語・大小文字・aliasは入力どおり、未指定は表示せず、原文と保存schema・版1.1.0を保つ。
Claudeのdesignプラグインで[比較canvas](https://claude.ai/artifact/4hmz67GuzYjexqiXyScUoN)を作り、D案を採用した。
設計は[ADR0086](../adr/0086-fenced-code-syntax.md)、検証は[品質記録](../quality/2026-11-03-code-label.md)。
最終PR head・必須CI・main・通常反映・保全整理の確定原本は
`D:/NeNeFolio/agents/code-label-20261103/integration-completion.json`とする。

続く#533はhideの指定でClaudeにClock/Loupeと揃うアプリアイコン制作を依頼している。
原本・canvas・明暗小サイズpreviewを確認後、通常exeとMSIX用画像を同じ原本から派生させる。
Storeへの公開/再提出やバージョン更新は依頼されていない。

#530の常設ステータスバーはPR #531、main `047c000`へ統合・通常版へ反映済み。
hideは16:09に「全部問題ない。」と実機受理した。受理原本は
`D:/NeNeFolio/agents/status-bar-20261103/user-acceptance.json`。
保全後の削除は自動承認審査に拒否されたため、`530-status-bar`のworktree/outputを保持する。
今回その削除を再試行しない。日報・引き継ぎ#528/PR #529は統合・整理済み。
追加waiverはnone。既存WVR-0001は継続する。

## 通常版と配布

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、通常dataは隣の`data`。
言語ラベル反映前の通常版は製品source `047c000`、1.1.0。ステータスバーまでhideが実機受理済み。
#532の反映source/hash・data照合と起動状態は上記確定原本を参照する。
次回も実稼働と保存状態を確認し、保存前の強制終了や起動中の上書きをしない。

GitHubの1.1.0 BetaとStore提出原本は11月1日のsource `8ef5d03`。
画像・構文色分け・今回のステータスバーを含む配布物の新規公開は行っていない。
Store 1.1.0は11月1日のhide報告「認定中」が最後の情報。認定合格・公開・実Store署名版導入は未確認。
常時監視は依頼されていない。詳細は[11月1日の引き継ぎ](../handoffs/2026-11-01.md)へ。

## 継続保留と保持する作業物

- #479の職場での文字ウェイト混在はhide指定で診断終了・修正保留。
  `D:/NeNeFolio/worktrees/479-editor-paste`と`D:/NeNeFolio/outputs/479-editor-paste`を未統合診断原本として保持。
- #466の表幅診断と停止済み継続品質改善は保留。
  `D:/NeNeFolio/worktrees/466-table-caller-width`と`D:/NeNeFolio/outputs/466-table-caller-width`を未完として保持。
- 旧削除拒否9作業木は再試行・別手段削除をしない。所在一覧は[今回の引き継ぎ](../handoffs/2026-11-03.md)へ。
  10月31日にhideが削除した47出力を古い保持記録だけで現存扱いに戻さない。
- 大文書のRichEdit投入/表配置、大量ノートの検索準備、極狭幅表、Mermaid等は未対応/範囲外として残る。
- WACKの総合WARNING、AppDataノートroot保持の未検証、Store署名版の導入は
  [MSIX品質記録](../quality/2026-11-01-msix-installation.md)と今回の引き継ぎを参照する。

既定は単体実行。古い停止/再開・モデル固定・一時許可は時点付きの参考情報として扱う。
