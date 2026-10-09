# ADR 0022 — 改名の意図を先に記録し、履歴・md・索引を再開可能に移す

- 状態: 受理（18:06 JST、独立レビューの指摘を反映）
- 日付: 2026-09-13
- Issue: #56（別名保存#55の統合後に製品経路を接続する）
- 規則: ARC-001〜005/007〜011、C-002〜008/012/014/017、QLT-009/012/013

2026-10-08: [ADR 0041](0041-category-rename-and-journal-kind.md) が単一plan/portを閉じたNOTE/CATEGORYへ広げ、
版1の読取と版2の単一書出し、ローカルNTFSの確認、kind付き復旧表示と専用再試行面を定める。
同ADRのD1/D2/D3で段階的に移行し、以下の既存NOTEの保存・同一性・再開契約は明示した補正以外を維持する。

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

## 18:15 JSTの実装境界（2026-09-13。次節が現在の範囲）

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

## 2026-09-16の補正（決定3の範囲）

hideの再開指示を受け、設計リナが決定3の「書けないdataの起動も拒否する」を実装前に次へ改める。他の決定は変えない。

- ロックの目的は同じdataを使う2つのプロセスの直列化であり、書込可能性の検査ではない。
  `data/.nenefolio.lock`を共有なしで開けない理由が**他プロセスの保持（共有違反）**なら起動を拒否する。
  **アクセス拒否・読み取り専用ボリューム**でロックファイルを作れないときは起動を許し、
  adapterは「ロック無し」を閉じた状態として持つ。保存・並替・改名などの書込はこれまでどおり各操作の失敗として理由を出す。
- ただし`data/.rename.json`が存在し、ロックを取れないときは復旧を実行できないので、理由を出して起動しない。
  復旧はロックを保持したプロセスだけが行う。
- ロック無しの起動で改名を要求したときは、記録を公開する前に「dataに書けないため名前を変更できません」で拒否する。
  記録前の拒否なので意図は残らない。

理由: 実行ファイルと同じ場所の`data/`を読み取り専用の場所に置いて閲覧だけ行う使い方を、改名の実装で失わせない。
決定4（ローカルNTFS・reparse point拒否）は据え置く。hideの実際の置き場が同期フォルダなら別途判断する。

## 2026-09-16の実装境界

決定1〜8と上の補正を、5層すべてに実装した。受理済みの決定本文は変えていない。

- application: `folio_state_rename_note` が未選択/無題/同名/大小文字だけの別名/既存名を先に断り、
  編集中なら既存の保存経路で確定してから `note_rename_create` で意図を確保する。
  `synchronize_index` と改名の再開は一つの同期入口に束ね、保存・切替・並替・色・新規・別名保存・
  終了確認がそこを通る。未完了は `FOLIO_STATE_RENAME_PENDING` で、意図は一つだけ保持する。
  `folio_state_create` はカテゴリ走査より先に `recover_rename` を呼び、終わらなければ起動しない。
  `pane_title_view.recovering` が「名前変更の復旧待ち」を表す。
- adapters/win32: `data/.nenefolio.lock` を共有なしで寿命中保持する。共有違反は起動拒否
  （`PERSISTENCE_ADAPTER_DATA_IN_USE`）、アクセス拒否・読み取り専用・data/不在は錠なしの起動を許し、
  記録があって錠を取れないときだけ `PERSISTENCE_ADAPTER_RECOVERY_LOCKED` で起動しない。
  改名は移動先の両不在 → 親と対象の `FILE_FLAG_OPEN_REPARSE_POINT` と `GetVolumeInformationByHandleW`
  → `GetFileInformationByHandleEx(FileIdInfo)` → `.rename.json` の新規公開 → 履歴 → md →
  index.json → 記録削除の順で、移動は `SetFileInformationByHandle(FileRenameInfo, ReplaceIfExists=FALSE)`。
  再開は旧/新の存在と保存済み識別子だけで段階を決め、合わなければ記録を消さずに止める。
- ui/win32: 共通操作 `FOLIO_COMMAND_RENAME`（表示名「名前を変更」・Ex別名 `rename`）、
  主窓のF2、既存の名前入力面の第3の操作種別。名前引数を許す操作は閉じた集合になった。

まだ無いもの: カテゴリを変える改名（移動はドラッグのまま）、記録が壊れたときの画面からの修復、
`.rename.json` の版2。決定4のとおり初版はローカルNTFSだけで、同期フォルダ上の`data/`は未判断。
電源断/OSクラッシュ時の名前空間更新の耐久性は決定どおり未証明。

独立レビューの所見を受けて、同じ単位で次を加えた（決定本文は変えていない）。

- 決定2の「公開後だけapplicationが保持する」を型で表す。`rename_outcome`は公開前の拒否と、
  公開後の`RENAME_PENDING`（再試行で進み得る）／`RENAME_HALTED`（data/を直すまで進まない）と、完了に分かれる。
  adapterは記録の公開（`file_bytes_create`の成功）を境に、以後のどの失敗もその2値へ写す。
  applicationは2値のどちらでも意図を保持し、同じ同期入口で再試行して失敗行を分ける。
- 決定5の「名前だけで移動済みと推測しない」を再開にも適用する。`rename_attempt`（START / RESUME）を加え、
  applicationは保持中の意図をRESUMEで頼む。RESUMEで記録が無ければ新規開始へは落とさずHALTEDにする。
  一方、記録の削除が`ERROR_FILE_NOT_FOUND`で失敗したら、消し終えたのと同じとして完了にする。
  STARTのときに他人の記録が既に`data/`にあれば、公開前の拒否ではなくHALTEDで意図を保持する。
  その`data/`は人手が要る状態であり、次回の起動が同じ理由で止まるのと揃える（設計リナが2026-09-17に確認して採用）。
- 決定1の名前面は、文書の実名（`folio_state_document_name`）を初期値にする。`pane_title_view`は実名と
  `recovering`だけを持ち、「名前変更の復旧待ち」の文言はUIのパンくず1か所が持つ。未完了の意図があるときは
  面を最初から固定状態（旧名→新名の表示・入力不可・「再試行」「閉じる」）で開く。

## 2026-09-17の補正（決定4の範囲・Issue #61）

hideの「同期フォルダのreparse pointは一般にどう処理されるか」を受け、設計リナが決定4の「reparse pointを拒否」を次へ改める。他の決定は変えない。

- OneDriveのFiles On-DemandとDropboxのSmart SyncはWindowsのCloud Files APIで同期フォルダ内の全ファイルを
  プレースホルダ（`IO_REPARSE_TAG_CLOUD`系）にする。一律の拒否では同期フォルダに置いた`data/`の改名が常に拒否される。
- 決定4の目的はシンボリックリンク／junctionを辿って`data/`の外を動かさないことである。その目的に必要なのは
  **name surrogate のタグ**（`IsReparseTagNameSurrogate`、0x20000000ビット。シンボリックリンク0xA000000C・マウントポイント0xA0000003）の拒否だけで、
  他のエディタもリンクの区別にはこの判定を使い、rename自体はタグを見ない。
- 親と対象は引き続き`FILE_FLAG_OPEN_REPARSE_POINT`で開き、`GetFileInformationByHandleEx(FileAttributeTagInfo)`のReparseTagが
  name surrogateのときだけ拒否する。クラウドのプレースホルダ・重複除去・WOFなどは通常のファイルとして扱う。
  NTFS判定・ファイルID・置換なしrename・記録と再開の契約は変えない。
- 同期サービスの競合コピーとオンラインのみファイルの取得はエディタを問わず起きる事象で、残るリスクとして記す。

## 2026-10-07の補正（決定3の予告の履行・Issue #174）

ADR 0039の決定10（カテゴリの作成）のport/adapterの単位で、決定3の予告「カテゴリ作成の後続実装はdata作成後に同じロックを取得する」を
永続化ポートの`create_category`が履行する。他の決定は変えない。

- `data/`が無いまま起動したプロセスは錠を持っていない（2026-09-16の補正で錠無しの起動を許している）。
  最初のカテゴリを作るときに`data/`を作り、起動時と同じ開き方（共有なし・無ければ作る。開き方はadapterの1か所）で`data/.nenefolio.lock`を取る。
- 錠を**いま取った直後に**`data/.rename.json`が無いことを確かめる。復旧（決定6）は起動時にしか走らないので、
  起動の後に他のプロセスが`data/`を作って改名を途中で止めた場合は、ここで断らないと記録を残したまま書き進めてしまう。
- 錠を取れない（他プロセスの保持・アクセス拒否・読み取り専用）・記録が在る・記録の有無を確かめられないときは、カテゴリを作らずに`UNWRITABLE`で断る。
  作った空の`data/`は残る。取得後に記録を拒否した錠の保持は、下記2026-10-09の補正で解放へ改めた。
- 起動時から錠を持っているプロセスは、錠の取得と記録の確認を繰り返さない。振る舞いは変えない。
- 起動時の`data/.rename.json`の確認と同じ判定（在る・照会できない・道を組めない、のどれでも「無い」とは言わない）を両方の経路で使う。

理由: `data/`が無い場所で起動して最初のカテゴリを作る使い方を、改名の直列化と復旧の前提（同じdataの永続化所有者を一つにする）を崩さずに許す。
adapterの実物の測定（`data/`無しからの作成・錠の取得・既存名・記録が在るとき・書けない場所）は統合後の別の確認記録で行う。

## 2026-10-09の補正 — 通常書込も錠を所有してから行う（Issue #223）

2026-09-16の「錠無しで起動できても書込は各操作で断る」を通常経路へ揃える。
`archive_note` / `write_note` / `create_note` / `move_note` / `write_category_ledger` / `write_settings` /
`write_note_ledger` の7本も、既存カテゴリ作成と同じ `writable_data` を通る。
pathを組めてから、最初のdisk読取・履歴作成・本文公開・移動・台帳生成より前にdataの錠を所有し、
取得できなければ既存 `UNWRITABLE` を返す。読みのportと錠無しの閲覧起動は変更しない。
改名・ごみ箱の既存型付き拒否経路はそのままとする。

遅延取得は起動時と同じ `open_lock` の1経路。取得したlocal HANDLEで記録不在を確かめてからadapterへ採用する。
記録が在る・確認できない場合はそのHANDLEを閉じて拒否する。2026-10-07の「拒否後も錠を持ち続ける」を置き換える。
保持済み錠の早期成功によって次の要求が記録確認を素通りすることを避け、再試行も同じ確認を通すためである。
記録を解消したあとの同sessionの書込は改めて取得できる。作った空のdataは残し、lockファイル自体は消さない。

既に錠を所有するsessionは同じ所有を使う。改名の内部台帳公開もこの経路で自分の記録に阻まれず、
記録の公開→移動→台帳→記録削除の1経路を維持する。所有済み錠のもとで外部editorが後から記録を置く場合の検知保証は加えない。

同helperがdataを用意するため、data無しで明示的にsettingsやカテゴリ台帳を初めて書く場合も、
data作成→錠取得→記録不在確認を経て成功し得る。これは以前の親経路不在による保存拒否からの差分である。
起動しただけではdataを作らない。初回保存で勝手にカテゴリを作る経路も追加しない。

実測は [対象品質記録](../quality/2026-10-09-write-lock-checks.md)。公開port/enum/永続schema/依存/ゲートは変更しない。

## 2026-10-09 の補正（#227 — 公開する記録の上限）

[ADR0006の共通raw file上限](0006-edit-mode-and-save-path.md)は台帳と復旧記録にも適用する。
STARTは記録全体の実serialized lengthが16 MiBを超えると、file_bytes_createの共通入口で
temporary file/記録公開前にUNWRITABLEとなり、既存JOURNAL_FAILEDで拒否する。
履歴/mdを動かさず、generic persistence結果やjournal schemaを増やさない。
