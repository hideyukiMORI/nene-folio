# いまの状態 — NeNe Folio

## 11月3日早朝の到達点

#520/#522/#524/#526の画像表示、画像を含むスクロール/ミニマップ、コード言語指定/構文色分け、
読込完了後の通知消去は必須CI成功・main統合・通常反映・保全整理まで完了した。
最新の製品sourceは`c17928d`、通常版は1.1.0。03:31に起動し、hideから「うん、良さそう」と実機確認の返答を受けた。
通常dataの「サンプル」に`シンタックスカラーサンプル.md`を追加済み。

成果と各PR/CIは[11月3日の日報](../reports/2026-11-03.md)、再開手順と原本は
[引き継ぎ書](../handoffs/2026-11-03.md)へ集約した。製品の最新確定原本は
`D:/NeNeFolio/agents/image-status-20261103/integration-completion.json`。
同原本の実機確認待ちは03:30時点の事実で、受理の追記は
`D:/NeNeFolio/agents/handoff-20261103/user-acceptance.json`を参照する。

hideは今夜の終了を予定し、日報・引き継ぎの作成を依頼した。追加機能の実装は始めない。
この文書task #528の最終head/必須CI/main/整理は
`D:/NeNeFolio/agents/handoff-20261103/integration-completion.json`で確定する。
文書3件のみ。製品・schema・版・依存の変更と追加waiverはnone。既存WVR-0001は継続する。

## 次回の検討候補

hideのステータスバー案: 本文と色を変える、更新日時/作成日時を置く、将来のタグや
ヘルプ/コマンドパレット等のショートカット説明を検討する。
表示項目・配置・実装範囲は未確定。タグやコマンドパレット自体の新設まで決定したものではない。
次回は新しいhide指示・正本・実Issue/PR・checkoutを照合して範囲を決める（ADR0043）。

## 通常版と配布の違い

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。製品source `c17928d`、版1.1.0。
03:28の反映時はサンプル追加後のdata32ファイル不変。起動原本は
`D:/NeNeFolio/agents/image-status-20261103/normal-launch.json`。
日報作成時はPID18372で稼働中。次回は実状態を確認し、保存前の強制終了や起動中の上書きをしない。
文書統合だけで通常版を再ビルドしない。

GitHubの1.1.0 BetaとStore提出原本は11月1日のsource `8ef5d03`。
今回の画像・構文色分け等を含む配布物の新規公開は行っていない。
Store 1.1.0は11月1日のhide報告「認定中」が最後の情報。認定合格・公開・実Store署名版の導入は未確認。
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
