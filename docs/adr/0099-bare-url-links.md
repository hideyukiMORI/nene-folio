# ADR 0099: 囲まないURLを、決まった後のただの文字からGFMの規則でリンクにする

Status: accepted / 2026-11-17 / Issue #850

[ADR0079](0079-view-web-link-activation.md)の決定のうち「裸URL認識は追加しない」だけを置き換える。
起動の経路（検証済み`web_uri`・RTF HYPERLINK field・`note_pane_links`・`web_link_port`）とRichEditの自動URL検出を使わない決定は維持する。

## 背景

閲覧表示では`<https://example.com/>`の形だけがリンクで、貼り付けたままのURLは普通の文字だった。
ノートへURLを貼る書き方として最も多いのは囲まない形で、GFM（GitHub）を含む主要なMarkdown環境はこれをリンクにする。
[Markdown対応の整理](../quality/2026-10-31-markdown-scope.md)に中規模の候補として残っていた。
規則の照合先は[GFM 0.29の拡張autolink](https://github.github.com/gfm/#autolinks-extension-)と、その実装
[cmark-gfm `extensions/autolink.c`](https://github.com/github/cmark-gfm/blob/master/extensions/autolink.c)
（`check_domain`・`autolink_delim`・`www_match`・`url_match`）。

## 決定

- 認識は core の新しい`markdown_bare_link`が一か所で行う（借りたviewと位置と上限を受け、長さを返すだけ）。
  `http://`・`https://`（ASCIIの大小を区別しない）と`www.`（区別する）で始まるもの。ftpと裸のメールアドレスは扱わない。
- 規則はcmark-gfmに合わせる: ドメインは英数字・`-`・`_`・`.`の連なりで、最後の2区画に`_`を含まない。
  `www.`はドメインに`.`を要する。scheme付きは要しない（`http://localhost`）。
  末尾の`?!.,:*_~'"`、閉じが多いときの`)`、`&name;`（`;`だけなら`;`）はURLに含めない。
  schemeの直前がASCIIの英字なら認識しない。
- GFMからの意図的な差（日本語の文のため）:
  - `www.`の直前に、GFMの行頭・空白・`*` `_` `~` `(`に加えて、ASCII以外の文字を認める。
  - URLはASCIIの範囲で終わる（空白・制御文字・`<`に加えて、ASCII以外の文字で止まる）。
    `https://example.comを参照`の「を参照」を含めない。ASCII以外を含むURLは従来どおり`<...>`で囲む。
  - ドメインは253 bytesまで（失敗する試行の走査も有限にする）。cmark-gfmの「`.`が11個以上なら`_`を許す」例外は採らない。
- 認識する場所は`markdown_inline_tokens`の文字の出力で、強調・コード・リンク・画像・山括弧autolink・escapeが決まった後の
  「ただの文字」の連なりだけ。リンクの表示文字の中（`in_link`）、コード、画像の原文、escapeの内側では認識しない。
  URLは、対になった強調記号・コード・escape・画像の手前で終わる。
  cmark-gfmは字句の走査中に認識するので、URLの途中に対になる強調記号がある稀な場合（`https://a.b/*x*`）だけ結果が違う。
  リンクの行先`[x](https://...)`をURLとして二重に扱わないために、この順序を選んだ。
- 行先は表示した原文のbytesのまま（`www.`は`http://`を前に付ける・GFM）。`web_uri_create`が拒むもの（上限超過等）は普通の文字のまま
  表示し、装飾しない（押しても開かないものをリンクに見せない）。表示文字の`&amp;`等は展開しない。
- 出力は既存の山括弧autolinkと同じ形（受理したfield・`\cf4\ul`・直後のhiddenの空白）。nativeの対応・起動・失敗表示は既存経路をそのまま使う。
  表示名とURLが同じものはRichEditがnative autoURLへ変えるので、Windows標準のリンク色になる（ADR0079と同じ）。

## 却下した案

- RichEditの自動URL検出（`EM_AUTOURLDETECT`）: Markdownの解釈が二か所になり、コード・リンクの内側も検出し、規則を製品で持てない（ARC-001）。ADR0079の却下理由のまま。
- 字句の段階（`autolink_after`）でtokenを先取りする: `[x](https://...)`の行先や画像の原文を、リンクの対応付けより先にURLとして取ってしまう。
- GFMどおり、ASCII以外の文字もURLに含める: 日本語の文で、続く語までリンクになる。IRIは`<...>`で書ける。
- 裸のメールアドレス: 起動しない（ADR0079でmailtoは対象外）ので、装飾だけのリンクを増やさない。

## 検証

認識する/しない例（GFM仕様の例・日本語の前後・句読点・括弧・`&name;`・`_`・上限・強調/コード/リンク/画像/escapeとの組合せ）とRTFのfield・確保失敗を単体で、
閲覧でのクリック/Ctrl+Enterが偽portへ一度だけ渡ること、編集表示と保存本文が変わらないことをnativeで確かめる。最終headの必須CIで正典`eng/check.ps1`を要求する。Waivers none。
