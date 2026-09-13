# ADR 0022 — 改名の意図を先に記録し、履歴・md・索引を再開可能に移す

- 状態: 受理（18:06 JST、独立レビューの指摘を反映）
- 日付: 2026-09-13
- Issue: #56（別名保存#55の統合後に製品経路を接続する）
- 規則: ARC-001〜005/007〜011、C-002〜008/012/014/017、QLT-009/012/013

## 文脈

#47の採用計画は、改名で現在mdと履歴の両方を新しい名前へ追随させる。
複数のファイル/ディレクトリとindex.jsonを単一のOS renameで確定できないため、
途中失敗を成功にせず、異常終了後にも同じ意図を再開できる契約が必要。
別名保存と違い、改名前には既存の保存/履歴が成功していなければならない。

## 決定

1. GUI「名前を変更」、F2、:rename 名前は共通RENAME操作。対象は現在表示する名前付きノート。
   未選択/無題は理由を表示。編集内容を既存の保存で確定してから改名する。モード/本文/Undoを再設定しない。
   名前面は初回/別名保存の既存面を閉じた操作種別で拡張する。元名を選択状態で表示し、カテゴリは変えない。
   同じ名前は変更なし。Windowsの大小文字だけの別名は衝突として拒否する。
2. 中核の不透明なnote_renameがカテゴリ/旧名/新名/同じ位置で名前だけを変えた台帳を所有する。
   applicationは副作用前にこの意図を確保し、未完了時に一つだけ保持する。
   adapterはrename_note(plan)で新規または同じ意図の再開を行い、起動時のrecover_renameは記録を読んで同じ処理を呼ぶ。
   ポートは記録公開前の拒否/OOM/衝突と、公開後のPENDINGを閉じた結果で区別する。後者だけapplicationが保持する。
   完了するまで通常の保存/切替/並替えなどは未完了処理を先に再試行し、失敗なら進めない。
   未完了時のパンくずは「名前変更の復旧待ち」と表示し、古い名前を現在の実ファイル名だと示さない。
   本文の追加入力はRichEditに残せるが、古いパスへは保存しない。保存成功で改名を完了してから新名へ本文を保存する。
3. 既存dataを開くpersistence_adapterはdata/.nenefolio.lockを共有なしで開き、寿命中保持する。
   同じdataを別プロセスが使う場合は明示的な起動失敗。ロックファイルは異常終了しても残してよく、所有はOSハンドルで判定する。
   dataがまだ無ければファイル/カテゴリを勝手に作らない。カテゴリ作成の後続実装はdata作成後に同じロックを取得する。
   この版では書けないdataの起動も拒否する。安全に復旧と編集を直列化するための制約として表示/文書へ明示する。
4. 改名先のmdと履歴ディレクトリが両方とも無いことを先に確認する。残存履歴も再利用/上書きしない。
   元mdと存在する元履歴のディレクトリを開き、volume serialと128bit file IDを取得して同一性を記録する。
   初版はローカルNTFSだけを対象とし、ボリュームの照会でNTFSであることを確認する。ネットワーク/他形式/照会不能は副作用前に拒否する。
   識別情報のAPI成功だけで他形式の安定性を保証しない。名前だけで移動済みと推測しない。
   元md/履歴ディレクトリとdata/カテゴリ/.history/履歴カテゴリの存在する親はOPEN_REPARSE_POINTで開き、reparse pointを拒否する。
   親も処理中ハンドルを保持し、途中で差し替えられないよう削除/書込と共有しない。exe配置までの祖先全体を改名機能の対象にしない。
   移す対象はDELETEを持つハンドルで開き、他の書込/削除と共有せず、SetFileInformationByHandleの置換なしrenameを行う。
   事前確認後に同名が現れてもOSに拒否させる。対象ハンドルは確定処理中保持する。
5. data/.rename.jsonを版1で原子的に新規公開し、flush完了後にだけ履歴→md→index.jsonの順に進む。
   記録はcategory/from/to、完成後の台帳、元mdの識別子と元履歴の識別子（無ければ空）を持つ。
   codec/名前検証はcoreの一経路。未知の版/不正記録は拒否し、既定値で削除しない。
   更新する段階番号は持たず、旧/新パスの存在と保存済み識別子で再開箇所を確定する。
   IDありの対象は両方存在、両方不在、識別子不一致で理由を出して停止する。
   履歴IDなしは旧・新の両側不在だけを受理し、どちら側に履歴が現れても拒否する。
   履歴なし→mdだけ、履歴あり→ディレクトリ全体を動かし版の番号/本文を変えない。
6. index.jsonが完成した後にだけ.rename.jsonを削除して完了とする。削除失敗も未完了。
   再試行は同じ完成後の台帳を原子的に書き直せる。途中で成功済みのmd/履歴を巻き戻したり削除したりしない。
   applicationは完了時だけ準備した台帳を受け取り、選択/パンくずを新名へ揃える。
   起動時はカテゴリ/ノート走査の前に復旧を終え、失敗なら理由を出して起動しない。
7. 初回/別名保存の台帳未同期と改名の未完了をapplicationの同じ同期入口で処理する。
   明示的な:q!はアプリを閉じても永続的な改名意図を破棄せず、次回起動が続きを行う。
   :qの変更確認は副作用を持たず、未完了なら拒否する。記録前の失敗/取消は名前を変えない。
   記録後のフォームは名前/カテゴリを変更不可にし、同じ改名を再開する。閉じることは改名意図の取消ではないと説明する。
   フォームを閉じても本文と未完了意図を保持する。履歴失敗を通常成功へ丸めない。
8. categories.jsonとindex.jsonの版1・5版履歴は維持。復旧記録だけ新しい版付きスキーマを加える。
   ADR0012のカテゴリ間移動の履歴契約は変更しない。別名保存#55とノート内検索は別の意味/Issueで扱う。

## 却下した選択肢

- mdだけ先に改名して履歴失敗を無視: hideの採用条件を満たさず、履歴の帰属を失う。
- 全ファイルをコピーして元を削除: 余分な本文書込と削除を増やし、同じ改名の経路にならない。
- メモリだけの段階/パスの存在だけで再開: 異常終了や外部ファイルの混入で誤った対象を確定する。
- 実行中の別プロセスが残した意図を起動側が復旧: ロックの所有中は拒否し、同じdataの永続化所有者を一つにする。

## 根拠と検証

- [FILE_ID_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_id_info)
- [GetFileInformationByHandleEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex)
- [File IDの性質](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information)
- [GetVolumeInformationByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationbyhandlew)
- [SetFileInformationByHandle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle)
- [CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)

保証するのはプロセス異常終了後の再開。電源断/OSクラッシュ時の名前空間更新の耐久性は未証明で、本文flushだけで保証したとは扱わない。

独立レビューでreparse point、履歴なし両側不在、ファイルシステムの範囲を追補した。
ロック取得はdata配下すべての書込成功を意味せず、各保存の失敗処理は維持する。

codec/台帳/意図と確保失敗は中核単体。OS側はタスク専用のmd/履歴で、各段階直後を作った再起動fixture、
同名と識別子不一致、履歴失敗、台帳失敗、記録削除失敗、同じdataの二重起動を測る。
GUIの取消/再開/Undo/IME/F2は別に測定し、物理入力と目視は区別する。
実装前にこの判断を記録した。Waivers: none。

## 18:15 JSTの実装境界

現在はnote_ledgerの同じ位置での改名と合成可能なread codec、note_rename、rename_journalの純粋中核だけを実装した。
application/adapter/UIの改名、dataロック、NTFS判定、実ファイルの移動/復旧は未実装。F2/:renameはまだ登録しない。
このADRの決定は完成機能を意味しない。中核は副作用を持たず、既存の製品操作からは未接続。

記録の版1は次の順序のキーを持つ。

```json
{"version":1,"rename":{"category":"A","from":"old","to":"new","ledger":{"version":1,"notes":["new"]}},"fileId":"0123456789abcdef0123456789abcdef0123456789abcdef","historyId":""}
```

識別子はvolume64の16桁とFILE_ID_128の16バイトを順に2桁ずつ表した32桁、計48桁の小文字hex。
空を許すのはhistoryIdだけ。coreは名前/記録形を検証し、実際の同一性/NTFSの判定はadapterが行う。
note_rename_createは元台帳と検証済みのnote_nameから準備し、note_rename_take_ledgerは完了後に一度だけ所有権を移す。
公開前拒否/公開後PENDINGを分けるポートの結果型と、applicationの保持状態は次の実装で加える。
