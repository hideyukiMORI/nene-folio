# いまの状態 — NeNe Folio

## 11月3日夜 — native Mermaid

hideの依頼で#539を開始した。今回の完成対象はフローチャートとシーケンス図。
本文を先に表示し、図を既存画像worker/取消/進捗/native差替えへ合流させる。
設計は[ADR0089](../adr/0089-native-lazy-mermaid.md)、対象検証と測定は[品質記録](../quality/2026-11-03-native-mermaid.md)。
22:30のhide指定でClaudeのdesignプラグインと[比較canvas](https://claude.ai/artifact/CxHhi1aBQuF2WGa9PAxnbe)を使い、
B案の部品/文字組みと追加相談した多行円柱の補正を反映した。対象core/native/確保失敗検証とRelease測定を記録した。
最終PR head・必須CI・main・通常反映/data照合・保全整理の確定原本は
`D:/NeNeFolio/agents/mermaid-20261103/integration-completion.json`。
22:33にhideのEscで画面操作を停止したため、その後の実出力確認は専用の描画テストで行った。
Store更新は今回の対象ではない。作業木/outputの最終所在も確定原本を参照する。

## 11月3日夜 — Store更新候補1.2.0

hideは19:24に言語ラベルと新アイコンを実機受理し、Microsoft Storeのパッケージアップロード画面へ進んだ。
#536で唯一の版入力を1.2.0（Windows版1.2.0.0）へ上げ、受理済み改善をStore提出原本へまとめる。
[変更一覧](../release/1.2.0.md)、[3言語の更新説明](../release/1.2.0-store-notes.txt)、
[品質記録](../quality/2026-11-03-store-release-1.2.0.md)を参照する。
最終PR head・必須CI・main・提出MSIX/hash・起動表示・保全整理の確定原本は
`D:/NeNeFolio/agents/release-1.2.0-20261103/integration-completion.json`。
Storeへの実際のアップロード・認定・公開は、候補生成と区別して新しい報告から記録する。
GitHubの公開版は1.1.0 Betaを維持する。

## 11月3日夕方 — 言語ラベルとアプリアイコン

hideの追加指定による#532のコード言語ラベルはPR #534、main `02060d3`へ統合済み。
fence info先頭語をコード面の左上に本文書体9pt、既存code_comment色で表示し、本文との間は4pt。
未知語・大小文字・aliasは入力どおり、未指定は表示せず、原文と保存schema・版1.1.0を保つ。
Claudeのdesignプラグインで[比較canvas](https://claude.ai/artifact/4hmz67GuzYjexqiXyScUoN)を作り、D案を採用した。
設計は[ADR0086](../adr/0086-fenced-code-syntax.md)、検証は[品質記録](../quality/2026-11-03-code-label.md)。
最終PR head・必須CI・main・通常反映・保全整理の確定原本は
`D:/NeNeFolio/agents/code-label-20261103/integration-completion.json`とする。

#533はClaudeの[比較canvas](https://claude.ai/artifact/BcA7CRegGBHzcQrZco58Lc)でアンバー地のB案を採用した。
角外の透過を持つ単一SVGから通常exe用ICOと57枚のMSIX画像を生成し、DPIごとの窓アイコンとPRI索引を追加する。
設計は[ADR0088](../adr/0088-application-icon-assets.md)、対象検証と限界は[品質記録](../quality/2026-11-03-app-icon.md)。
最終PR head・必須CI・main・#532と一緒の通常反映・保全整理は
`D:/NeNeFolio/agents/app-icon-20261103/integration-completion.json`を確定原本とする。
#533はPR #535、main `564b1f7`へ統合・通常反映・保全整理済み。
#532/#533の実機受理原本は両agentsのuser-acceptance.json。両作業木/outputは保全後に整理した。
これらの機能作業ではStore更新を行わず、上記#536で新たにStore提出原本を準備する。

#530の常設ステータスバーはPR #531、main `047c000`へ統合・通常版へ反映済み。
hideは16:09に「全部問題ない。」と実機受理した。受理原本は
`D:/NeNeFolio/agents/status-bar-20261103/user-acceptance.json`。
保全後の削除は自動承認審査に拒否されたため、`530-status-bar`のworktree/outputを保持する。
今回その削除を再試行しない。日報・引き継ぎ#528/PR #529は統合・整理済み。
追加waiverはnone。既存WVR-0001は継続する。

## 通常版と配布

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、通常dataは隣の`data`。
今回のStore準備前の通常版は製品source `564b1f7`、1.1.0。言語ラベル・アイコンまでhideが実機受理済み。
#532/#533の反映source/hash・data照合と起動状態は上記確定原本を参照する。
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
- 大文書のRichEdit投入/表配置、大量ノートの検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/範囲外として残る。
- WACKの総合WARNING、AppDataノートroot保持の未検証、Store署名版の導入は
  [MSIX品質記録](../quality/2026-11-01-msix-installation.md)と今回の引き継ぎを参照する。

既定は単体実行。古い停止/再開・モデル固定・一時許可は時点付きの参考情報として扱う。
