# ADR 0071 — 引用とリストの親子関係を一つの順序付きownerで解釈する

- 状態: 受理（設計。実装と完成候補の結果は品質記録へ）
- 日付: 2026-10-29
- Issue: #404
- 規則: ARC-001/003/004/008/010/012、C-002/003/005/008/011/012/014/016/017、QLT-008/009/012/013
- 関連: ADR0054/0069/0070、FR-005/006/011

## 文脈

main `5c1240e`はquote深度とlist stackを独立して持ち、quote深度の変化でlistを全破棄する。
リスト内の引用から親本文へ戻ると番号と字下げを失い、L→Q→LとQ→L→Qも区別できない。
表とfenceの所属判定もquote数と字下げだけを参照し、別itemや外側の行を取り込む余地がある。
ADR0070の逆順containerの未保証範囲を解消するため、単にstack破棄を取り除くのではなく所有を置換する。

## 決定

1. coreの短命な不完全型`markdown_containers`を、quote/listの順序、現在item、連番、未表示markerの唯一のownerとする。
   rootからleafへ並ぶ内部record列にQUOTEとLIST_ITEMを記録し、原文prefixをその順に照合する。
   表示用のquote数・list数・字下げはpathから導く。旧`markdown_list`と独立した可変quote状態は同じ変更で削除する。
   blocksはleaf・借用span・RTF出力を持ち、containersはRTFを描かない。公開frame配列・永続AST・第2の本文を作らない。

2. 物理行は既存のCR/LF/CRLF読取を共有する。quoteを先に全部剥がす方式をやめ、任意prefix位置から同じ字句操作で進む。
   行queryは元bytesを借用し、byte offsetと構文prefixのvisual columnを区別する。
   tabを途中まで消費した場合は、tabの次のbyte位置と残余0〜3列のpaddingで表す。本文全体のspace展開はしない。
   list継続・quote・fence deindentはこの操作を共有し、codeは残余paddingを出した後に原文の残りを出す。
   通常段落ではpaddingを余分な本文へ足さず、63列超表のliteral表示では原文prefixとの二重出力をしない。
   offset、padding、columnと長さの整合を境界で検証し、加算・乗算・容量成長をcheckedにする。

3. 通常受理と厳密照合を分ける。通常受理はraw行・次の物理行・非空paragraphの有無を受け、既存pathとの一致とblock開始を判断する。
   paragraphが開いていてpath/item変更が必要なら、path・番号・受理済み行を変更せず`FLUSH_REQUIRED`を返す。
   blocksは旧paragraphとそのmarkerを出し、同じraw行をparagraphなしで一度だけ再受理する。
   reserve・pop・push・番号更新はこの判定の後に行い、未出力paragraphが参照するownerを先に変更しない。
   同scopeの通常継続とlazy継続は、既存pathを維持して受理できる。再受理で無限にflush要求を返す設計にしない。
   全公開関数は4引数以内とし、結果・paragraph状態・eventは役割を持つ閉じたenumで表す。

4. lazyは既に開いた非空paragraphの継続だけに適用する。空行後の新段落、heading、rule、fence、tableには広げない。
   新blockの字句判定は既存line/tableの規則を共有し、別の表delimiter parserを作らない。
   paragraphを割る新ordered listの1開始規則と、空のitemはparagraphを割らない規則を守る。
   既存itemの継続columnsに達しない同親scopeの有効marker（0〜3列）はsiblingとして照合し、
   原文のmarker開始列の完全一致をlist同一性にしない。継続columnsを満たしてからのmarkerは子listになる。
   prefixを欠いたsetext underlineを内側の見出しへ昇格させない。
   blankもpath照合を通し、quote記号なしblankはquoteとその子を閉じ、継続可能な親listを残す。
   明示quote blankはquoteを保ちながらparagraphを閉じる。完全CommonMark互換の宣言はしない。
   空itemの次のblankはitem本文を終了するが、同じlistの連番は同じrecord内に保持する。
   空/内容あり/本文終了を閉じたphaseで区別し、本文終了後は同scopeの次siblingだけを受け付ける。
   字下げ本文の継続は認めず、外側prefixが途切れたときはその配下の終了済みrecordも閉じる。
   `7.\n\n1. next`は8へ続き、`7.\n\n   text\n1. next`は外側本文と新しい1になる。

   基本の受け入れ例は`7. parent\n   > quote\n\n   continued\n1. next`であり、continuedは親本文、nextは8になる。
   空行を除いた例ではcontinuedも同じquote paragraphへ入る。番号は同じ親listから継続する。
   Q→L→Q、L→L→Q、quote内子listから親item復帰、別root quote、別itemを区別する。

5. 厳密照合はconst ownerへの純粋な問い合わせとする。現在pathの全prefixと同一itemを照合し、成功時だけ残余queryを返す。
   不一致と不正queryは別の型付き結果とし、割当・frame変更・番号更新をしない。
   fenceの本文と閉じ記号、tableのdelimiter先読みとbodyはこの一つの照合を通る。
   fence中の記号は新containerとして開かずcodeに残す。scope離脱時はcodeを閉じ、同じraw行を通常受理へ戻す。
   tableも別scope・兄弟item・新blockで閉じ、delimiterを読むだけでは通常受理を呼ばない。
   63列超のliteral tableも同じ所属で閉じ、各原文行の記号と全列を保持する。
   literal headerの原文に含まれるitem markerはそのまま表示印として扱い、その行のpendingを消費して合成印を足さない。
   原文の一部だけを変換して印を除くことはしない。したがってliteral内の番号は原文どおりとなり、
   ownerの連番は進めたうえで、次の通常itemから自動連番の表示へ戻る。

6. markerはitemごとに一度だけ表示する。各LIST_ITEMのpendingをownerが持ち、外側から順に照会・消費する。
   marker用の別stackや段落ごとの可変marker配列を作らず、連番は同じownerだけが進める。
   最後のpending markerを最初のparagraph・heading・quote本文のhanging領域へ付ける。
   複数pendingがある場合は外側の印を順に専用段落へ出し、最後の一つだけleafに付ける。
   native tableとfenced codeは全pendingを開始前の専用段落へ出す。空のitem/空quoteも受理行で専用段落を出す。
   空fenceも開始時に印が出るため、scope離脱・EOFで印を失わず、印だけのための空code行を足さない。
   次の通常受理へparagraphなしで渡す時点で、前のleafに属する未処理markerを残さない。

7. markerの字面は本文font・11pt・本文色・通常weight・背景なしに固定し、heading/quote/codeの装飾を継承しない。
   marker位置はそのitem本文位置の220twip左、leaf本文は現在のcontainer位置とする。
   最初のtabの着地点をleaf本文位置へ明示する。quoteが加わると負のfirst indentが深くなるが、marker列は親itemに残す。
   codeの本文は従来の180twip内側余白と一続きの背景面を保ち、marker段落はcode groupの外に置く。
   list内の通常本文/引用は段落後40twip、item先頭headingの段落前は0、専用marker段落は段落後0とする。
   heading本体のサイズ・後余白、後続headingの前余白、codeの最初/最後120twipはADR0069を保つ。
   空の水平線leafが先頭の場合もmarkerを専用段落で保持し、細い区切りへ文字を混ぜない。

8. native表の字下げと残り幅の再配置は別の焦点Issueへ分ける。#404は所属と終了境界を修正し、
   表の左端がcontainer本文へ揃う保証は追加しない。この未解消範囲を利用者向け対応表に残す。
   原文の保存、plain編集、schema、設定、依存、UI入力/検索経路と品質ゲートは変更しない。

9. ownerの内部recordは倍増容量の配列へ閉じ、深さ依存の再帰や固定深さでの黙ったflattenを使わない。
   失敗時はownerが解放可能であればよく、継続利用しない失敗のための第2stackを作らない。
   OOMと借用入力不整合を区別し、既存RTF生成入口の部分生成物破棄・公開out不変へ接続する。
   512段と混合pathの停止性は確認するが、極端な深さの全native可視性は保証しない。

## 実装前の成立性と不採用案

NeNeFolioリナの読み取り助言と、専用の非入力desktop・120dpiの手書きRTF診断を判断材料にした。
単一markerはquote/heading/code本文への明示tabで配置でき、headingのmarkerは11pt、code本体は10ptと背景を保った。
ただしcodeの同一行案は先頭marker/tab領域だけ背景が欠け、続行は全幅背景となることを画像でも確認した。
ADR0069の一続きの面を保つため、codeは検証済みの専用marker段落方式を採用する。
同一段落に二つの印を置く案は、本文880twipに対して実位置が1440twip相当へ進んだため採らない。
これはnative成立性の診断であり、製品コードの検証ではない。生ログ・source・RTF・画像・失敗は
`D:/NeNeFolio/agents/404-markdown-containers/`へ保全する。

## 完成時の検証

対象unitで親復帰/連番、同scopeとlazy、prefix不足のblock、blank、複数/空marker、heading/code/task、
table/fenceの離脱と純粋な先読み、CR/LF/CRLF、tab途中消費、Unicode、63列超literalの保持を確認する。
frame成長・深い混合path・全確保点の失敗とout不変を測定ビルドで確認する。
製品RTFをnativeへ投入し、代表的な印と本文の文字位置・段落属性・折返し・背景復帰と原文plain復帰を確認する。
成功済みで関係入力不変の検証は再利用し、最終PR headの`eng/check.ps1`必須CIを統合条件とする。Waivers: none。

## 参考

- [CommonMark 0.31.2: block quotes](https://spec.commonmark.org/0.31.2/#block-quotes)
- [CommonMark 0.31.2: list items](https://spec.commonmark.org/0.31.2/#list-items)
- [GFM: tables](https://github.github.com/gfm/#tables-extension-)
- [ADR0070](0070-markdown-syntax-and-native-tables.md)
