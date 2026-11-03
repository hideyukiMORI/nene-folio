# いまの状態 — NeNe Folio

## 11月3日午後の続き

hideの「日報と引き継ぎを読んで続ける」、続く「ステータスバー改善まで進める」指定に基づく作業。
日報・引き継ぎPR #529は必須CI成功後にmain `4586ef9`へ統合し、#528の追加作業物は保全整理済み。
原本は`D:/NeNeFolio/agents/handoff-20261103/integration-completion.json`。

#530は常設の下端バーに本文と異なる配色、保存ファイルの更新・作成日時、既存キー案内をまとめる。
日時はファイル属性であり、無題は未保存、取得失敗と改名途中は取得不能として扱う。
狭幅では表示を短縮し、全文は同じUIAとtooltipへ渡す。通知を優先し、消えたら通常情報へ戻す。
通常保存で作成時刻を保持し、本文と保存schema、表示版1.1.0は維持する。
設計は[ADR0087](../adr/0087-note-status-bar.md)、検証と限界は[品質記録](../quality/2026-11-03-note-status.md)。

最終PR head・必須CI・main・通常反映・data不変・保全整理の確定原本は
`D:/NeNeFolio/agents/status-bar-20261103/integration-completion.json`。
技術統合とhideの実機受理は分け、#530についてhideの受理返答はまだない。
新設タグ、新規コマンドパレット、Store/配布更新、保留案件の再開は今回の対象外。
追加waiverはnone。既存WVR-0001は継続する。

## 通常版と配布

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、通常dataは隣の`data`。
今回の反映直前までは製品source `c17928d`、1.1.0。03:31の画像・色分け版はhideが実機受理済み。
#530の反映source/hash・反映前後のdata照合と起動状態は上記確定原本を参照する。
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
