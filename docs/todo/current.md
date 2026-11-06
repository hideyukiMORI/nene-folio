# いまの状態 — NeNe Folio

## 2026-11-06 21時台 — 1.3.2の公開前候補へ区切る

hideの21:32の依頼により、新しい品質課題を追加せず、最後の#771を完了して1.3.2候補を準備する。
#771/PR772は最終CI42219301978 full-gate/check成功、main e962ac78c1b5859e650d028effd6163b13fab516へ統合済み。
通常exe不変、全file保全・専用worktree/output整理済み。原本D:/NeNeFolio/agents/771-export-recovery/integration-completion.json。
#769/PR770も最終CI42215039675成功、main fed522aへ統合・全保全整理済み。詳細は日報と各品質記録へ。

#773はCMake唯一の版を1.3.2へ更新し、変更一覧・3言語更新文・正規ZIP/MSIX・専用起動/Version確認を揃える。
[変更一覧](../release/1.3.2.md)・[品質記録](../quality/2026-11-06-release-1.3.2.md)。最終CI/配布生成/起動確認はこれから。
恒久配布D:/NeNeFolio/releases/1.3.2/、確定原本D:/NeNeFolio/agents/release-1.3.2-20261106/integration-completion.json。
GitHub tag/Release公開とStore送信は未実施。これまでの継続品質対応を新しいテーマへ広げず、この公開判断用候補までを仕上げる。

今日のpaste・書式キー・アクセシビリティ選択・検索・Debug ASan修正と、保存/終了/書き出し等のnative回帰は統合済み。
最新の製品変更#763/PR764はCI42200797878成功、main4dabcfdca8951f5113c812adbc3248bdd74fed42から通常版へ反映、実data32file不変。
日付付き[日報](../reports/2026-11-06.md)・[引き継ぎ](../handoffs/2026-11-06.md)、全体journal D:/NeNeFolio/agents/quality-20261106/journal.mdを参照。
追加waiver none、既存WVR-0001継続。保留466/634/530と旧削除拒否領域は維持する。

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

通常exeは `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、反映sourceは `4dabcfdca8951f5113c812adbc3248bdd74fed42`。
SHA256は `74fe1738ee249e7ca9396c23f8e70b414b9876e615c2088a4b4ede91f07cc1f1`。#763反映時、実data32fileの全bytes/hash/作成更新日時不変を確認した。
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
