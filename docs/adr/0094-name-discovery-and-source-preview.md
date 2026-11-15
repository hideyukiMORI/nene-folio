# ADR 0094 — 名前検索と原文プレビューを既存engineと文書選択へ結ぶ

- 状態: 受理（production変更前の独立読取レビュー2点を反映）
- 日付: 2026-11-15
- Issue: #823
- 関連: #811、ADR0073/0074/0090/0092/0093、FR-032/038/045/046
- 規則: ARC-001/003/004/005/007/008/009/010/011/012、C-002/003/004/005/007/008/012/013/014/016/017/018、QLT-008/009/010/012/013、GIT-001〜004

## 問題と範囲

常設一覧の部分一致は候補を開く前に本文を見られず、名前の離れた文字を使って探せない。
段階3aは「ノートを探す」の名前モード、保存済み原文の有限preview、明示的な開く/閉じるを一つの面へ加える。
常設一覧の名前+本文の部分一致は維持する。本文live grep/一致行への移動は次の焦点Issue、演出は段階4とする。
任意folder、外部検索process、watcher、別本文台帳、版上げ/配布/Storeは含めない。

背景Claudeの読取専用design相談を参照したが、実機確認や仕様の自動受理とは扱わない。
条件の写し、空語から開始、狭幅の切替、完了後の一括表示を採る。
相談にあった「お気に入り優先を無効」は採らず、hideの希望と計画に従い既存設定を引き継いで条件として表示する。

## 入口と条件の所有

1. 共通コマンド/操作メニュー/パレットから開く。新しいglobal shortcutや一覧検索欄の暗黙のmode変更は加えない。
   名前付きEDITは既存の共通保存が成功してから面を開く。保存失敗なら本文/Undo/選択/scrollを保持して開かない。
   無題は保存を強制せず、対象にも加えない。入口で成功した保存は取消時にも巻き戻さない。
2. `folio_state`が一回の検索sessionを所有する。検索語は常に空で始め、一覧語を持ち込まない。
   星scope、選択tagsの所有copy、ALL/ANY、note_orderを開いた時点で写す。
   条件行に「この検索だけ」と適用内容を表示する。面内の解除はこのcopyだけを変え、一覧へ書き戻さない。
   初期実装は星と各tagの解除/全解除に限り、tag追加/ALL-ANY切替/カテゴリ選択を面の入れ子で増やさない。
   優先表示は絞込条件ではなく順位設定であり、条件全解除でも開いた時の設定を維持する。
3. 現在rootの登録済み全カテゴリ/保存済みノートを対象とする。検索結果はノート単位で、カテゴリだけの行を作らない。
   星の変更、改名/移動/削除などの文書mutationを検索面に重ねず、既存の「どこでも」操作で別操作へ移る際は面を閉じる。

## 一つのmatcherと順位

`index_filter_query`へ閉じたscope（従来のNAME_BODY_SUBSTRING / 新しいNAME_FUZZY）を追加する。
`index_filter_build`だけが比較と順位構築を行う。旧一致関数や別の全文engineは増やさない。
entryは既存のref/name/body/tagsに、カテゴリ名と星を値として添える。カテゴリ名の省略は従来scopeの入力に限って意味を持つ。

新scopeは次の辞書式順位を使う。

1. FAVORITES_FIRSTなら星ありの群、その後に星なしの群。MANUALならこの差を付けない。
2. ノート名の完全一致、前方一致、部分一致、部分列一致、カテゴリ名の同じ4段階の順。
   ノート名に一致したものを、カテゴリ名だけに一致したものより先に置く。
3. 同じ群/段階では既存カテゴリ順、その中の元のノート台帳順。結果を台帳へ保存しない。

ASCIIだけ大小を無視し、非ASCIIのcase fold/Unicode正規化はしない。
部分列一致は、語のUnicode scalarが名前の中に同じ順で存在することを意味する。
語の空白や句読点も文字として扱う。名前とカテゴリを連結して一致を作らず、拡張子の扱いは台帳名の既存契約どおりとする。
最小gapやfzf互換scoreは主張しない。行には一致した側と段階を3言語で説明する。
空語は名前を調べず、条件に合う全ノートを星群/台帳順で出す。新scopeでは空語でも完成結果を作り、従来scopeのNO_TERM契約を変えない。

従来scopeのBODY欠損は従来どおり名前によらず非一致。
新scopeはtag条件がなければ本文の存在/可読性に依存しない。未読を読取失敗と数えず、preview/Openで読めないことを表示する。
tag条件がある場合は共有corpusで取得したmetadataに一致するノートだけを対象にする。
読取不能と対応外metadataを件数として別に示し、完全な0件と混同しない。

## 有限workと完成集合

- 部分一致は既存のtail/prefix比較を共有し、一致開始byteから完全/前方/部分を決める。
  そのsourceで部分一致がなかった場合だけ同ownerの部分列phaseへ移る。
  部分列の一tickはsourceのscalar一個とqueryのscalar一個のdecode/比較を行う（各最大4byte）。
  名前の長さ、カテゴリの長さは既存と同様にbyte/NUL単位で計上する。
  sourceの切替/SEALED/term借用解除は同じ経路で行い、別のmutable ownerは追加しない。
- 一致したrefは既存の台帳順番号列へ入れる。既存の二分探索とremovedの意味を壊す並べ替えは行わない。
  新scopeだけが全一致のkind/favorite/次番号を所有し、星群×9段階（空語+8一致段階）の固定18 bucketへ台帳順に連結する。
  bucket末尾への一回の追加はentry確定の有限処理とする。全entry評価後、bucket順に最大200件の順位projectionを作る。
  bucketを越えるか一件を投影するたびに一work使い、投影の終了までREADYにしない。
  同順位はbucket内の入力順で安定する。上位から外れた一致もtotal件数へ含む。
- `index_filter`は従来の全一致番号列を引き続き所有し、完成済みの順位projectionとtotal/表示件数を読み出す限定APIを持つ。
  projectionをmutable配列として返さない。従来のnote/category二分探索は全一致番号列を使う。
  removedはscope/順位設定と全一致のmetadataを保持し、番号写像後に同じbucket追加/投影helperを使う。
  201件目以降からの昇格を省かず、元のSEALED ownerは変えない。検索面自体は台帳変更時に再検索する。
- 保持費用は既存の2列×(N+1)×sizeof(size_t)、新scopeの全一致metadata、語prefix、固定200件の順位projectionである。
  200は画面/順位projectionの上限であり、全一致番号列や全文corpusのメモリ上限ではない。
  台帳件数と配列積の表現可能性を確保前に検証し、OOMなら失敗を示して古い結果を新語の結果として残さない。
  検索語の入力上限は256 Unicode scalar、UTF-8最大1024byteとし、超過を途中切断せず面内で拒否する。
- applicationの既存65536 work/idle、一本文最大一回read/idle、通常message優先、IME/modal中の抑止を使う。
  count/確保/語前処理/corpus lookup/OS read/UIの費用は比較workの外で、実時間の上限は主張しない。

## corpus、取消、世代

tag条件なしでは名前検索のために全本文を先読みしない。候補のpreview要求だけを遅延して読む。
tag条件があるsessionの開始では既存のcorpus loaderを新しい一巡として使い、完了後にmatchingへ進む。
同session内の語変更はその一巡を再利用し、未完成なら読込位置を保つ。
tagを全解除すれば全本文の準備を待つ必要はないが、既存一覧の要求を壊すためにcorpusを完成済みと偽らない。

既存`load_corpus_note`をref入力の共通private read/storeへ分解し、順次準備と一件previewで共有する。
読み込んだnote_textは同じcorpusへ渡し、別の本文cache/port/file readerを作らない。
previewのread/storeで既存corpusの内容を差し替える場合は、put/removeより前に同じ入力へ依存するbuildを破棄する。
一覧のindex_build/完成filter/tag_catalogは常に対象とし、discoveryのbuild/完成結果はsessionのtag条件が有効な場合に対象とする。
tagなしの名前検索はbody/tagsを入力しないため、その結果は本文差替えから独立している。当該previewだけを更新する。
依存判定はsessionが所有する条件で行い、sealで解除されるbuildの借用tagsから推測しない。
共通通知は一覧語/tagsと検索面の要求を保って再構築し、store/ledger改訂が変わらない外部読込にも適用する。
tagなしの名前entryにはbody=nullptr/length=0/tags=nullptrを渡して継続入力を不変にする。
台帳/保存改訂とquery/条件変更ではtagの有無にかかわらずdiscoveryを失効させる。
一回の検索sessionで取得済みの本文をpreviewへ再利用し、外部変更は自動監視しない。
面の再開は必要なtag corpusの新読込、明示Openは既存の文書読込となる。
名前検索には本文一致座標がないため、以前のpreview位置を新本文へ適用しない。

要求語/条件変更、台帳識別改訂、保存改訂でbuildと旧結果/previewを先に失効させる。
queryとtagsのownerを破棄する前にbuildを破棄する。同じ長さの語変更でも進捗を再利用しない。
matchingは一括完成公開とし、途中件数を最終件数として出さない。選択/previewは完成結果の世代に結び付ける。
選択が変われば旧previewを直ちに消す。同じノートが新結果に残れば識別で選択を保ち、消えれば先頭を選ぶ。
UIAの旧行識別は補助面の既存世代で失効させる。applicationは候補集合の改訂を選択/preview改訂と分けて返す。
候補移動だけでは同じSTATUSを作り直さず、同じ最終件数のlive通知を繰り返さない。

## 原文preview

新しい不変値`note_excerpt`は検証済み`note_text`から先頭の最大80論理行か最大16KiBの範囲を導く。
CRLF/LF/CR、末尾改行、空行、UTF-8 scalar境界を保ち、上限でCRLFの間やscalarの途中を切らない。
prefixの元byteと論理行位置を所有し、入力を変更しない。上限の理由と表示範囲を値として返す。
空本文も正常なpreviewとする。UTF-8変換と有限長native表示は投影であり、保存対象にしない。

見出しはカテゴリ/ノート名、本文は読み取り専用の原文、行番号は別の列とする。
front matter/リンク/画像参照/コードも文字として表示し、画像やURLの取得・コード実行・Markdown描画は行わない。
nativeの読み取り専用text面からkeyboardでscroll/copyでき、Tabで出入りできる。
元文書のnative pane、選択、Undo/Redo、scrollはpreviewの移動/表示/取消では変更しない。
未読/取得中/正常/読取不能/OOMを区別し、旧見出しの本文を新見出しへ流用しない。

## 確定と取消

単クリック/上下/Page移動は候補選択とpreviewだけ。変換中でないEnterまたは「開く」だけを確定とする。
検索中/0件/古い世代の候補は開かない。現在の名前付き文書と同じ候補なら既存FR-038に従い再読込せず閉じる。
別文書は唯一のswitch_note/folio_state_select_noteへ合流し、別の本文採用/保存経路を作らない。

無題の離脱promptが台帳を変え得るため、確定時のカテゴリ名/ノート名を所有copyとして保持する。
不変の`note_identity`が検証済みのカテゴリ名と台帳stemをそのまま所有し、拡張子を再解釈しない。
共通のsave_editが成功した後にその名前を現在の台帳へ再解決する。
消失/改名/対象不明なら面を残して理由を示す。古い番号で別のノートを開かない。
この再解決を既存のswitch_note内部の宛先確定へまとめ、検索面だけが文書切替処理を複製しない。
取消や保存失敗なら面/元本文を維持し、成功時だけ面を閉じて既存のreflect_noteを使う。
native投入拒否は既存UNAVAILABLE契約に従い、成功した本文採用を古いnative bufferへ巻き戻さない。

Esc/閉じるは検索sessionだけを破棄して入口のfocusへ戻す。検索語と解除条件を一覧へ残さない。
IME未確定中は語を更新せず、Enter/Esc/矢印/マウス確定を奪わない。確定後に一回だけ最新語を受理する。

## 配置・UIA・検証

共通command surfaceへDISCOVERYを加える。上に題/名前入力/条件/状態、下に操作、残りが結果とpreview。
十分な幅では左右二列、狭幅では結果/preview切替とする。未実装の本文modeは表示しない。
560×360 DIP、3言語、明暗/高コントラストで入力・状態・開く/閉じるを残す。
条件は狭幅で要約し、全内容をUIAへ公開する。色に加え選択印/枠と文字で処理と失敗を示す。
結果は単一Selection、previewは読み取り専用textとして公開し、通知は完了/0件/失敗に限定する。

固定名と独立したscalar oracle、全順位と同点、星/タグ、Unicode反例、0/1/不均等work、上位200、total、
SEALED/removed、全確保失敗点、cold/warmと取消/台帳世代、excerpt境界、native本文不変/無題/確定/IME/UIAを確認する。
初回readとwarm matching、preview/native表示を分けて外側で計測し、他案件の負荷がある場合は条件を記録する。
関係入力が不変な成功済み試験は再利用し、最終headの必須CI、正典merge、通常exe反映と実data照合、
全証跡の恒久保全後に今回823の追加worktree/outputsだけを監査して整理する。branch/commitは保持する。

Waivers: none（追加なし、既存WVR-0001継続）。規則のactive/planned、ゲート閾値/除外/抑制は変えない。

独立読取レビューの採用記録はD:/NeNeFolio/agents/discovery-design-20261115/contract-review.md。
corpus更新前の両build破棄と完成filter/catalog失効、removedの全順位metadata/201件目昇格を追記して受理した。

16:19のapplication着手前の補足読取レビューでは、実装済みNAMESのtagなし経路がbody/tagsへ依存しないことを確認した。
全候補をpreview選択のたびに無関係に再照合するのを避けるため、上記の依存による失効へ精密化した。
