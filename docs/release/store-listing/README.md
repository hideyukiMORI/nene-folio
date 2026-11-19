# Microsoft Storeの掲載原稿

商品: [NeNe Folio](https://apps.microsoft.com/detail/9PCHS225SWBX)（9PCHS225SWBX）。
**現在の正本は1.5.0の機能に合わせた案（2026-11-19、[Issue #1000](https://github.com/hideyukiMORI/nene-folio/issues/1000)）で、Storeで公開されている。**
1.5.0（2026-11-19にGitHubで公開）の更新と一緒に、hideが同日21時ごろに申請し（[Issue #1065](https://github.com/hideyukiMORI/nene-folio/issues/1065)）、同日23時台に公開を報告した（[Issue #1083](https://github.com/hideyukiMORI/nene-folio/issues/1083)）。
2026-11-20に公開カタログ（ja-JP・en-US・zh-CN）を取得し、説明・短い説明・機能と「このバージョンの新機能」（[1.5.0更新文](../1.5.0-store-notes.txt)）の1.5.0の段落が、この原稿と空白を除いて一致することを確かめた（掲載の最終更新は2026-11-19T13:34:54Z。[品質記録](../../quality/2026-11-19-release-1.5.0.md)）。
1.4.0と1.5.0の機能（お気に入り・タグ・ノートを探す面・見出しの欄・本文の右クリック・窓の位置・定義リストと脚注・状態遷移図/ER図/クラス図/円グラフ/ガント）を足し、
「クラス図やガントチャートなど他の種類の図には対応していません」を、対応する種類の主な記法に対応し、それ以外と図のスタイル指定には対応しない、という書き方へ改めた。
ガントは今日の日付を使わない（開始日を書いた作業から描く）ことも書いた。検索キーワード7語は変えていない。字数は日本語1391・英語2768・中国語1037文字（説明）、短い説明117・251・87文字。
1.5.0を出さずに1.4.0のまま申請すると、見出しの欄・円グラフ・ガントなど未公開の機能を案内してしまうので、この案は1.5.0と同時にだけ使う。

以下はそれ以前の経緯。1.3.2基準の原稿は2026-11-12の訂正案で、Mermaidの図を2番目の柱として案内した
（[Issue #803](https://github.com/hideyukiMORI/nene-folio/issues/803)）。
hideが2026-11-12に[この訂正案を申請したと報告した](https://github.com/hideyukiMORI/nene-folio/issues/805)。認定と公開ページへの反映は未確認。
最後に公開ページで照合したのは2026-11-07の原稿で、hideが2026-11-10に[変更を申請したと報告し](https://github.com/hideyukiMORI/nene-folio/issues/797)、
同日に[反映を確認した](https://github.com/hideyukiMORI/nene-folio/issues/801)。認定日時は未確認。
Storeの現在版番号や1.3.2の認定・公開を示すものではない。

## 正本と入力欄

掲載文の正本は[日本語](ja-JP.json)、[English](en-US.json)、[简体中文](zh-CN.json)の3ファイル。
各言語で次の欄を置き換える。JSON自体はPartner Centerのインポート形式ではない。

| 正本のキー | Storeの欄 | 入れ方 |
| --- | --- | --- |
| `description` | 説明 / Description | 文字列の本文全体。JSONの引用符やescapeは入れない |
| `features` | 製品の機能 / Product features | 配列の各項目を一つずつ入力。箇条書き記号は足さない |
| `shortDescription` | 短い説明 / Short description | 文字列の本文全体 |
| `keywords` | 検索キーワード / Search terms | 配列の各項目を一つずつ入力。既存の語を確認してから入れ替える |

貼り付け用のUTF-8テキストは、同じ原稿から
`D:/NeNeFolio/releases/1.3.2/store-listing-20261112/{ja-JP,en-US,zh-CN}/`へ生成した。
`description.txt`、`features.txt`（1行1項目）、`short-description.txt`、`keywords.txt`（1行1語）を使う。
`source-manifest.json`で正本のcommit・各原稿と出力のSHA256を照合できる。派生テキストを直接編集しない。
公開中の原稿の貼り付け用テキストは`D:/NeNeFolio/releases/1.3.2/store-listing-20261107/`に残す。

`keywords`は2026-11-12に加えた。1.0.0提出時の素材（`D:/NeNeFolio/releases/1.0.0/store-listing-20261031/`）にある7語のうち、
各言語で2語を「Mermaid」と「フローチャート」に当たる語へ入れ替えた案で、語数は7のまま。
Partner Centerに現在入っている語は確認していない。入力の前に画面の語と照らす。

「このバージョンの新機能」は別欄で、既存の[1.3.2更新文](../1.3.2-store-notes.txt)を使う。
商品名・画像・サポート/プライバシーURL・packageは訂正対象ではない。Mermaidの図を写した画像の追加は別の作業とする。
旧1.0.0の掲載素材は日付付き履歴として維持する。原稿には未公開のCJK書体修正を混ぜない。

## 根拠と制限

[Markdownの対応範囲](../../MARKDOWN.md)と[取り込み・書き出し](../../TRANSFER.md)に合わせ、PNG/JPEGのローカル/HTTPS画像、
Mermaidのフローチャート・シーケンス図の対応部分、対応言語の基本的なコード色分けを案内する。
Mermaidは独立した節と機能の2番目の項目に置き、テーマの配色への追従、本文を先に表示すること、
スクリプトを実行せず外部へ通信しないこと、原文を保つことを書く。対応は2種類の図の一部の記法で、他の種類の図は非対応と明記する。
HTML描画・独自クラウド同期は非対応のまま。HTTPS画像の外部アクセスと、ZIPが設定/履歴のバックアップにならない点も明記した。

[MicrosoftのMSIX掲載欄ガイド](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/add-and-edit-store-listing-info)に従い、
説明10,000文字、機能1項目200文字・20項目以内、短い説明1,000文字以内を確認する。短い説明は推奨の270文字未満に収めた。
説明にはHTML・コード例・URLを入れない。これらは掲載原稿の確認であり、アプリの実行時schemaや品質ゲートの変更ではない。

投稿前のレビュー・Store側への入力/申請と、公開後の掲載反映は別段階。
根拠は[2026-11-07の作成・照合の記録](../../quality/2026-11-07-store-listing-current.md)と
[2026-11-12のMermaid訴求の記録](../../quality/2026-11-12-store-listing-mermaid.md)に残す。
