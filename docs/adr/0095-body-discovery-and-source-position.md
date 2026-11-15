# ADR 0095 — 本文の最初の一致を同じ原文位置で確認して開く

- 状態: 受理（production変更前の独立読取レビュー1点を反映）
- 日付: 2026-11-15
- Issue: #825
- 関連: #811、ADR0023/0026/0073/0074/0092/0094、FR-032/038/047
- 規則: ARC-001/003/004/005/007/008/009/010/011/012、C-002/003/004/005/007/008/012/013/014/016/017/018、QLT-008/009/010/012/013、GIT-001〜004

## 問題と境界

名前検索は原文の先頭を見せられるが、本文の一致行と原文内の位置を持たない。
段階3bは同じ検索面へ本文モードを追加し、保存済み原文の最初の一致を結果、preview、明示的な確定へ結ぶ。
既存drawerの名前+本文検索、名前モード、保存/文書切替、corpusの所有経路を置き換えず、それぞれの唯一のengineを拡張する。
外部folder/process/watcher、regex、本文fuzzy、全出現列挙/次前一致、未保存の無題、演出、About、配布/Storeは含めない。

背景Claudeのdesign相談を参照した。入力行内のmode、各noteの最初の一致、EDITでの確定、snapshot不一致時の非移動を採る。
提案の寸法・UIA動作は実機確認済みとは扱わない。語は最大256scalar/1024byteの単一行なので、previewで一致全体を確保できる構成にする。
その条件では「一致そのものが16KiBより長い」状態を作らず、長い原文行の省略と区別する。

## モードと入力

`folio_state`が閉じた`discovery_mode`（NAMES/BODY）を所有する。面の開始は従来どおり名前・空語。
モード切替は所有する確定語、星scope、tags、ALL/ANY、priorityを保ち、旧build/結果/previewを先に失効させる。
一覧へ書き戻さない。名前の空語は従来どおり全候補、本文の空語は独立したINPUT状態で入力を促し、結果0件とは言わない。

本文語はASCIIだけ大小を無視するリテラルsubstringとする。原文のfront matter/code/URL/空白も通常の文字として探す。
Unicode正規化/非ASCII case foldingはしない。空白だけの語は非空語として扱う。
CR/LFとNULは単一行の語として拒否し、上限超過を切断しない。query変更時には常に旧結果を消す。
nativeの単一行入力が改行付きpasteを切り捨てる前に拒否し、その拒否も面内の理由へ反映する。
IME未確定中はmode切替/確定/候補移動を実行せず、確定済みの語だけをapplicationへ渡す。

## matcherと位置の所有

`index_match_scope`へBODYを追加し、`index_filter_build`の既存substring比較をそのまま使う。
BODYは名前/カテゴリを調べず、欠損本文を非一致とする。tag条件の有無とは独立に読取不能を表示する。
対応外metadataでもtag条件が無ければ原文を検索する。tag条件があるときだけ、そのnoteを判定不能として除外する。
空のBODY語はtagの有無にかかわらずNO_TERM。NAME_BODYの従来契約とNAMESの順位は維持する。

BODYも全一致の台帳順番号列と最大200件のprojectionを持つ。順位は星優先が有効なら星群、その中は台帳順。
名前scoreを本文へ流用しない。新しいBODY_FIRST kindで説明し、一致回数や「良さ」を主張しない。
全一致metadataを保持し、removedの番号写像・201件目以降からの昇格も同じadmit/projection経路で行う。

最初のsubstringを見つけた後、同じbuildがPOSITION phaseで先頭から一致末尾まで原文を測る。
一tickはUTF-8 scalar一個またはCRLF一組（最大4byte）を処理し、一workを消費する。
測るものは原文byteの半開範囲、RichEditのUTF-16半開範囲、1始まり論理行、一致行の開始byte、最大20行前の開始byte/行番号。
前行の開始位置は固定21枠だけで保持する。全結果へ保存するのはこれらの座標値であり、原文やqueryへの新しい借用ではない。
一般の検索完了集合を作る前に位置も完成させる。0/1/不均等workによって一致や位置が変わらない。
位置を持つownerは従来のbuild/filterのみ。公開する座標の投影値は既存note_byte_span/note_search_spanと同じC-003の座標例外とし、可変なownerや配列を返さない。

原文を一scalar進め、CRLF/LF/CRを一つの段落単位として数える処理はcoreの`note_source`へ集約する。
既存note_metadataのprivate native_point/native_lengthをここへ移し、タグ編集も同じ処理を呼ぶ。
sourceのraw byteは変更しない。編集用位置では改行一つをUTF-16一単位、標準EDITのpreview用位置ではCRLF二単位として射影する。
UTF-8 decodeと補助平面の二単位計数を複製しない。既存Markdown plain RTFの一段落化とnative GT_RAWTEXTの契約をnative試験で照合する。

費用は既存の全一致番号列/metadata、固定200projection、固定21行位置、queryprefixを含む。200は全corpusのメモリ上限ではない。
byte範囲と積の表現可能性を確認し、OOMでは旧結果を残さず失敗とする。位置測定は比較workに含むが、同期read/確保/lookup/UI投影は実時間上限を持たない。

## 新しい一巡と再利用

面を開いた時のtag準備、または最初の非空BODY要求のうち、先に必要になった方がsessionの新しい全本文一巡を開始する。
tagなし名前modeで少数をpreviewしただけの場合は、最初のBODY要求時に新しい一巡を始める。
tag準備が既に同じsessionの一巡を始めていれば、その途中位置/完了結果を利用する。
modeを往復しただけで再読せず、語の変更でも同じ一巡を使う。空BODY入力では新規一巡を開始しない。
sessionを閉じて再開したとき、または台帳/保存改訂で失効したときはこの一巡の権利も失効させる。

所有する本文は既存note_corpusだけ。read/storeは同じload_corpus_note/prepare_corpus_next、最大一本文/idle。
途中で名前previewが同じ一巡の未読noteを取得した場合、共通の続行はその観測を再利用して二重readを避ける。
管理操作などが明示的に新しい一巡を要求するときは、全noteの観測を先に未読へ戻し、古い取得済み状態を持ち越さない。
corpusのput/remove前に依存build/完成結果を破棄する共通通知は、BODYまたはtag条件を持つdiscoveryを対象にする。
tagなしNAMESは本文へ依存しないまま。BODYで準備途中でも、名前へ戻ればtag条件が無い限り本文一巡を待たず名前照合できる。
既存一覧の需要を壊すためにcorpus_loadedを偽らない。全体一巡の開始済み/完了と選択previewの取得済みを区別する。
変更途中の候補は公開せず、結果集合改訂とpreview改訂の分離を維持する。

## 有限previewと一行抜粋

`note_excerpt`の唯一の生成処理をprefixと一致周辺の要求へ拡張する。名前modeのprefix契約は維持する。
本文modeは最初の一致行の最大20行前から、合計80論理行/16KiBまでを所有copyする。
この開始位置では一致末尾まで16KiBに収まらない場合、まず一致行の開始へ寄せる。
一致行だけでも長い場合は、一致開始の最大256byte前へ寄せ、scalar/CRLF境界を保つ。後ろも同じ80行/16KiB上限。
最大1024byteの単一行queryの一致全体を必ず範囲内へ含める。入力range不正は生成時に拒否する。
先頭以外からの表示、先頭/末尾の行の一部省略、表示終了の上限、開始行番号を値として区別する。
範囲計算はmatcherが完成させた行位置から始め、preview生成のために巨大な先頭prefixを同期走査し直さない。

標準EDITへは従来の行ごとのCRLF投影を使う。matchを同じnote_sourceの単位でその表示範囲へ写し、native選択として表示する。
ES_NOHIDESELで入力欄にfocusがある間も一致を見せる。読み取り専用TextPatternの選択、scroll/copy、独立した行番号を維持する。
rowの二行目は最初の一致行番号と有限の一行抜粋。最大192byteのscalar境界で一致周辺を切り、切った側に省略印を付ける。
TAB/制御文字はrow内だけ空白へ投影し、元のpreviewと保存対象は変更しない。UIAは同じ行番号/抜粋/最初の一致の意味を読める。
件数はノート数。未読/読取失敗/対応外tag/0件を区別し、candidate移動のたびに完了件数をlive通知し直さない。

## 確定snapshotと文書切替

本文modeのボタンは「一致行を開く」。HelpTextとキー案内に、編集表示で開くことを明記する。
確定時にapplicationが所有する不変`discovery_target`を生成する。検証済みnote_identity、corpusのnote_text copy、完成したmatch位置を持つ。
これは保存promptを越える短命な確定要求であり、通常の本文cache/第二の可変本文ownerではない。
OOMなら文書を変更しない。snapshotが既存note_text_editabilityでEDITOR_ALTERSならNOTE_UNEDITABLEで確定準備を拒否し、面と元文書を保つ。
その原文の検索/previewは可能なままとし、編集可能な原文であることと混同しない。
無題の保存確認中に結果が失効しても、宛先・照合元・位置をこの値から読み出せる。

文書切替は既存switch_note（保存→identity再解決→folio_state_select_note→reflect_note）だけ。
同じ名前付き文書なら再選択しない。同じEDITはnativeを再投入せず、Undo/Redoを保つ。
VIEWなら既存enter_editを使い、別ノートでも採用後に同じenter_editへ合流する。名前modeはモードを変えない現行動作。

applicationが採用済み本文とtargetのsnapshotをbyte単位で比較し、identityも現在の文書と一致する場合だけnative位置を返す。
同じEDITでは入口で保存したbaselineが比較元。外部ファイルを再読して現在のnativeを上書きしない。
別noteでは通常の選択が再読して採用した本文が比較元。完全一致時だけnote_pane_selectで一致を選択し、未保存印やUndoを増やさない。
不一致ならEDITで開いたまま面を閉じ、「保存内容が検索時から変わったため、一致箇所へは移動していません」を既存pane noticeへ表示する。
古い座標を新本文に当てず、検索の失敗と文書の採用成功を混同しない。
消失/読取不能/保存取消/保存失敗は面を保ち、元の文書と通常の失敗契約を守る。
native投入拒否は既存UNAVAILABLE契約。採用済み本文を古いnativeへ巻き戻さず、一致選択をしない。

確定途中の失敗は採用境界で区別する。同じVIEWでenter_editのRTF準備OOM/編集不可になった場合は、元文書と面を保つ。
別noteをまだ採用していない取消/失敗も同じ。別noteを採用した後のRTF準備OOM/native投入拒否では面を閉じ、採用済み本文と通常のpane失敗を残す。
再読した本文が編集不可へ変わった場合は、既存select_noteのVIEWへの保護を維持し、面を閉じて既存NOTE_UNEDITABLE通知を示す。
この例外ではEDITへ強制せず、snapshotと同一かどうかにかかわらず一致位置を適用しない。
同じVIEWからnative投入を開始した後に拒否された場合も、paneは既存UNAVAILABLEで保護され、面を閉じて復旧操作へ渡す。
採用後の状態を「元文書を維持した失敗」と記録しない。現在のidentity、mode、pane readinessと成否を別々に判定する。

## 配置とUIA

入力行の右に名前/本文の二択を置き、縦の行数を増やさない。560×360 DIPの条件行と結果/原文切替を維持する。
Tabは入力→mode（一停止）→条件→候補→原文→切替→開く→閉じる。mode内の左右は注目移動、Space/Enterで選択する。
mode上のEnterは文書を開かない。query/mode変更時は狭幅の結果表示へ戻す。
二択はRadioButton/SelectionItemとして公開し、選択済みmodeと鍵のあるmodeを区別する。
入力の名前、確定button、状態/preview説明はmodeに追従し、UIAとpaint/hitが同じgeometry/状態を使う。
3言語、明暗/高コントラスト、minimum/広幅を対象確認する。実読み上げと全OS/DPIは別の未確認範囲として記録する。

## 検証と完了

独立byte/scalar oracle、mixed newline/日本語/補助平面/末尾空行、最初の一致/200超/removed、0/1/不均等work、context省略/非常に長い行、
全確保失敗点、corpus一巡再利用/旧世代/更新失効、同一EDIT/VIEW/別note/無題/外部変更/native拒否を確認する。
タグ編集の位置換算は共通化に直接影響するため、その境界試験を再利用または対象再実行する。
初回read、warm語変更、位置付きpreviewを外側のclockで分けて計測し、同期I/Oや実時間上限の未保証を明記する。
全試行/失敗を品質記録へ残し、最終headの必須CI、正典merge、通常exe反映/実data不変、全証跡保全を終えて今回825だけの領域を整理する。

Waivers: none（追加なし、既存WVR-0001継続）。schema/版/ゲート閾値・除外・抑制は変更しない。

18:26の独立読取レビューで編集不可/採用後の編集準備失敗を具体的な例外として追記し、18:26:51の限定再読で指摘を閉じた。
位置の有限work、corpusの一巡開始の所有、座標投影と共通変換について追加の重大な契約穴は見つからなかった。
読取レビューは実装/実機確認ではなく、原本はD:/NeNeFolio/agents/live-grep-design-20261115/contract-review.md。
