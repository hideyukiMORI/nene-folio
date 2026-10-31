# ADR 0078: 軽い行内記法を既存RTFへ写し、水平線の余白を線自身に持たせる

Status: accepted / 2026-10-31 / Issue #480

## 背景

hideは水平線の上下の余白を少し増やすことと、未対応Markdownのうち軽いものの実装を求めた。
言語選択/構文色分けはStore公開後と指定し、Mermaidの実装負担を質問した。Mermaidは描画基盤が必要なため今回は見送る。現実装は区切り前後の余白を
前後の段落に分けており、空行で先にflushした場合や見出しで差が生じる。

## 決定

- ADR0070/0071のMarkdown→UTF-8 RTF→RichEditを維持する。HTML/WebView/JavaScriptや外部描画依存を加えない。
- `<URI>`と`<email>`を既存リンクと同じ色・下線で表示する。URI schemeはASCII先頭英字の2〜32文字、
  続きは英数字/`+.-`。空白/control/山括弧を含まない本文を要求する。emailはCommonMark 0.31.2の
  ASCII local-partと各1〜63文字のdomain label規則を使う。裸URLや外部を開く操作は追加しない。
- コードと自動リンクは原文を左から優先し、先に開いた有効範囲内の装飾を解釈しない。
  bracket pairingと強調もその範囲を飛ばす。リンク内のbackslash/文字参照は原文表示を維持する。
- 通常の行内文字で、10進1〜7桁/16進1〜6桁の数値文字参照と、`amp`/`lt`/`gt`/`quot`/`apos`/`nbsp`の
  6つのnamed参照を展開する。末尾semicolon必須。全HTML5 named集合への対応とはしない。
  NUL・surrogate・U+10FFFF超はU+FFFD。展開後の文字をMarkdownへ再解釈せず、既存UTF-8符号化とRTF escapeへ渡す。
  コード・画像原文・自動リンク内では展開しない。元の編集RTF/保存本文は一切変えない。
- 水平線は固定1ptの線の前後に、背景なしの固定12ptの空段落を置く。ADR0070のとおり線の段落へ
  前後余白を直接与えると背景が太い帯になるため、その方法は採らない。前後段落に対する特別な余白状態を削除し、
  通常の段落/見出しの余白は保つ。線と空白の生成は同じruleの経路に閉じる。
  線の色・親のindent・構文判定は維持する。閲覧のrawには空段落が2つ増えるが、保存/編集原文へ混ぜない。
- 新しい純粋な字句判定は`markdown_autolink`と`markdown_character_reference`に閉じる。
  借用viewは既存render入口で検証し、追加の所有文字列・状態・実行時確保を作らない。

## 検証と制限

単体で構文境界/長い入力/原文保持/確保失敗を確認する。実RichEditへ同じRTFを渡した画像とnative書式を確認する。
専用fixture/背景撮影を用い、hideの実ノート/clipboard/focusを使わない。物理IME/実reader/DWM呈示とは分ける。
保存schema、製品版、依存、ゲートは不変。Waivers none。

仕様参考: [CommonMark 0.31.2 自動リンク](https://spec.commonmark.org/0.31.2/#autolinks)、
[文字参照](https://spec.commonmark.org/0.31.2/#entity-and-numeric-character-references)。
本製品は部分集合であり、完全なCommonMark/GFM互換を保証しない。

最初の候補で線へ`sb240/sa240`を直接付けたところ、native値検査は成功したが画像では太い帯になった。
その候補は不採用とし、背景なしの空段落へ修正した。単体とnative属性だけで視覚品質を受理しない。
