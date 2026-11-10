# Microsoft Storeの掲載原稿

商品: [NeNe Folio](https://apps.microsoft.com/detail/9PCHS225SWBX)（9PCHS225SWBX）。
公開済み1.3.2の機能を基準に、2026-11-07に準備した訂正案。
hideが2026-11-10に[変更を申請したと報告した](https://github.com/hideyukiMORI/nene-folio/issues/797)。同日、公開Storeページへの[反映を確認した](https://github.com/hideyukiMORI/nene-folio/issues/801)。認定日時は未確認。
Storeの現在版番号や1.3.2の認定・公開を示すものではない。

## 正本と入力欄

掲載文の正本は[日本語](ja-JP.json)、[English](en-US.json)、[简体中文](zh-CN.json)の3ファイル。
各言語で次の欄だけを置き換える。JSON自体はPartner Centerのインポート形式ではない。

| 正本のキー | Storeの欄 | 入れ方 |
| --- | --- | --- |
| `description` | 説明 / Description | 文字列の本文全体。JSONの引用符やescapeは入れない |
| `features` | 製品の機能 / Product features | 配列の各項目を一つずつ入力。箇条書き記号は足さない |
| `shortDescription` | 短い説明 / Short description | 文字列の本文全体 |

貼り付け用のUTF-8テキストは、同じ原稿から
`D:/NeNeFolio/releases/1.3.2/store-listing-20261107/{ja-JP,en-US,zh-CN}/`へ生成した。
`description.txt`、`features.txt`（1行1項目）、`short-description.txt`を使う。
`source-manifest.json`で正本のcommit・各原稿と出力のSHA256を照合できる。派生テキストを直接編集しない。

「このバージョンの新機能」は別欄で、既存の[1.3.2更新文](../1.3.2-store-notes.txt)を使う。
商品名・画像・キーワード・サポート/プライバシーURL・packageは今回の訂正対象ではない。
旧1.0.0の掲載素材は日付付き履歴として維持する。今回の原稿には未公開のCJK書体修正を混ぜない。

## 根拠と制限

[Markdownの対応範囲](../../MARKDOWN.md)と[取り込み・書き出し](../../TRANSFER.md)に合わせ、PNG/JPEGのローカル/HTTPS画像、
Mermaidのフローチャート・シーケンス図の対応部分、対応言語の基本的なコード色分けを案内する。
HTML描画・独自クラウド同期は非対応のまま。HTTPS画像の外部アクセスと、ZIPが設定/履歴のバックアップにならない点も明記した。

[MicrosoftのMSIX掲載欄ガイド](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/add-and-edit-store-listing-info)に従い、
説明10,000文字、機能1項目200文字・20項目以内、短い説明1,000文字以内を確認する。短い説明は推奨の270文字未満に収めた。
説明にはHTML・コード例・URLを入れない。これらは掲載原稿の確認であり、アプリの実行時schemaや品質ゲートの変更ではない。

投稿前のレビュー・Store側への入力/申請と、公開後の掲載反映は別段階。
[作成・照合の記録](../../quality/2026-11-07-store-listing-current.md)に根拠を残す。
