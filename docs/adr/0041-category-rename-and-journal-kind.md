# ADR 0041 — 改名意図を閉じた種別に広げ、カテゴリと履歴と台帳を一つの記録から復旧する

- 状態: 受理（実装はD1/D2/D3で段階的に行う）
- 日付: 2026-10-08（実時計）
- Issue: #204（設計）、親 #169、FR-037
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-008 / ARC-009 / ARC-010 / ARC-011、C-002 / C-003 / C-005 / C-012 / C-014、QLT-008 / QLT-009 / QLT-010 / QLT-012 / QLT-013、CNF-009 / CNF-011
- 関連: [ADR 0039](0039-note-trash-and-category-operations.md) 決定15、[ADR 0022](0022-note-rename-and-recovery.md)、ADR 0024 / ADR 0028 / ADR 0035 / ADR 0038

## 文脈

カテゴリ名はディレクトリ名で、改名は `data/<旧>`、`data/.history/<旧>`、`categories.json` の3対象にまたがる。
途中で止まった後をカテゴリ走査だけで直すと、新名は末尾・既定色・展開として復元され、既存の順序・色・展開を失う。
既存の `.rename.json` は版1・ノートだけで、`note_rename` と `rename_note` と未完了意図の所有者もノート専用である。
一方、ファイル識別子・置換しないハンドル改名・原子的な記録公開・起動前の復旧・一つだけの未完了意図は既にある。
カテゴリ用にこれらを複製せず、意味が違う部分だけ閉じた種別で選ぶ。

調査元は main `f461707`、記録は `D:/NeNeFolio/agents/169-category-rename-survey/report.md`。
このADRはC1/C2より後の実装を定めるもので、受理だけでFR-037を実装済みにはしない。

## 決定

### 1. 意図と所有権を一本にする

`enum rename_kind` は `RENAME_KIND_NOTE` / `RENAME_KIND_CATEGORY` の閉じた集合とする。
既存の不透明な `note_rename` を不透明な `rename_plan` に置き換え、旧型・旧codec・旧portの並行経路を残さない。
plan は種別、旧名、新名、完成後の台帳を所有する。NOTEだけが親カテゴリ名と `note_ledger`、CATEGORYは `category_ledger` を持つ。
種類に合わない台帳を取得する呼出しを契約で禁じ、呼出し元は種別を網羅してから型付きの取得関数を使う。

- NOTEの作成は既存の検証済み `note_name` と `note_rename_target` を使い、ノートの挙動を変えない。
- CATEGORYの作成は対象添字と検証済み `category_name` を使う。同じ位置の名前だけを変えた `category_ledger_renamed` を所有し、順序・色・展開を保つ。
  外で作られた旧名は既存の台帳の `name_list` 規則で読む。新名はカテゴリ作成と同じ専用型で検証する。
- planの比較は種別・親カテゴリ（NOTEだけ）・旧名・新名・完成後台帳の全要素を比べる。
  CATEGORYでは全行の名前・順序・色・展開も比較する。同じfrom/toだけで同じ意図とみなさない。
- `category_ledger_read(reader, out)` を加え、既存parserを合成可能な読取と最外の終端検査に分ける。
  JSON文字列への再変換や別parserを作らない。失敗時out不変・OOMの分類を守る。
- applicationの保留所有者は `rename_plan` 一つだけ。NOTE/CATEGORYそれぞれのpending pointerを並べない。
  ポートも `apply_rename(adapter, plan, START/RESUME)` の一本に置き換える。起動の `recover_rename` は既存の一入口を維持する。

### 2. 記録は版2を一つだけ書き、版1を読み続ける

`.rename.json` の場所は変えず、新規公開はNOTE/CATEGORYとも常に版2とする。キーの順序は既存codecと同じく固定。
版1のnote記録は読め、読取時にNOTE planへ正規化する。旧記録を途中で版2へ書き直さず、同じ復旧を終えて消す。

```json
{"version":2,"kind":"note","rename":{"category":"A","from":"old","to":"new","ledger":{"version":1,"notes":["new"]}},"fileId":"0123456789abcdef0123456789abcdef0123456789abcdef","historyId":""}
```

```json
{"version":2,"kind":"category","rename":{"from":"A","to":"B","ledger":{"version":1,"categories":[{"name":"B","color":"#123456","expanded":false}]}},"fileId":"0123456789abcdef0123456789abcdef0123456789abcdef","historyId":""}
```

`fileId` は既存と同じvolume64と128bit file IDの48桁小文字hex。CATEGORYでは本体ディレクトリの識別子を表す。
`historyId` だけ空を許し、元履歴が無かった意図を表す。plan内の完成後台帳は旧名を含まず、新名を一つ含まなければならない。
未知の版・未知のkind・kindとpayloadの不一致・欠けた/余分なキー・不正な名前や識別子を推測で直さない。
記録を残して復旧失敗を返す。`categories.json` / `index.json` 自体の版は1のまま。
版2の記録を残した状態では旧バイナリへ戻せない。旧版は未知の記録として拒否するため、新版で復旧を完了してから戻す。

### 3. OS処理と復旧は既存の一経路を広げる

錠を持つadapterだけが実行する。STARTは既存記録が無いときだけ新規公開でき、記録があるときは同じplanとの完全一致を要求する。
RESUMEで記録が無ければ、新規開始へ落とさず `HALTED`。記録の公開前後で結果を分けるADR0022の境界を維持する。

1. CATEGORYの移動先 `data/<新>` と `.history/<新>` の両方が無いことを確認する。ファイル・ディレクトリ・孤立履歴を上書きしない。
2. `data/` と存在する `.history/` を処理中保持し、name surrogateを拒否する。
   **移動対象のカテゴリ自身を親の共有ハンドルとして保持しない。** DELETEを持つ対象ハンドルとの競合を作らず、対象は既存の移動直前の同一性確認で守る。
3. 本体と存在する履歴はディレクトリであること、name surrogateでないこと、ローカルNTFSであること、識別子を確認する。
   NTFSの照会だけでネットワークでないとはみなさない。ハンドルのvolume GUIDパスを取得できることも確認し、照会不能は副作用前に拒否する。
   この確認は共通の改名識別子経路へ置き、NOTEのローカルNTFS契約も同じ経路で強制する。Shell用の260文字制限は改名へ持ち込まない。
4. 完成後台帳と両識別子を含む記録を既存の原子的な新規公開経路で書き、flush完了後にだけ動かす。
5. 履歴ディレクトリ → 本体ディレクトリ → `categories.json` → `.rename.json`除去の順で進める。
   本体内のmd・index・その他のファイルはディレクトリと一緒に移る。ノートごとに列挙してコピー/削除しない。
   移動は既存 `SetFileInformationByHandle(FileRenameInfo, ReplaceIfExists=FALSE)` だけで、汎用コピーや上書きfallbackは無い。
6. NOTEは既存の履歴→md→index→記録除去を同じkind分岐から行う。パス組みと台帳の確定だけを種別で選び、
   識別子照合・旧新の存在判定・移動・公開後の結果分類・記録の読取/除去は共有する。

復旧は段階番号を持たず、各対象の旧新の存在と保存済みIDだけで次を決める。

| 記録のID | 旧 | 新 | 判断 |
| --- | --- | --- | --- |
| あり | あり | なし | 旧のIDが一致した対象だけを新へ動かす |
| あり | なし | あり | 新のIDが一致したら移動済みとして続く |
| あり | 両方あり、または両方なし | — | HALTED、推測しない |
| 空（履歴だけ） | なし | なし | 履歴なしとして続く |
| 空（履歴だけ） | どちらかあり | — | HALTED、外で現れた履歴を採用しない |

台帳の書込失敗・記録除去失敗も未完了で、`CATEGORY_LEDGER_STALE` に丸めてplanを捨てない。
再試行は同じ完成後台帳を原子的に書き直す。成功した実体を巻き戻さない。
起動は従来どおりカテゴリ/ノート走査より先に復旧し、終わらなければ起動しない。

### 4. applicationの開始と採用

`folio_state_rename_category(state, target, units, count)` は対象を名指しする。
`category_rename_target` は対象添字と検証済み `category_name` を呼び出しの間だけ借りる入力で、
範囲検証と準備の所有はapplicationにある。2026-10-09のD2実装前にC-012の4引数上限へ合わせた補正で、意味と順序は変えない。
順序は `FILTERED` → 範囲 → 共通同期 → 完全同名no-op → ASCII大小を畳んだ名前衝突 → 必要な保存 → plan/空corpusの事前確保 → port。
完全同名は先行同期を除き、新しい改名の副作用も対象文書の先保存も起こさない。
大小文字だけの変更は衝突として断る。異なる文字体系のcase-onlyはOS側の衝突に従う。

ADR0039決定15の「開いている文書がそのカテゴリなら先に保存」を次のように限定する。

- 現在文書が対象カテゴリのNAMEDかつEDITなら、既存の保存/履歴を成功させてから改名する。保存失敗なら改名しない。
- 対象のUNTITLEDは保存せず、本文・未保存状態・Undoを保持する。カテゴリ添字は同じままで、新台帳採用後の初回保存先が新名になる。
- 別カテゴリの文書、NONE、対象のVIEWは保存も再読込もしない。UIが本文を取り出すのも対象NAMED+EDITだけ。
- 先保存が成功した後の準備失敗は、保存まで巻き戻したとは表示しない。

保留は既存のPENDING/HALTEDの両方でplanを保持し、同期入口が同じplanをRESUMEする。
新しい構造変更は先にこの同期を通るので、2件目の意図で上書きしない。
完了時は準備済みcategory ledgerを採用し、カテゴリ/ノートの添字・カーソル・順序・色・展開・現在文書・本文・mode・Undoを変えない。
この採用に新しい確保や本文読込を行わない。

### 5. corpus・下見・履歴の追随

カテゴリの改名開始は絞り込み中に断る。副作用前に空の `note_corpus` を確保し、CATEGORY planとともに保留できるようにする。
完了時に旧corpusを破棄して空へ交換し、`corpus_loaded=false`。次の絞り込みは新名を使う既存読込経路だけを通る。
flagだけを落として旧名のentryを残さない。この選択は全本文の複製を避けるが、次回は他カテゴリも再読込する。
以前読めた本文が再読込で失敗した場合は、既存規則どおりそのノートは一致しない。

ADR0039決定15の下見の追随は、**対象が現在文書のカテゴリなら完了時に下見を破棄し、入力値を保持して再計算を促す**契約へ補正する。
別カテゴリの現在文書の下見は保持する。保留中に新しい下見を作っても、古いsnapshotで上書きしない。
`REPLACE_STALE`は原因を本文だけに限定せず、「下見が古くなりました。もう一度入力してください。」
（`The preview is out of date. Type again.` / `预览已过期。请重新输入。`）として再入力を促す。
下見の有無を問い合わせる純粋な表示値を加え、UIは検索語が非空で自身の結果がREADYでも下見が無ければSTALEを描く。
空の初期入力にSTALEを出さない。
有効な0件と破棄済みを件数だけで見分けない。これにより、改名コマンド以外の同期経由で完了しても古い成功件数を残さない。

履歴の行は本文/版だけを持ち、カテゴリ文字列を持たないので、名前だけの採用では保持できる。
事前保存で新しい版が積まれたときは既存 `close_history` が先に働く。その場合、改名の成否にかかわらず、
開いている履歴面は既存の保存後と同じく閉じる。空の一覧だけを残さない。
別カテゴリの履歴は触らない。

**保留中の読込を止める境界を足す。** CATEGORY pending中の非空の絞り込み要求と、対象カテゴリの履歴再読込は、
cache構築/語変更/scroll変更/履歴の破棄より前に共通同期を通る。未完了なら既存の表示値を保ち、旧名を読みに行かない。
空の絞り込み解除、本文上の検索/置換のようなディスクを読まない操作、別カテゴリの履歴は継続できる。
既存のNOTE改名の下見契約はこの補正で広げて変えない。

### 6. 結果とUI

結果は既存 `rename_outcome` を使う。名前の衝突だけCATEGORYでは既存 `CATEGORY_NAME_TAKEN`、名前の不正は `CATEGORY_NAME_INVALID`。
ロック/対応外/識別子/記録/保留の文言をノートに限定しない表現へ直す。
「何も変えていない」は改名の実体についてだけ正しいため、先保存があり得る文言は「名前は変更していない」とする。
壊れた記録の文言は「版1の形ではない」をやめ、対応する版/形を読めないことと記録を残したことを伝える。3言語と全表を揃える。

入口は `FOLIO_COMMAND_RENAME_CATEGORY`（`renamecategory` / `rencat`、名前引数、一覧に出す）とカテゴリ行メニュー。
既定対象はC2と同じカーソル→現在文書→対象なし。行メニューは押した行。先頭fallbackは使わない。
先行pendingの確認は対象解決より前に行い、NONEかつカーソルなしからも既存意図の復旧面を開ける。
`NAME_PROMPT_RENAME_CATEGORY` は明示targetを保持し、一覧なしの面に元カテゴリ名を選択状態で出す。
名前引数があれば面を開かず、カテゴリの型を作って同じ対象付き受け口へ渡す。

`rename_view` にkindを加える。未完了フォームは実際に保持する意図のkind/from/toを表示して入力を固定する。
fromは元の名前であり、まだその名前に実体がある保証ではない。
公開 `folio_state_retry_rename` は共通同期だけを行い、ノート名を再構築せず、別の保存を始めない。
既存の名前面のretryもこれへ統一する。NONE/UNTITLEDからでも保持中の意図を再開できる。
通常の初回保存/別名保存/ノート改名/カテゴリ改名の面を開く前に先行pendingがあれば、実際のkindを示す復旧専用面を先に出す。
READYで復旧した場合だけ、対象・初期名・一覧を取り直して、元の要求の通常面を新しく開く。
復旧だけで元の保存/改名要求まで済んだと扱わない。閉じた場合や失敗時は元の要求を始めない。
復旧後に対象が無ければ通常の対象なしを返す。名前引数付きの要求にも同じ先行確認を使い、引数を別kindの再試行へ読み替えない。
一覧の有無と配置が異なる面のkindを、開いたまま書き換える経路は作らない。
今回のNOTE/CATEGORY改名STARTから生じたpendingだけは、その面で実際のkind/from/toを固定し、再試行READYで閉じる。
初回/別名保存面の表示後に想定外の先行pendingを検出した場合は、入力名と選択カテゴリを保持して失敗を示す。
入力を `fill_pending` で上書きせず、そのsubmitでは保存しない。NEW_CATEGORYは既存どおり保留面へ化けない。
再試行後は借用した旧viewを使わず、状態を取り直す。特に既存NOTEの採用後にcache確保が失敗し、
planだけ既に消えていた場合は失敗を示して終了し、存在しない意図の固定再試行面を残さない。
先行復旧専用面なら元の要求は未実行である。今回の改名STARTからの面なら改名そのものは既に完了しているので、未実行とは扱わない。
未完了の説明は、現在のkindと実在する再試行入口を指し、カテゴリ意図をノート改名と呼ばない。

完了後にRichEditをstream-inしない。パンくず・一覧・必要な下見/履歴表示だけを更新する。
パンくずの復旧表示はboolを `NONE` / `NOTE` / `CATEGORY` の閉じた種別へ置き換える。
対象カテゴリを持つ文書のパンくずは、保留中にはカテゴリ部を「復旧待ち」（`Recovering` / `等待恢复`）と表示し、ノート名は実名を保つ。
幅の計測と描画には同じ表示名を使う。実名の幅だけを測って別の文言を描かない。
別カテゴリの文書のパンくずは変えない。NONEでは文書を表示しているような仮のtitleを作らず、保留は名前面/失敗表示で伝える。
本文のUndo・mode・入力欄・有効な元focusを保ち、面の成功/取消/再試行後は既存のフォーカス規則へ戻す。
新しい成功通知・確認箱・キーは足さない。

### 7. 実装単位と検証

C1 → C2の統合後、次の順で進める。各単位は独立のIssue/PRとし、最終productionで単一ゲートを実行する。

1. **D1: 共通plan・版2記録・adapter。** category ledgerの合成read/renamed、NOTE planからrename_planへの移行、v1読取/v2書出し、
   typed portと全fakeの置換、カテゴリの実体処理/復旧を実装する。既存NOTEのapplicationを新しい型へ機械的に接続する。
   CATEGORYはまだUIから要求できないが、起動の復旧は両kindを正しく扱う。未接続のCATEGORYをNOTEとしてdereferenceしない。
2. **D2: カテゴリ改名のapplication。** 対象・保存範囲・準備・単一保留・採用・cache/preview/history・読込前の同期・retry/viewを実装する。
3. **D3: コマンドと名前面。** 行/既定対象、入力と保留、3言語、本文と面の保持、README/SPECIFICATIONのFR-037を実態へ合わせる。
   D3で親#169の4操作を完了とする。

対象検証はkind codecの往復/不正/未知版/out保持/確保失敗、台帳の色・順・展開、v1既存note復旧と新規v2note、
カテゴリの履歴有無/非空/各途中段階/両側存在・不在/ID不一致/台帳・記録削除失敗/同意図再試行/記録不在RESUME/ロック/リンク/長いパス。
applicationはNONE/NAMED VIEW/EDIT/同本文/UNTITLED/別文書、no-op/衝突/範囲/filter/同期、準備OOM、保留後のfilter/history/preview、
どの同期入口から完了しても同じ採用になることを測る。UIは元名/保留表示/再試行/取消/Undo/focus/下見失効/履歴を測る。
改名の共通基盤を置き換えるため、既存NOTE改名と復旧の直接影響部分は対象検証に含む。
B/Cの削除・ごみ箱・通知や、無関係な成功済み実機probeを繰り返さない。

## 限界と却下

- 保証するのはプロセス異常終了後の再開。電源断/OSクラッシュの名前空間耐久性は未証明。
- 同期サービスの競合・オンラインのみの取得・exeまでの祖先差替え・全外部競合は解決したと扱わない。
- ネットワーク/非NTFS/照会不能、name surrogate、case-onlyの改名を対応範囲へ足さない。
- 別journal、別pending owner、カテゴリごとのノート改名ループ、コピー削除、失敗時の巻き戻しは採らない。
- cache全本文複製とpreview snapshotの保留越し採用は、費用と古い下見の復活を避けるため採らない。
- ゲート・閾値・抑制・依存は変えない。Waivers: none。

## 設計確認

独立の静読でP1/P2なし。報告は `D:/NeNeFolio/agents/204-rename-design/review.md`。
ClaudeCodeのデザインリナによる `design:design-critique` / `design:ux-copy` と議論し、
先行復旧と通常保存の分離、NONEからの復旧入口、カテゴリ部の幅計測、空の下見と履歴面を上の契約へ取り込んだ。
記録は `D:/NeNeFolio/design/2026-10-08/category-rename/report.txt`。これは設計の受理で、D1以降の実装・実測の受理ではない。

## OS仕様の参照

ハンドルで最終パスを解くAPIはvolume GUID形式を返せるが、ネットワーク共有にはvolume GUIDが無い。
この照会失敗をローカルNTFSの拒否へ結ぶのは本ADRの設計判断である。
[GetFinalPathNameByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew)

改名は既存の `ReplaceIfExists=FALSE` を維持し、競合した新しい宛先を置き換えない。
[FILE_RENAME_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)
