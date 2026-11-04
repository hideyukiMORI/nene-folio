# 保存用一時名の上限と公開復帰 — #601

通常置換、新規作成、ZIP公開は同じfile_bytesの一時ファイル→flush→MoveFileExWを使う。
既存の同名/一時ファイル保持の確認に加え、255/256候補の排他保持と1候補を空けた後の復帰を調べた。
初回16呼出しと復帰3呼出しは期待どおりで、製品/testの変更は不要と判断した。

## 根拠と範囲

[ADR0020](../adr/0020-untitled-note-and-first-save.md)は256候補をCREATE_NEWで取得し、既存候補を奪わず上限時に拒否する。
[ADR0082](../adr/0082-markdown-transfer.md)は新規MarkdownとZIP公開もこの正典へ合流させる。
[既存初回保存記録](2026-09-13-first-save-checks.md)の同名/大小文字・他一時ファイル保持、
[既存transfer記録](2026-11-01-markdown-transfer.md)の既存出力拒否は再実行せず、上限の排他保持と復帰に焦点を置いた。
既存の画像表示/容量/保存作成日時の試験も読取確認し、今回の実行成功へ数えていない。

#597最終候補の正規Debug3libを、#599で照合したコピーのSHAと今回コピーのSHAで照合して再利用。
外部helperは固定toolchain/lld/明示ASan+UBSan runtimeでlinkし、実file_bytes_store/create/export_archiveを呼ぶ。
製品library自体を変更せず、製品への試験用APIも追加しない。

最初のhelperはstaticの一時名生成関数をfixture作成にも使うためfile_bytes.cを直接includeしたが、
include先のHANDLE引数にnullability-completeness警告が出て/WXで失敗した。警告を抑制せず、この方式を取りやめた。
fixtureの名前だけを既存の固定hash/00〜ff配置に合わせてPythonで作り、helperは実libraryへlinkする形へ訂正した。
fixtureの名前生成はhash関数の独立検証ではない。上限結果、既存fileの保持、公開物bytesの確認とは分ける。

## fixtureと実行

専用D側に16directoryを作成。占有fileの内容は番号付きsentinel、既存宛先は固定原文、保存候補は日本語を含むCRLF本文。
既存fixtureのmtimeを固定過去値へ設定し、実行前の全file内容SHA/byte長/作成更新日時を保存した。
native呼出し中は占有fileを共有なしGENERIC_READで開いて保持し、終了後に全handleを閉じた。

| 入口/宛先 | 占有候補 | 初回結果 |
| --- | --- | --- |
| 通常置換・既存宛先 | 0 / 1 / 255 | STORED 3件、作成日時保持 |
| 通常置換・既存宛先 | 256 | UNWRITABLE 1件、元bytes/作成更新日時保持 |
| 新規作成・未存在宛先 | 0 / 1 / 255 | STORED 3件 |
| 新規作成・未存在宛先 | 256 | UNWRITABLE 1件、宛先未公開 |
| ZIP公開・未存在宛先 | 0 / 1 / 255 | STORED 3件 |
| ZIP公開・未存在宛先 | 256 | UNWRITABLE 1件、宛先未公開 |
| 新規作成/ZIP公開・既存宛先 | 各0 / 255 | NAME_TAKEN 4件、既存bytes/作成更新日時保持 |

256候補の3caseは初回結果を独立監査してから、最後のff候補だけを同じfixture内の別名へ移して保持した。
再び残り255fileを排他で保持し、同じ入口を呼ぶと3件すべてSTORED。解放した3sentinelも内容/作成更新日時が不変だった。
失敗前からある一時ファイルを自動削除した結果ではない。

## 結果と全試行

| run | 結果 |
| --- | --- |
| link-01 | include方式でnullability警告5件、子1。exe未生成のartifact記録でwrapper125。source/raw/結果を保持 |
| link-02 | 実libraryを呼ぶhelper、子/記録側0、stderr/記録エラー空 |
| check-01 | setup/初回16/復帰3のnative、Python評価/記録側すべて0。stderr/記録エラー空、各process60秒timeoutなし |
| audit-01 | 最終2063fileと記録artifact7件を再SHA照合、子/記録側0、stderr/記録エラー空 |

初回はSTORED9/UNWRITABLE3/NAME_TAKEN4、復帰はSTORED3。
2046占有fileは初回前後で内容/作成更新日時が一致し、復帰後も3件の別名保持を含めて一致した。
file集合は初回前2055、初回後2061、復帰後2063。予定外の残存一時ファイルや消失はなかった。
置換成功対象は内容/更新日時が変わり、作成日時を保持した。変更しない対象・拒否対象の不変と区別する。
summary.jsonの既存file不変という広いfield名の意味を、元snapshot/実fileへ戻ったauditのobservation.jsonで明確化した。
元summaryは上書きせず、すべての既存宛先が不変だったとは報告しない。

4ZIPをPython zipfileで独立に読み、CRC、UTF-8名の1member、全本文bytes、STORE方式、固定1980-01-01を照合した。
再試行時も同じ生成結果と全bytes一致。ここでのZIPは合成本文だけで、実ノートや実画像を使っていない。

原本は`D:/NeNeFolio/agents/atomic-publication-20261104`、試行は同名outputsのtools/record-run.py記録。
source/header/library/exe/fixture定義の入力snapshot、stdout/stderr、終了値、before/intermediate/after metadataを収載した。
準備scriptは同日の全体agents領域。API照会や探索の失敗はjournalへ分離する。

## 制限と完了経路

これは同一ホストの有限条件。公開途中の電源断/クラッシュ、ディスク満杯、ACLの全組合せ、任意の並行書込、
network filesystem/全Windows版、全heap/性能、保存UI全体は保証しない。占有helperが排他handleを持つ場合を扱い、
第三者が同時に候補を作成/削除し続ける競合は試していない。順次の読み取り比較から原子性の一般的証明をしない。

ARC-001/003/007/009/010/012、QLT-008/012/013、GIT-001〜004、ADR0006/0020/0082を確認した。
品質記録/gate-proofs/日報/引き継ぎ/currentの5文書だけを変更。文書差分/参照確認後、最終head必須CIと正典統合へ進む。
通常exeは#597の製品入力/実SHA不変を確認して再build/data操作を行わない。
追加作業木とoutputの全保全/hash照合、取込/未保存/ignored/稼働参照/link確認後に整理し、branch/commitを保持する。
確定原本は同agentsのintegration-completion.json。追加waiverなし、既存WVR-0001は変更なし。実app/実ノート/Store操作なし。
