# ADR 0070 — Markdownの基本構文と表を一つのRTF変換へ加える

- 状態: 受理（設計。完成候補の結果は品質記録へ）
- 日付: 2026-10-26
- Issue: #396
- 規則: ARC-001/002/003/004/005/009/010/011/012、C-002/003/005/008/011/012/013/014/016/017、QLT-008/009/012/013
- 関連: FR-005/006/011、ADR0002/0006/0023/0054/0067/0069

## 文脈

hideから未対応のMarkdown構文の実装を進める依頼を受けた。main `cfbff7c`の行分類は7種類で、
表・setextは前後の行を解釈できず、入れ子は平坦な本文になる。行末の明示改行は空白へ連結される。
fenceの開閉は記号の長さを保持せず、code spanは1個のbacktickだけで閉じる。
行ごとの分岐を足すだけでは、水平線とリスト、setextと水平線、コード内の構文が干渉する。

表示は既存のcore Markdown→UTF-8 RTF→同じRichEditを維持する。完全なCommonMark実装への置換を目的にせず、
ノートで使う下記の構文を同じ経路へ加え、採用範囲を利用者向け文書へ明記する。

## 決定

1. 次の構文を対象にする。構文の優先順位と境界はCommonMark 0.31.2およびGFMの該当節を根拠とする。

   | 構文 | 解釈と表示 |
   | --- | --- |
   | 表 | headerと同じ列数のdelimiter行で開始。任意の外側pipe、左/中央/右寄せ、escapeしたpipe、セル内の行内装飾を扱う。足りないbodyセルは空、余分なセルはGFMに従う |
   | リスト | bulletと数字+`.`/`)`を認識し、字下げとcontainerの境界を保つ。子リストと継続本文を親の項目へ揃え、番号は各リストの先頭番号から連番にする |
   | 引用 | `>`の階層を字下げへ反映し、内側の基本blockも同じ変換へ渡す |
   | task | リスト先頭の`[ ]` / `[x]` / `[X]`を未完了/完了の印として描く。閲覧中にクリックして書き換える機能は加えない |
   | 取り消し線 | 閉じた`~~...~~`をstrike書式にする。コードとescapeの内側では解釈しない |
   | 水平区切り | 同じ`-`/`*`/`_`を3個以上並べた行（空白を挟む形も含む）を区切りにする。setextが成立する前行との関係を先に判定する |
   | setext見出し | 段落の直後の`===`または`---`をh1/h2にする。表示の寸法はADR0069と共有する |
   | 明示改行 | 本文の行末2空白またはescapeされていないbackslashを改行にする。通常のsoft breakは空白連結を維持する |
   | fence | backtick/tildeを区別し、開いた長さ以上の同じ記号と末尾空白だけで閉じる。短い記号や別記号はコード本文に残す |
   | code span | 同じ長さのbacktick runで閉じる。内部のMarkdownを解釈せず、code spanの空白規則を適用する |

   既存のATX見出し・強調・リンク・コード面の表示も同じinline/block処理を通る。未閉じや不正な構文で
   原文を消したり、後続文書へ書式を漏らしたりしない。非対応構文は文字として保持する。

2. 変換の責務を整理する。公開の`markdown_rtf`は生成物の所有、文書のfont/color表、plain編集の入口を持つ。
   blockの先読みとcontainer状態、inlineの区切り、tableのセル分割とRTF生成を、それぞれ名前のある内部処理へ分ける。
   各関数は元の検証済みUTF-8を長さ付きで借用する。段落内の複数行は元sourceを参照するspan列として一時保持できる。
   全体AST・第2の本文文字列・持続的なsource→表示位置表を加えない。
   RTF文字escapeは一つの実装を共有する。新しい状態/enumは1ファイル1型とし、閉じた選択肢を網羅する。
   長い入力・深い入れ子・未閉じ記号列でも長さを越えて読まず、整数overflowや無制限の再帰を起こさない。
   必要な一時領域の確保失敗は既存の変換失敗へ返し、部分的な生成物を公開しない。
   token列とlist stackは短命な所有者の内部に閉じ、可変なrecordを共有headerへ公開しない。
   tokenはinline所有者の内部recordとして合成し、添字付きの生配列APIや個々のfield setterを外へ出さない。
   別の変換処理から操作するlist stackは不完全型とし、push/pop/連番と解放を同じownerへ集める。
   line/span/viewを借用入力として受ける境界は、長さ・offset・範囲を検証してから参照する。
   不整合な借用入力を確保失敗と同じ結果にせず、必要な型付き拒否を変換結果へ追加できる。
   Markdownとして未対応・未閉じというだけでは入力不整合にせず、元の文字を保持する。

3. 表はRichEditの本物のtableとしてRTFで生成する。headerは本文と同じ大きさの太字、列の寄せは各段落へ設定する。
   罫線には既存muted色、本文/見出しには既存paletteを使い、新しい固定色や設定は増やさない。
   各列は等幅を基本とし、セルの文字はnativeが折り返す。本文clientの有効幅と倍率から列幅を求める。
   native row APIの上限63列を対応範囲とし、それを超える表は全体を元の記法で表示する。
   先頭63列だけを表示するような切捨てはしない。これはGFMの不揃いbodyセルの規則とは別のnative制約である。

4. 表の幅調整は`note_pane`のnative配置処理に限定する。RTF投入完了・VIEWの幅変更・既存倍率変更から同じ処理を呼び、
   nativeのrow/cell書式を読み、`EM_SETTABLEPARMS`で更新する。RTFを再投入せず、本文・選択・focus・Undo・dirtyを保持する。
   同じ幅/倍率での反復、EDIT、投入中、同期再入は避ける。TOMのUndo停止/再開とmodified復元を既存の規律へ揃える。
   成否は確認し、必要な配置が失敗した控えを完成として公開しない。失敗は既存の本文準備/再試行契約へ接続する。
   nativeの構造を正本とし、永続するcell索引や別のtable rendererを持たない。

5. 表内の構造文字は`GT_RAWTEXT`から削除しない。これはnativeの選択位置を維持する取得契約であり、
   同じraw本文の検索結果をそのままnative選択へ渡す。コピー対象はnativeの選択文字列で確認する。
   VIEWの保存・別名保存・EDITへの復帰は既存のapplicationが所有するMarkdown原文を使う。
   表の構造文字をMarkdownファイルや編集用plain RTFへ保存しない。
   ADR0069でコード背景用tableを採らなかったのは、コードに表の意味が無いのに文字構造を変えるためであり、今回と区別する。

6. 水平区切りは既存の背景拡張と空の細いnative段落で描く。文字を繰り返して幅を埋めず、自前paintや画像を加えない。
   同じ段落の前後余白は背景の太い帯になるため、区切り本体は1pt程度の固定高さとし、外の段落余白と区別する。
   通常本文の自然な行高やコード面の寸法はADR0069に保つ。

7. 原文・保存schema・設定・実行時依存は維持する。Markdown生成の呼出形は維持し、
   借用入力の不整合を区別する結果型だけを必要に応じて追加する。既存の変換不成立の経路へ渡し、部分表示を採用しない。
   WebView/外部parser/ネットワーク取得を追加しない。HTML描画、画像埋込み、CommonMark完全互換というSPECの非要件を維持する。
   数式・脚注・独自wiki記法など、今回採らない拡張は対応表で区別する。
   今回のcontainer範囲は、引用内の基本block、引用の階層、子listと通常の継続本文までとする。
   list内にquoteを挟んで同じ親listの本文へ戻る逆順の組合せは、階層/字下げを保証しない。
   原文の保存・編集は維持し、この制限を利用者向け対応表へ明記する。

候補の規約レビューでは、単一の`.c`だけで使うtoken/frameをその`.c`へ直接移す案が、
CNF-002のtop-level型数制限と衝突することを確認した。これを除外で通さず、上記の短命ownerへ責務を寄せる。
inlineのrecord定義は主要なowner structの内部へ合成し、ファイルの主要宣言と公開入口を一つに保つ。
既存の`note_search_query`と同様、借用入力の完全型は検証前の入力であり、検証済みの公開可変状態として扱わない。

## 実装前の成立性

専用の非入力desktopで実MSFTEDITを診断した。raw table構造を保持したまま、実coreの`note_search_next`から
header/日本語+emoji/第2セル/表直後のnative選択へ渡して取得文字が一致した。
600px/100%から320px/150%へのcell幅更新はRTF再投入なしで成立し、TOM Undo停止/再開とmodified復元を併用した
診断は43 checks / 0 failuresだった。選択・raw本文・dirty・Undoを保持した。
空の1pt段落は120dpiで2pxの区切りとして描かれ、rawは前段落・空段落・後段落だけ、後続font/backgroundも復帰した。
同じ区切り段落へ上下6ptを与えると22pxの帯になるため、その配置は採らない。

これらは成立性の診断であり、最終製品の検証とは区別する。source・ログ・画像は
`D:/NeNeFolio/agents/markdown-syntax-20261026/`へ収載する。

## 完成時の検証

対象unitで各構文の正例・反例・複合/入れ子、優先順位、閉じ記号、Unicode/escape、plain編集の保持を確認する。
新しい一時領域は測定ビルドの確保失敗へ接続する。大きい記号列や深い入れ子の対象試験で停止性と境界を確認する。
実製品のnative fixtureで表の検索/選択/コピー対象、原文からの編集復帰、書式、幅/倍率、dirty/Undo/focus、
投入/配置失敗時の扱いと、通常wheelを確認する。light/dark/高コントラストの代表画像を目視する。
成功済みで関係入力が変わらない検証は再利用し、最終PR headの`eng/check.ps1`と必須`check`を統合条件とする。
Waivers: none。

## 参考

- [CommonMark 0.31.2](https://spec.commonmark.org/0.31.2/)
- [GitHub Flavored Markdown](https://github.github.com/gfm/)
- [GETTEXTEX / GT_RAWTEXT](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-gettextex)
- [EM_SETTABLEPARMS](https://learn.microsoft.com/en-us/windows/win32/controls/em-settableparms)
- [ITextDocument::Undo](https://learn.microsoft.com/en-us/windows/win32/api/tom/nf-tom-itextdocument-undo)

## 2026-11-04: パスとタイトルの丸括弧を外側記法へ数えない（#595）

山括弧で囲んだ画像パスの片側丸括弧と引用符title内の丸括弧を一般の括弧対応付けへ数え、
画像参照を見失う反例を確認した。実画像の同梱も成功扱いのまま欠落する。
inline tokenの既存対応付けで、認識済みlabelの閉じ角括弧に続く `(` に属する山括弧内と空白後の引用符titleを
literalな範囲として飛ばす。既存literal token内の引用符を調べ、対応付けは同じtoken所有者の中に保つ。
山括弧はnested `<`/改行で打切り、escapeされた閉じ文字は終端にしない。
成功したliteral範囲を一度だけ飛ばし、未閉じの入力へ全suffix再探索を重ねない。

この処理は構造括弧の候補を選ぶだけで、destinationの妥当性・原文span・escape/entity展開は
既存markdown_destinationへ、ローカルpercent展開は既存image_sourceへ渡す。
画像とWebリンクで共用し、quotedtitleの括弧で隣の画像/リンクを吸収しない。
外側codeの優先順位、未閉じの原文保持、通常の入れ子丸括弧とescape/percent表記を維持する。

## 2026-11-04: destinationを外側codeの対応付けから分ける（#597）

パス/title中のbacktickが後続本文のbacktickと先に対応付けられ、画像参照または後続codeの書式を失う反例を確認した。
CommonMark 0.31.2のdestination/titleとlabelの境界、および固定した公式参照実装の同一入力で優先順位を照合した。

inlineのcode/autolinkとlabel/destinationの範囲を、同じ左からの走査で確定する。
label内から始まるcodeは従来どおり優先し、成立したdestinationの内側は外側codeの候補へ渡さない。
失敗したdestination候補では通常のinline走査を続け、再帰・全体の再解析・固定の新しい深さ上限を追加しない。
既存tokenの括弧対応と既存scratchへ計算する裸pathのsuffix境界を利用し、深い未閉じ候補から毎回末尾を走査しない。
先頭の空白と成立して消費する範囲はtokenを前へ進め、parenthesized titleの先読みだけ有界な二分探索を使う。
文字列のコピーや新たな所有者/公開APIを増やさない。
この前計算はinlineの構造候補だけであり、値の妥当性・元bytesspan・escape/entity展開は既存markdown_destination、
local percent展開はimage_sourceが引き続き担当する。元の全体code先行処理を残した別経路は作らない。

既存の画像altの文字保持、autolink/ラベル内code/入れ子画像、未閉じと後続書式、長い通常文と深い失敗入力を対象に確認する。
外部参照parserは検証用D側にだけ保管し、製品依存には加えない。新しいschema/waiver/gate変更はない。

この走査順に伴い、#422の親label交差fixture一例を含む境界の表示契約を変更する。
旧期待は当時の画像literal保持の設計に合わせたouter-link優先であり、後から成立する外側label候補の終端へ
画像の裸path内の角括弧を流用していた。今回は左から成立した画像destinationをliteralとして消費するため、
その角括弧は外側labelを閉じず、外側linkが成立しない場合は未閉じの文字と画像として表示する。
固定CommonMark 0.31.2の独立結果と、旧/候補の追加14入力を照合して採用した。
単なる未決の外側括弧で内側画像を拒否する案は正常画像も失い、候補ごとの再走査も深い入力で二乗化し得るため採らない。
旧fixtureは削除せず完全RTF一致の新しい期待へ変更し、外側link不成立/成立・path内角括弧・codeの対照も固定する。
成立した通常linkのlabel内に完全に収まる画像だけを含める制約は継続するが、この交差入力の表示は互換不変とは報告しない。
裸pathの開き角括弧とparenthesized titleの閉じ角括弧で壊れていた外側linkの書式も回復する。
画像参照が新たに成立する入力は、既存の遅延読込とZIP同梱の対象にもなるため、表示だけの変更とは扱わない。
既存のbacktick付きリンク先のfixtureでも、最初の閉じ丸括弧の後の本文をdestinationへ吸収する期待を訂正する。
同じ固定参照で本文が残ることを確認し、RTF完全一致を保ったまま後続文字の保持を要求する。
新しい公開API/owner/持続状態/schema/依存/gateを追加しない。
根拠は既存採用範囲のCommonMark0.31.2 §6.3 destination（例499）とtitleの境界。

初回の引用符全体を字句境界へ加える案は、Debug診断で引用符の多い通常文409600bytesが
旧50.205ms→178.793ms（4定常sample中央値、起動診断あり）となったため採用しない。
元の字句分割を維持し、対応付け中のdestination内にある既存literal tokenだけを走査して
titleの開始引用符を見つける。通常本文の引用符へtoken増加を波及させない。

## 2026-11-05 の数値出力（#667）

同じrtf_text_numberのstackへ十進数字を正順に作り、一度だけ既存text_bufferへ追加する。API/所有/RTF bytes/sticky OOMは不変。
0/最大値/桁境界/成長境界472組と変換の失敗注入を既存markdown_testsで確認する。性能は数値batch改善の範囲に限定する。[品質記録](../quality/2026-11-05-rtf-number.md)。

## 2026-11-05 の通常文字出力（#675）

code_syntax.emitのTEXT tokenだけ、空styleの追加を省く。原文escapingと着色tokenの囲み、字句/状態/所有、公開APIは不変。
--code-syntaxで既存着色試験を単独選択し、既存Markdownと確保失敗境界も確認する。
42条件の全出力と比較/確認計測の限界は[品質記録](../quality/2026-11-05-syntax-text.md)へ。一般UI高速化は主張しない。
