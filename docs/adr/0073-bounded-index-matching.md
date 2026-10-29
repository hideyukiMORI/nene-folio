# ADR 0073 — 全ノートの一致計算を同じengineの有限workで継続する

- 状態: 受理（実装前の独立読取レビューとARC-005の限定更新を反映）
- 日付: 2026-10-29
- Issue: #428
- 規則: ARC-001/004/005/007/010/011/012、C-002/003/004/005/007/008/011/012/013/016/017、QLT-008/009/010/012
- 関連: ADR0024/0068、FR-032

## 根拠と目的

main `9d611cf`のwarm入力と初回corpus完成時には、一回のUI呼出し内で全entryの名前/本文を一致計算する構造が残る。
[2026-10-25の記録](../quality/2026-10-25-first-search-responsiveness.md)の合成10,000件×10KiBでは、
warm入力73〜108ms、初回末尾filter68〜75msだった。現在headの再測定値ではない。
一致計算を途中で返し、既存loopで通常messageを処理する機会を作ることを目的とする。

比較workの上限とmessageの処理順を決定的に確認する。65536は最適値やms保証ではなくapplicationの政策値。
他案件の負荷が続く間は性能計時しない。新たな速度倍率・入力全体の時間上限・初回I/O短縮を受理根拠にしない。

## 正典のengineと所有

1. coreの`index_filter_build`が一つのmatcherと構築進捗を持ち、`index_filter`は完成集合の不透明なownerとする。
   2つの主要型は別の.cへ置く。旧同期`index_filter_create`、`index_filter_query`、全entry借用配列、
   旧contains/matchesの独立実装を同じ変更で撤去する。分割用と同期用にmatcherを二つ持たない。
   ASCII fold、UTF-8 byte比較、name優先、body欠損はnameによらず非一致、台帳順と二分探索の意味を維持する。

2. buildだけが構築中の比較phase/数値位置・番号列・scratchを変更できる。
   ARC-005へ`src/core/index_filter_build`というowner限定の区画を先に追加し、applicationには限定操作を持つopaque handleだけを渡す。
   可変配列/内部record/書込viewは公開せず、他のcore/application ownerへの一般的な可変性許可にしない。
   同じ初期状態・不変入力・work列から同じ結果になる決定性を守る。時刻/環境/I/O/UI状態をcoreへ入れない。
   完成済みのSEALED ownerは再び可変に戻らず、stepを受けても書込・entry/term参照をしない。

3. buildはterm bytesだけを構築中不変borrowする。entry/name/bodyはstep呼出中だけborrowし、idle間にpointerを保持しない。
   名前/本文/refの変更は、長さが同じでも次step前に進捗を破棄する。SEALEDへ移る際にterm borrowを解除する。
   create時count0もSEALEDで、偽entryをstepへ渡さずfinishできる。SEALEDのquery/removed/finish retryはterm寿命に依存しない。
   数値進捗と借用が必要なのは構築中だけであり、callerの別名から完成後に編集できるAPIを加えない。

4. 完成filterはSEALED buildを所有して番号配列の全件copyを避ける。
   256要素のskip表等の約2KiBが完成結果の寿命まで残り、finishでwrapperを1回確保する費用を受け入れる。
   buildの所有objectと番号2配列の確保を一つのprivate構築経路へ集約し、通常createとremoved変換で共有する。
   nullable localで確保成功を判定してから必須memberへ渡す。失敗時outと入力ownerは不変にする。

## 公開操作と前提

| owner | 操作と契約 |
| --- | --- |
| build | create(term, length, count, out): 空語→不正UTF-8→容量の順で検証し、失敗out不変。終端用1要素を含むsize_t配列を表現できないcountは確保前にOOM |
| build | step(build, entry, remaining_work): MORE / ENTRY_DONE / ALL_DONEの閉じた結果。構築中は同entryを完了まで再解決して渡す |
| build | readiness(build): NOT_READY / READY。部分件数を完成件数として扱わない |
| build | count(build) / ref(build, index): READYが前提。refにはindex<countも必要。前提違反を0/nullへ隠さない |
| build | removed(sealed, target, out): 旧SEALEDを変更せず、削除/後続番号写像を済ませた新SEALEDを成功時だけ返す |
| build | destroy(build): nullableを許す、term/name/bodyを解放しない |
| filter | finish(build**, out): NOT_READY / OUT_OF_MEMORY / ACCEPTEDの閉じた結果。失敗は両out/owner不変、成功時だけ*build=nullptrとして所有移譲 |
| filter | removed(filter, target, out): 既存の削除前予約。bridge成功のSEALED保証から同じprivate wrapper構築を呼ぶ |
| filter | note / category / count / destroy: 完成結果の照会と破棄。note/categoryは既存の二分探索 |

7+6の13操作を採る。API数だけを減らすためにcountをfactoryの第二outとgetterへ分けない。
結果enumは意味ごとに別headerへ置く。finishのNOT_READYをOOM/空成功へ写さない。
finishは所有中の非null buildを渡すことが前提で、成功後のaliasは使わない。
removedは新buildのwrapper確保失敗ならそのlocal buildを破棄し、旧filter/outを保持する。
createの検証順と容量上限は#418を維持し、最大fitの試験では過大な実mallocを行わない。

## workとapplicationの実行

1. workはentry開始、nameの各byteとNULの長さ確認、1文字比較、一般語のtail/head/完全比較各byteに1ずつ使う。
   nameのstrlenや完全比較を予算外で一括実行しない。比較途中位置を保存し、再開で先頭を反復しない。
   無料のphase遷移は同じstep内で進め、正workでMOREならworkを実際に消費する。
   work0では未完成entryを進めない。有限入力はwork1反復でも完了する。SEALEDは予算にかかわらずALL_DONE。

2. `folio_state`はpending buildと既存`note_ref`の数値cursorを持つ。
   同期refreshは同じbuild/cursorをcall-localで持ち、単一のprivate
   `step_filter(state, build, cursor*, work*)`でentry解決・cursor前進を共有する。
   entry解決で別の先払いworkを使わず、空categoryを越える時だけapplicationで1workを使う。
   ENTRY_DONE/ALL_DONEで残work0になってもその呼出し内でcursorを前進させ、同entryを重ねてadmitしない。
   一件一idle固定にせず、残workで次entryへ進む。

3. 入力と初回corpus完成後は、既存idle continuationに一回65536 workを渡す。
   corpus未完成なら従来どおり最大一ノートを既存portで読み、完成後にmatchingへ進む。
   matchingの最初の継続で件数を集計・buildを作成し、同じ呼出しからworkを使う。count0はそのままfinishする。
   countの加算と配列容量は確保前に確認する。O(categories)初期集計を独立した長期phaseにはしない。
   確定filterのsave/rename修復は同じhelperを同期drainする。公開continueへ再入せず、別I/O/iterator/matcherを作らない。

4. 同じUI threadを維持する。queue先行・空queue時だけ継続という既存loopと、
   close/session/native modal/IME等の既存実行可否を使う。製品のthread/timer/入れ子pumpは追加しない。

## 観測契約と無効化

- corpus完成済みのwarm入力も、先行同期と語変換成功後に旧確定集合/語を外し、最新requestと既存「準備中」の全索引へ移す。
  現在文書/本文/mode/Undoを保持し、開始時cursorは移さない。新語受理時と完成採用時のscroll0は既存pendingへ揃える。
  完成時だけ既存settle_cursorと結果表示を行う。部分結果を見せず、準備中の全索引を検索結果とは表示しない。
- 不正語/語確保失敗は前の要求/結果/scrollを保つ。先行同期が成功した後ならその修復済み結果を保つ。
  語受理後のbuilder/finish OOMはrequestを止め、全索引のまま既存失敗通知を一回行う。旧filterへ戻す別snapshotは持たない。
- 新有効語は未完成corpusの読込位置を保ち、matching進捗だけを破棄して置換する。clear/destroyはbuild→termの順で破棄する。
- `index_changed`と保存copy更新に使う既存restart入口でpending build/cursorを破棄し、requestは保持する。
  corpus未完成なら読込位置0、完成済みなら再読込せずmatchingをやり直す。loader自身のputでは自身の位置を巻き戻さない。
  mutationの失敗後も変更済み入力があれば次step前に無効化する。同期修復前に取得したbuild pointerを修復後へ持ち越さない。
- finish成功ではpending build/requestをstateからdetachしてからadopt/index_changedする。新結果を自己無効化しない。
- 確定filterの保存/改名後の同期再構築、#219のcopy_pending、失敗時の旧結果/term/scroll保持、
  削除前のremoved予約は維持する。pending mutation中には確定filterが無いため同期matchingを追加しない。

## 限定と却下案

語変換/前処理、O(categories)集計、配列確保、corpus lookup、OS read一回、先行同期修復、結果のcursor/UIA/描画処理はworkの実時間上限の外。
`filter_input_changed → refresh_index_filter → drawer_window_reveal_cursor → folio_state_reveal_cursor → folio_state_reveal_row → folio_state_drawer_layout`
にはcursorがある場合の同期layout構築が残る。描画cacheの再利用だけでは消えない。
warmの狭い結果から全索引へ戻すことで、展開状態次第ではこの費用が増える。入力全体を一定時間以内にしたとは呼ばない。

- 全mutationをidleへ移す案は#219の同期修復と削除前予約の契約を広く変えるため採らない。
- 完成時の全件copy、公開mutable配列take、旧同期matcherをfallbackとして残す案は採らない。
- countをfactory第二outへ出す12操作案より、ownerから一貫して完成metadataを読む13操作案を採る。
- core全体をmutable区画にする案、既存短命builderを根拠に規約更新を省く案は採らない。
- 背景I/O、未計時memchr候補、drawer cache改善、保存先設定を混ぜない。

## 検証と規範の移行

旧sourceに対するwarm巨大一件のset直後pending/初回matching未完成の限定redを保存する。
新core APIは旧sourceに無いため、そのunitに旧binary redを装わない。
完成候補では既存具体集合と0/1/2/不均等/65536 work、tail/head/完全比較途中、name終端、欠損、count0、空category、
entry完了時残0、appの正確な65536配線、最新語/clear/mutation/#219/removedを対象確認する。
既存selectorと全体runは同じfixtureを使う。所有と全新規確保失敗点・SEALED retry・out不変を同Debug sanitizerで確認する。

専用desktopの合成warm一件で実loopを使い、正workを使った未完了matching直後へ制御messageをPostする。
通常message、clear、close、native modal抑止→再開の4ケースだけを独立に確認する。
step/dispatch/read/workの整数event順を記録し、Sleep/timer/実時間のタイミング探し・物理入力・自作loop・製品test seamを使わない。
modalのcontrollerは自身のowner/caption/threadのdialogだけを扱う。窓/adapter/stateを解放し元thread desktopへ戻してから専用desktopを解放する。
計画が成立しない場合は失敗として残し、時間依存の試験を決定的成功へ読み替えない。

ARC-004/005、C-013、PROJECT_LAYOUT、GLOSSARY、ADR0024/0068、公開API注記を同じ変更で更新する。
ARC-005/C-013はplannedのまま。機械強制の不存在をactiveへ変更せず、既存CNF/compile/graph/sanitizerの強制を維持する。
旧API/header/symbolの撤去を確認し、旧warm同期完了経路を拒否する回帰fixtureを同じunitへ収載する。
新port/schema/依存/ゲートの閾値/除外/抑制は追加しない。最終PR headの必須CIは既存`eng/check.ps1`。

独立読取レビューは`D:/NeNeFolio/agents/quality-20261029/search-adoption-review.md`。
候補の比較数上限、所有、layout限定、ARC-005例外の不足を親が確認し、本ADRと正本更新で採用範囲を固定した。
対象実行は新run-dirのrecord-runで入力/exe/raw出力/終了値を保全し、結果は品質記録へ残す。
統合/通常反映/恒久保全後、新作業木/出力だけを安全監査して整理する。削除拒否35箇所は再試行しない。

Waivers: none
