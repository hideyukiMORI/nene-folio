# Markdown対応の整理 — 2026-10-31

Issue #480。hideの「未対応を洗い出し、軽いものは実装する。言語選択等はStore公開後」という依頼に基づく。
現在の`markdown_blocks`/`markdown_containers`/`markdown_inline_tokens`/`markdown_table`と
利用者向け[Markdownの書き方](../MARKDOWN.md)を照合した。完全なCommonMark/GFM互換の監査ではない。

## 対応しているもの

ATX/setext見出し、段落と明示改行、太字/斜体/取り消し線、可変長backtickのcode span、
backtick/tildeのfenced code、番号付き/番号なしリスト・入れ子・引用・task表示、水平区切り、
inline linkの装飾、GFM型の表と列寄せ。表は最大63列などのnative制限を持つ。

#480では水平区切りの上下に余白を追加し、`<URI>`/`<email>`の自動リンク表示、数値文字参照、
`amp`/`lt`/`gt`/`quot`/`apos`/`nbsp`の6種のnamed参照を加えた。
いずれも既存のC→UTF-8 RTF→RichEditで描き、実行時依存やネットワークアクセスを増やさない。

## 未対応・今後の候補

| 項目 | 現在の表示・操作 | 実装負担の判断と扱い |
| --- | --- | --- |
| コードの言語選択・構文色分け | fenceに言語名を書けるが色分けしない | 中〜大。言語ごとの解析、配色、長文性能、切替UIが必要。hide指定でStore公開後 |
| コードの言語名表示・コピー操作 | 専用のラベル/コピーbuttonはない | 表示だけなら小。選択UI/コピー操作と区別し、今回は追加しない |
| Mermaid | fenced codeの本文として表示 | 大。通常はJavaScriptで解析・描画するため、現在の素C/Win32へ描画基盤を組み込む判断が必要。今回は見送り、公開後の候補 |
| 字下げコードblock | 4空白を独立したコード開始規則として扱わない | 中。リスト本文/継続行との優先順位に影響。次の構文候補 |
| 参照形式のlink/imageと定義 | 定義を集めて解決しない | 中。文書全体の定義、重複、正規化、forward参照が必要 |
| 裸URL・裸メールの自動リンク | 山括弧のないものは普通の文字 | 中。GFMの末尾句読点/括弧/境界規則が別に必要 |
| 全HTML5 named文字参照 | 基本6種以外は原文 | 小〜中。対応表の固定・出典・更新と全件検証が必要。追加依存なしでも実装可能 |
| link title | tooltipなし | 中。構文の解釈と表示契約が必要。HTTP/HTTPSの外部起動は#482/ADR0079で追加 |
| 画像の埋込み | `![...](...)`全体を文字表示 | 中〜大。ファイル/URL解決、画像decoder、縮尺、寿命、エラー表示が必要 |
| HTML描画 | HTMLとして実行/描画しない | 大。別rendererや扱うHTML集合の設計が必要。現在の非要件 |
| 数式（TeX/LaTeX） | 数式rendererなし | 大。構文/字体/配置の基盤が必要 |
| 脚注・定義リスト | 専用構文の解決/描画なし | 中。参照先・番号・block構造を持つ拡張 |
| GitHub alerts、wiki link、目次、front matter、mark/上付き/下付き等 | 専用の拡張として扱わない | 小〜大。互換先と意味を先に選ぶ。今回は一括追加しない |
| taskのクリック切替 | checkbox風の閲覧表示のみ | 中。原文位置との対応、保存/Undo、native操作が必要 |

負担は本製品の構造からの判断であり、日数見積りではない。色分けやMermaidが単に「表示文字を増やす」変更と
同じ重さとは扱わない。なお、一般のcode fenceへ`mermaid`と書くこと自体は原文として保存できる。

参考: [CommonMark 0.31.2](https://spec.commonmark.org/0.31.2/)、[GFM仕様](https://github.github.com/gfm/)、
[Mermaid公式の概要](https://mermaid.js.org/intro/)。仕様の対応範囲と製品固有の表示方針を区別した。

## 文字ウェイト混在の保留 — #479

hideの職場での報告は、モード・貼付元が未確定（両方/覚えていない）。短い切り分けで、隔離した実RichEditへ
書式付きRTFとUnicodeを置き`WM_PASTE`すると、本文へ別字体/18pt/太字/赤字が混入する経路を再現した。
本文文字と実Undo/Redoは保持した。`SES_EMULATESYSEDIT`だけでは混入を防がなかった。
この経路と職場の現象が同じ原因かは確定していない。hideの「ある程度探ったら閉じて別を進める」で修正保留。
Issueはopenで追跡し、production変更0。通常data/利用者clipboard/focusを操作していない。

原本は`D:/NeNeFolio/agents/editor-paste-20261031/`と`D:/NeNeFolio/outputs/479-editor-paste/`。
正典Debug/tidy成功。最終診断baseline-03の50checks/12failは、2つのWM_PASTE書式不一致と、
WM_CHAR(22)単送では貼り付けが起きなかった10件に分かれる。後者を実Ctrl+Vの再現とは扱わない。
fixtureのSDK定数未定義とstation準備失敗も原本へ残す。未完worktree/outputは診断原本と未統合記録のため保持する。

Waivers none。#480の実装/検証結果は[品質記録](2026-10-31-markdown-light.md)を参照。
