# ADR 0069 — 閲覧の密度とコード面を既存のRTFへ揃える

- 状態: 受理（設計。実装・対象検証・最終CIは品質記録へ）
- 日付: 2026-10-26
- Issue: #394（hideの実機確認後の表示調整）
- 規則: ARC-001/003/004/005/009/011/012、C-002/003/008/012/013/014/016/017、QLT-008/009/012/013
- 関連: ADR0002/0005/0006/0049/0054/0057/0067

## 文脈

hideは#392のスクロールとCtrl倍率を実機で確認し、続いて閲覧時の行間、`####`の大きさ、
fenced codeの見た目の調整を指定した。基点main `5e7205b`の本文は11pt・1.25行送り・段落後8pt、
h4は12pt太字、コードは行ごとの文字highlightと段落後2ptで、空行や行長ごとに背景が途切れる。

表示はcoreの`markdown_rtf`が生成したUTF-8 RTFを、同じ`note_pane`へ投入する。
文字・選択位置・検索・コピーを変えるための書式変更ではない。新しいrendererや文字位置の変換表を作らない。
ClaudeCodeのdesign-critique相談はOAuth期限切れで実行できなかったため、設計サナが同じ密度・階層・一貫性・可読性の観点で判断した。

## 決定

1. 閲覧RTFの寸法を次に揃える。値は`markdown_rtf.c`が一度だけ所有し、編集RTFの段落設定は変更しない。

   | 要素 | 書体・大きさ | 行送り | 前 / 後の余白 |
   | --- | --- | --- | --- |
   | 本文p | 既存の言語別Noto Sans、11pt | 自然な1行（`sl240/slmult1`） | 0 / 4pt |
   | 引用 | 本文と同じ、既存の引用色・字下げ | 同上 | 0 / 4pt |
   | 箇条書き・番号付きリスト | 本文と同じ、既存のmarker・字下げ | 同上 | 0 / 2pt |
   | h1 / h2 / h3 | 22 / 17 / 14pt、太字、既存heading色 | 自然な行高 | 10 / 4pt |
   | h4 / h5 / h6 | 本文と同じ11pt、通常weight、既存heading色 | 自然な行高 | 8 / 3pt |
   | fenced code | Consolas 10pt、既存code文字/背景色 | 自然な1行 | 最初だけ6pt / 最後だけ6pt |

   pを詰める際も固定行高を使わず、CJK・絵文字・fallback字形に必要な高さをnativeへ任せる。
   h4を本文サイズ・色中心へ変える指定に合わせ、h5/h6だけが太字で強く残らないよう揃える。
   明示的なインライン強調は既存Markdownの意味として維持する。
2. コード面はRichEditの`SES_EXTENDBACKCOLOR`を、唯一の`note_pane_create`で文書投入前に設定して描かせる。
   `EM_SETEDITSTYLE`のmaskは当該bitだけとし、ほかのnative設定を変更しない。
   コード文字の左右を各9pt字下げし、面内の余白にする。面は本文のclient幅へnativeが追随する。
   RTFのコード全体を一つのgroupで囲み、各原文行は従来どおり一つの段落とする。
   highlightは段落記号まで保持し、最後の段落を書いた後でgroupを閉じる。
   これで空行・行間・左右余白を含む一つの背景面を作り、次の通常段落と編集への再投入に背景を漏らさない。
3. 最初と最後だけ上下に6ptを取り、中間のコード段落の前後は0にする。
   末尾の段落を確定する時点まで段落記号を保留し、次行・閉じfence・文書末尾で確定する。
   開いている段落の状態は既存のrenderer内だけで持ち、本文の第2の写し・AST・独立したblock索引を加えない。
   空のfenceだけでは空白文字や追加段落を生成しない。未閉じfenceも文書末尾でRTFのgroupを閉じる。
4. 色は実効`theme_palette.rtf`の既存役割から受ける。新しい固定色・テーマ項目・保存設定は加えない。
   高コントラストではADR0057のwindow/windowText対を維持し、背景色の差を強制しない。
   コードは等幅・字下げ・上下余白、小見出しは段落の区切りでも本文から区別できるようにする。
5. スクロール・倍率・本文投入・本文取得・保存は既存の経路を維持する。
   面幅はnativeへ任せ、resizeやzoomのたびにRTFを再生成しない。コピー用の余分な空白やtab、table markerを挿入しない。
   丸角、影、言語ラベル、コピーボタン、syntax highlightingはこの表示調整へ追加しない。

## 成立性の根拠と採らない案

専用非入力desktopのnative診断で、通常段落＋`SES_EXTENDBACKCOLOR`＋段落記号までのhighlightは、
コード内空行を含めて背景の隙間0、300/500px幅と150%倍率への追随を確認した。
raw/default/selectionの取得文字列はCRを含めて一致し、表の制御文字を追加しなかった。
RTF groupを閉じてから外側書式へ戻す案は、plain編集RTF再投入後に背景画素0となった。
入力・source・ログ・画像は`D:/NeNeFolio/agents/markdown-style-20261026/`へ収載する。
完成候補の実製品での確認はこの診断と区別する。

- 段落shading/borderだけを足す: MSFTEDITは診断で描画しなかった。ADR0005の制約を架空の対応へ置き換えない。
- 1cell table: 面は描けるが、raw本文へU+FFF9/U+FFFB/U+0007と余分なCRが入り、固定幅は狭い窓とzoomで切れる。
- 単一段落の`line`改行: raw本文のCRがU+000Bへ変わり、背景も文字幅にとどまる。
- 空白/tabで面を埋める、別の控えや自前paintで覆う: コピー文字や描画・位置の正本を増やす。
- コードのhighlightを段落記号の前で戻す: 空行部分の背景が途切れる。

API根拠: [EM_SETEDITSTYLE](https://learn.microsoft.com/en-us/windows/win32/controls/em-seteditstyle)、
[EM_GETEDITSTYLE](https://learn.microsoft.com/en-us/windows/win32/controls/em-geteditstyle)、
[PARAFORMAT2](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-paraformat2_1)。

## 完成時の検証

対象markdown unitで段落・見出し・コードのRTF構造、空/未閉じ/連続block、Unicode/escape、編集RTF不変を確認する。
実製品のnative fixtureでは、行送り/段落余白とh4の実fontを読み、コード背景の連続性、plain再投入、
表示文字・検索/選択位置、幅/倍率/通常ホイール、light/dark/高コントラストの代表条件を直交配置で確認する。
画像で本文の密度と見出し階層、コード面の余白・文字切れを確認する。
対象build/tidy/format後、最終PR headの`eng/check.ps1`と必須`check`を統合条件とする。
関係入力が変わらない成功済み結果は再利用し、物理入力や全配色/全OSの保証へ広げない。

公開API・保存schema・依存・ゲート変更なし。Waivers: none。

## 後続の構文拡張

[ADR0070](0070-markdown-syntax-and-native-tables.md)でblock/inline/tableの内部処理を分割する。
本ADRの寸法とコード面はblock処理へ移し、値の所有経路は一つに保つ。
Markdownの表は実際の表構造を持たせるが、コード面には引き続きtableを使わない。
