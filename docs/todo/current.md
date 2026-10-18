# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの推奨案反映指示に基づく[#344](https://github.com/hideyukiMORI/nene-folio/issues/344)。
新規ノートを共通ヘッダー左へ集約し、本文の保存・操作・ヘルプをパンくず/通常本文と同じ36px基準へ揃えた。
狭い一覧窓は紙＋の図形になり、tooltipとUIA名を維持する。ADR0059を実装前に固定。
Debug/tidy・対象unit・実3DPI×3言語×3表示・keyboard/UIA/capture/無題取消が成功。
[品質記録](../quality/2026-10-18-header-layout.md)とD:/NeNeFolio/agents/header-layout-20261018/を参照。
最終CI/main・通常exe・更新候補・起動/整理の実結果は同所completion.jsonを確認する。

#340/#342はPR341/343で統合済み、基点main89d8a81。編集行間/行番号・検索placeholder・検索focus保存を継承。
新しい確認先はD:/NeNeFolio/check/2026-10-18/344-header-refresh/。稼働中の旧候補やユーザーデータは操作しない。
物理IME 1はhideの暫定所感、3は未確定中切替なし。人の読み上げ聴取は未受理。
カテゴリ色はテーマと独立した識別色。最初の候補の青は確認データへサナが指定した色であり、仕様変更なし。
公開版v0.1.0/候補0.2.0。公開tag/Releaseなし。schema・依存・ゲート変更なし、Waivers: none。
以前の一時物整理の残留は各completion.jsonを参照し、拒否済み操作を再試行しない。
