# いまのタスク — NeNe Folio

記録日: 2026-10-17。#312開始mainは8ffb9bcb9e64e1f131642cd38eebfad917a0d019、公開tag v0.1.0。
hideの「次の対応を進めて」で、#312のアクセシビリティ/OS高コントラストの現状測定と範囲設計を進めた。
現在の対話席が単体で調査・設計・測定し、必要な独立設計レビューだけ読み取り専用で依頼した。

## 今回の焦点と最終照合

- [#312](https://github.com/hideyukiMORI/nene-folio/issues/312)の範囲・正典経路・受理条件を[ADR0055](../adr/0055-accessibility-and-system-contrast-scope.md)へ収載。
  自前ヘッダ/索引のUIA、OS配色、補助画面を別Issueにした。製品実装はすべてplanned。
- 隔離fixtureで既存exeの操作/取得情報を測定。本文/検索値は取得できたが、行/操作/設定項目は取得treeにない。
  OS高コントラストはOFFを読取確認。reader発話と実OS ON/OFFは未測定。詳細は[品質記録](../quality/2026-10-17-accessibility-observations.md)。
- 最終head必須CI・統合・証跡収載・専用作業物整理は、#312の関連PRと
  D:/NeNeFolio/agents/312-a11y-scope/completion.jsonで照合する。
  MERGED/CLOSEDと整理成功が揃えば#312の範囲設計は完了として再開しない。

## 次の入口

- 最初の実装は[#319](https://github.com/hideyukiMORI/nene-folio/issues/319): 基本操作のUIA公開とキーボード到達。
  C/COM寿命・threading・native子窓の結合を小さく実測し、applicationのcursorと文書選択を分ける契約を守る。
- [#320](https://github.com/hideyukiMORI/nene-folio/issues/320): OS高コントラスト。#319から独立して実装できる。
- [#321](https://github.com/hideyukiMORI/nene-folio/issues/321): 設定/候補/検索置換/通知。#319の公開基盤を再利用する。
  後続の実装やreader/OS試験を、#312の設計完了と同一視しない。
- #311は[PR #318](https://github.com/hideyukiMORI/nene-folio/pull/318)、#310は[PR #317](https://github.com/hideyukiMORI/nene-folio/pull/317)でCI・main・通常exe反映・専用作業物整理まで完了。
  詳細は[日報](../reports/2026-10-17.md)・[引き継ぎ](../handoffs/2026-10-17.md)と各completion.json。
- 性能、portable/installer、UI分割、定期draft退避、起動temp回収、公開v0.1.0差替え、#243、旧作業木整理は別件。

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe（#311反映済み）。#312は文書だけで再build/更新しない。
対象検証の不変な結果は再利用する（QLT-012）。schema・依存・ゲート変更なし。Waivers: none。
