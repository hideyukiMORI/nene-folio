# Store掲載文を公開版へ揃える（#787）

## 根拠と対応

hideの継続指示に基づき、#785完了後の次対応として着手した。
商品9PCHS225SWBXの公開英語説明は、PNG/JPEG画像・Mermaid・コード色分けを非対応とする1.0.0時点の内容だった。
公式msstoreのraw出力はD:/NeNeFolio/agents/readme-current-20261107/runs/store-show-01/stdout.log。
旧3言語原稿はD:/NeNeFolio/releases/1.0.0/store-listing-20261031/。過去の提出素材として変更しない。

新しい正本は[掲載原稿](../release/store-listing/README.md)の3言語JSON。
説明・機能一覧・短い説明だけを維持し、JSONをStoreインポート形式とは扱わない。
公開1.3.2の[Markdown](../MARKDOWN.md)、[転送](../TRANSFER.md)、[公開変更一覧](../release/1.3.2.md)と照合した。
PNG/JPEGのローカル/HTTPS表示、flow/sequenceの対応部分、対応言語の基本的なコード色分けを3言語で揃えた。
HTML描画・独自クラウド同期非対応、HTTPS画像の外部通信、ZIPが設定/履歴のバックアップにならない点も一致させた。
簡体字のミニマップはui_text.cの「缩略图」に合わせた。
原稿に未公開CJK書体修正の更新説明、全Mermaid/全画像形式対応、Store1.3.2公開済みという主張を入れない。

## 掲載欄と派生物の確認

[MicrosoftのMSIX掲載欄ガイド](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/add-and-edit-store-listing-info)を2026-11-07に確認した。
説明10,000、機能1項目200・20項目以内、短い説明1,000文字以内。短い説明は推奨270文字未満とする。
説明にHTML・コード例・URLを入れず、機能はStore側が箇条書き表示するため記号を付けない。

`export-listing.py`は確定したcleanなtask headの3原稿をgit blobと照合し、未作成の
D:/NeNeFolio/releases/1.3.2/store-listing-20261107/へ9テキストとsource-manifest.jsonを生成する。
元の3キーをそれぞれdescription.txt、features.txt（1行1項目）、short-description.txtへ渡し、全欄の一致とSHA256を記録する。
原稿を変更するときは正本JSONを編集し、新しい日付/出力先へ生成する。派生テキストを別の正本にしない。
既存の1.3.2-store-notes.txtは「このバージョンの新機能」の正本として維持する。

対象検証は文書差分・相対参照、3言語のJSON/文字数/8機能/共通の対応範囲、生成テキスト全欄とhash、製品入力/通常exe不変、conformance。
実行結果と文字数はD:/NeNeFolio/agents/store-listing-current-20261107/docs-check.json・delivery.json・runs/に記録する。
ローカル製品build/起動/機能試験は行わず、最終head必須CIを統合条件とする。

## 前対応の確定結果と限界

#785/PR786最終headf620fd1a24dc7ef42e5802067fe2b5f626beba6c、CI42245044660 full-gate/check成功、main445315bへ統合済み。
1,268files全保全後に専用worktree/outputを整理した。確定原本同agents/readme-current-20261107/integration-completion.json。
check-01の入力一覧誤りは検査子起動前、merge-01の本文必須欄不足はReady前に停止した。
それぞれ記録を保持して訂正し、check-02/merge-02が成功。旧Draft CIのfailureを最終CIと混同しない。

この対応は未送信の原稿準備。Partner Center入力/送信、packageや既存Store画像の変更は行わない。
Storeの現在版番号、1.3.2審査/公開、訂正文の掲載は未確認。
FR-014、QLT-008/012/013、CNF-006、GIT-001〜004、ADR0043/0045/0080。追加waiver none、既存WVR-0001継続。
アプリのコード/API/保存schema/版/ゲートに変更なし。原稿JSONは文書用の新しい管理対象で、アプリは読み込まない。
統合・成果物保管・作業領域整理の確定原本は同agents/store-listing-current-20261107/integration-completion.json。
