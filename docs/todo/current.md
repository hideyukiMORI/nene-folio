# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの0.2.0候補への実機フィードバックを単体で修正中。
[#340](https://github.com/hideyukiMORI/nene-folio/issues/340)で編集行間・行番号・新規ノートボタン・検索欄の表示を修正した。
対象build/RTF/native DPI/操作検証は成功。[品質記録](../quality/2026-10-18-editor-layout.md)を参照。
必須CI・main統合・整理はD:/NeNeFolio/agents/editor-layout-20261018/completion.jsonへ実記録する。

次は合意済みの検索focus保存: 名前付きの変更本文を既存保存経路で保存してから検索する。
UNTITLEDは保存先を尋ねず検索へ進み、検索対象外を維持する。保存失敗時には本文を保つ。
通常exeと新しい配布候補はこの連続修正後にまとめて反映し、hideが編集中の#338候補を上書き/終了しない。

物理IMEはhideの回答で1が暫定良好、3が未確定中に切り替わらないことを確認。人の読み上げ聴取は未受理。
カテゴリの青は候補fixtureに設定した識別色であり、テーマ追随の仕様変更はしていない。
公開版v0.1.0/候補0.2.0。公開tag/Releaseは作成しない。schema・依存・ゲート変更なし、Waivers: none。
成功済みの無関係なcore/OOM/UIA/reader/性能を再実行しない。
以前の一時物整理の残留は各完了記録を参照し、拒否済み操作を再試行しない。
