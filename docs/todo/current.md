# いまの状態 — NeNe Folio

## 2026-11-06 — 置換2欄保存まで完了、カテゴリ間dragの編集保持を確認中

hideの新しい依頼により、停止指示まで品質対応を継続する。試験・計測・必要な調査と独立レビューを行い、全試行と変更を記録する。
最優先の#479はPR708の最終CI42122814552が成功し、main `50fa28ea6ebc120023f1bbd30a753f9c3a5e9ab3`へ統合した。
既存RichEditの編集/Undo経路で文字形式だけを受け入れる。対象414checksと直接影響する画像保存28checksが成功。
通常反映と実data32file不変、11811filesの全保全・作業木/旧新outputの整理まで完了し、branch/commitは保持した。
[品質記録](../quality/2026-11-06-editor-paste.md)。確定原本はD:/NeNeFolio/agents/479-editor-paste-20261106/integration-completion.json。
#709/PR710はCtrl+R/2/5の段落書式変更を止め、339checksと最終CI42129740643が成功。main `8ccbf29a232bd25d03102be3ce8f0fe4f9e05851`へ統合・通常反映・10611filesの全保全整理まで完了した。
[品質記録](../quality/2026-11-06-editor-format.md)。確定原本はD:/NeNeFolio/agents/709-editor-format/integration-completion.json。
#711/PR712は表示3択と狭幅chooserの編集保持486checksと最終CI42132598999が成功し、main `47f80045ce3f33b71e176d31c38b43f84caa6d94`へ統合した。
通常exe不変、4402filesの全保全と専用領域整理まで完了。[品質記録](../quality/2026-11-06-layout-native.md)、確定原本D:/NeNeFolio/agents/711-layout-native/integration-completion.json。
#713/PR714はASanのfrexp出力先未検出をDebug linkで修復し、最終CI42136664822成功、main570d14000de8cbc228c4017823d850a6543231a8へ統合。
通常Debug反映と実data32files不変、7806files全保全・専用領域整理まで完了した。[品質記録](../quality/2026-11-06-asan-interceptor.md)。
確定原本D:/NeNeFolio/agents/713-asan-interceptor/integration-completion.json。
#715/PR716は別名保存の対象164checksと再実行170checks、最終CI42139348824成功、main4096ce691e151bbba32bc08bf3739952ebf18535へ統合。
通常exe不変、3358files全保全・専用領域整理まで完了。[品質記録](../quality/2026-11-06-save-as-native.md)、確定原本同agents/715-save-as-native/integration-completion.json。
#717/PR718は改名252checks・直接共用save_as164checks・再実行252checksと最終CI42141683575成功、main8ebc8b8a7b28fe81f9bf68630c58acf26617b1aaへ統合。
通常exe不変、4099files全保全・専用領域整理まで完了。[品質記録](../quality/2026-11-06-rename-native.md)、確定原本同agents/717-rename-native/integration-completion.json。
#719/PR720は置換2欄の保存221checksと最終CI42144117242成功、mainb85308e653eac90041c764accb882231350eb058へ統合。
通常exe不変、3371files全保全・専用領域整理まで完了。[品質記録](../quality/2026-11-06-replace-save-native.md)、確定原本同agents/719-replace-save-native/integration-completion.json。
#721はカテゴリ間dragのnative編集保持と各保存先の履歴を追加し、358checksと再実行398checksが成功、stderr空。
製品src/API/schemaは不変。最終CI/統合はこれから。[品質記録](../quality/2026-11-06-move-edit-native.md)。作業木D:/NeNeFolio/worktrees/721-move-edit-native、試行同outputs、恒久原本同agents。
全体journalはD:/NeNeFolio/agents/quality-20261106/journal.md。
00:51の停止は今回指示で解除。停止処理の履歴はD:/NeNeFolio/agents/706-store-publication/integration-completion.jsonに保持する。
#704/PR705は操作パレットの取消と編集保持を確認（141checks、stderr空）し、最終CI42054891152成功、main eaef26aへ統合・全保全整理済み。
[品質記録](../quality/2026-11-06-palette-native.md)。原本はD:/NeNeFolio/agents/704-palette-native/integration-completion.json。
#702/PR703は最終CI42045853037成功、main82204cbへ統合・全保全整理済み。原本はD:/NeNeFolio/agents/702-discard-native/integration-completion.json。

## 公開済み1.3.1

[GitHub 1.3.1 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v1.3.1)を公開し、ZIP/SHA256SUMSの再取得と全11memberの原本一致を確認した。
#697 / PR699、最終CI42035926749のfull-gate/check成功。配布source/tagは `d474e531a5199895d6d8447435c7f45ade362cc3`。
Windows版は1.3.1.0。正規ZIP/MSIX生成、専用コピーでの起動・設定Version 1.3.1・正常終了を確認した。
[変更一覧](../release/1.3.1.md)・[リリース品質記録](../quality/2026-11-05-release-1.3.1.md)を参照。
確定原本は `D:/NeNeFolio/agents/release-1.3.1-20261105/integration-completion.json`。697専用worktree/outputは全保全後に整理済み、branch/commitは保持した。
結果収載#700 / PR701も最終CI42040854960成功、main e1df61cへ統合・全保全整理済み。原本は `D:/NeNeFolio/agents/700-release-result/integration-completion.json`。

11月5日21:28の一時停止後、22:13のリリース依頼を完了し、23:17頃に品質対応を再開した。11月6日00:51頃に区切り、同日07:08の新指示で継続品質対応を再開した。

## リリースに含む最新の品質改善

#693 / PR694、最終CI42026620126のfull-gate/checkが成功し、main `b898c79413b578bbfdb96c615ee0528ddae62fee` へ統合済み。
検索はサロゲートの無いUTF-16先頭を4単位ずつ検証し、候補塊と末尾は既存処理へ戻る。API/保存形式/検索の意味は不変。
20通常本文・80条件・3600sampleが全結果一致。262144単位のASCII/日本語/末尾絵文字は前方約92〜93%、count約31〜36%短縮。
先頭絵文字などの費用も残り、65536単位の後方は+5.15%（18.32µs）。全数値・対象試験・静読の範囲は[検索検証の品質記録](../quality/2026-11-05-search-validation.md)へ。
原本は `D:/NeNeFolio/agents/693-search-validation/integration-completion.json`。

現在の履歴は[11月6日日報](../reports/2026-11-06.md)と[引き継ぎ](../handoffs/2026-11-06.md)。前日までの完了・不採用候補・全試行・保全先は[11月5日日報](../reports/2026-11-05.md)と[引き継ぎ](../handoffs/2026-11-05.md)から辿る。
全体journalは `D:/NeNeFolio/agents/quality-20261105/journal.md`。停止記録#695の履歴は同agents/695-current-state/integration-completion.jsonから辿れる。
主担当単体で実装・計測・検証し、必要な独立静読だけ委任した。追加waiver none、既存WVR-0001継続。

## 通常版と実機確認

通常exeは `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、反映sourceは `570d14000de8cbc228c4017823d850a6543231a8`。
SHA256は `2f1864c471e0cb0ff14053dfa52fbc2aabe0da21caa4d16f8507e61cd41e14a0`。#713反映時、実data32fileの全bytes/hash/作成更新日時不変を確認した。
通常app/実ノート/Storeは品質試験に使っていない。hideによる1.3.0の実機受理は未確認。
文書・native試験のみの変更を理由に通常buildを反復しない。配布1.3.0のsourceとは区別する。

## Store公開と残る未確認事項

1.3.1の提出用MSIXと日本語・英語・簡体字の更新文は `D:/NeNeFolio/releases/1.3.1/store-candidate-20261105/` に準備済み。
ZIPは `D:/NeNeFolio/releases/1.3.1/github/`。ZIP SHA256は `120968465a35664fa91ec300a18bbca699406c2c8a96b0c8ca4f4923c32535f7`、
MSIX SHA256は `3b113aabfb8c88b00ad0bc6b0d0b19acf7670a881797c6df0fbf2e58f44f5326`。
11月6日の[hide本人の報告（#706）](https://github.com/hideyukiMORI/nene-folio/issues/706)で、Store掲載1.3.1とStoreアプリ経由の実機更新成功を確認済みとした。
原本はD:/NeNeFolio/agents/release-1.3.1-20261105/store-status.json。11月5日23:17頃の「Pre-processing」報告も履歴として保持する。
認定日時・申請番号・正確な申請時刻は未確認。portal画面・提出package hash・更新実機の独立確認は行っていない。
Store署名版は品質試験に使っておらず、既存の配布試験はMSIX内payloadの起動確認である。

[GitHub 1.3.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v1.3.0)は公開済み。配布source/tagは `4359e9e2268bdeef135a4630dbaa241d57c3063d`。
11月4日22:30頃のhideの「申請した」という報告により、Store1.3.0（Windows1.3.0.0）は申請済みとして記録した。
申請番号・正確な申請時刻・現在の審査段階・認定/公開は未確認。次はhideの結果報告を待つ。
提出用MSIX/3言語更新文は `D:/NeNeFolio/releases/1.3.0/store-candidate-20261104/`、申請原本は同agents/release-1.3.0-20261104/store-status.json。
[変更一覧](../release/1.3.0.md)・[リリース品質記録](../quality/2026-11-04-release-1.3.0.md)・[11月4日日報](../reports/2026-11-04.md)・[11月4日引き継ぎ](../handoffs/2026-11-04.md)を参照する。
以前の1.2.0のPre-processing報告を現在の認定/公開状態へ流用しない。WACK/AppData root/Store署名版の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)へ。

## 保留・保持物

#634のJSON変異試験は安全判定の通知を受けて保留。比較は未実行、repo変更/PR無し。
未完了のためD:/NeNeFolio/worktrees/634-json-boundary-oracleと同outputsを保持し、追加build/runは行わない。
入力・補助build失敗・通知とhideの通常品質対応継続指示は同agentsと全体journalへ記録した。

#466の表幅診断は保留。#479は修正・統合・通常反映・整理済み。#530と旧削除拒否9作業木は保持し、削除を再試行しない。
正確な所在・理由は引き継ぎ書へ。10月31日にhideが削除した47出力を古い記録だけで現存扱いに戻さない。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/保留。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
