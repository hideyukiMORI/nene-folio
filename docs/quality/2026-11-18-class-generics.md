# 品質記録 — 2026-11-18 Mermaidのクラス図のジェネリクス（#938）

ROADMAP「MarkdownとMermaidの整理」の次の候補（11月17日の引き継ぎ）から、hideの判断が要らないクラス図のジェネリクスを実装した。
Mermaidの公式の例（`class Square~Shape~{ List~int~ position }`）が、これまでは図全体が「未対応」の案内になっていた。

## 変更

- `src/core/mermaid_class.c`: クラス名の直後の`~…~`を`<…>`として表示する（`read_class`・`show_generic`）。メンバーの行も同じ規則（`append_generic`）。
  `Box`と`Box~T~`は同じクラス（`find_class`が「名前」と「名前<…>」の両方を探す）。
- 規則は名前の文字に挟まれた`~`が開き、開いている間の別の`~`が閉じる。入れ子`List~List~int~~`は`List<List<int>>`、行頭の`~reset()`（パッケージの可視性）はそのまま。
- 閉じない`~`・カンマを含む`Map~K,V~`・中身の無い`Box~~`は図全体を未対応にする（部分的な図を作らない）。
- 文書: ADR0107の補足、docs/MARKDOWN.md。

## 試験と結果

作業木 E:/NeNeFolio/worktrees/938-class-generics、構築 E:/NeNeFolio/outputs/938-class-generics/build。証跡の写しは D:/NeNeFolio/agents/938-class-generics/。

| 回 | 内容 | 結果 |
| --- | --- | --- |
| t1 | `folio_unit`（単体: 宣言・段のメンバー・`Name : member`・関係の両側・入れ子・非ASCII・可視性の`~`・不正5通り）と`diagram_render` | 2件成功 |
| t2 | `diagram_render_tests`の画像（見本のクラス図に`class 箱~T~ { -List~T~ 中身 }`と`犬 --> 箱`を足した） | 563 checks成功。名前の帯が「箱<T>」、メンバーが「-List<T> 中身」（class-light.png・class-dark.png） |
| t3 | 単体に数字・`_`に挟まれる形（`Vec2~f~`・`Pair_~K2~`）を足す途中 | Bashのheredocで日本語が化けて置き換えが失敗（ファイルは不変）。Editで足し直した。`diagram_status`は構築の対象に入れておらず「Not Run」 |
| t4 | `folio_unit`・`diagram_render`・`diagram_status` | 3件成功 |
| cov | `eng/coverage.py`（作業木の`build`を構築の木へのjunctionにして実行。1回目はjunction無しで`compile_commands.json`が見つからず失敗） | 分岐カバレッジ91.26%（11458/12556、下限90%）。`mermaid_class.c`は91.6% |

### 最初の必須CIの失敗

統合の手順の必須CI（run 44207748313）が`eng/symbols.py`で落ちた: 「ARC-003: core: undeclared external symbol memchr」。
coreが使ってよい外部シンボルの一覧に`memchr`は無い。手元の確認に`eng/symbols.py`を入れていなかった（自分の手順の漏れ）。
許可の一覧は変えず、コードを直した（表示の文字列が`>`で終わるかで判定する）。手元の確認の手順に`eng/symbols.py`を足した。
直した後: 構築成功、`ctest -R`の3件成功、規約0、`Symbols: 2 libraries checked, 0 violation(s)`。

画像で気づいたこと（今回の変更とは無関係）: 見本の図で「使う」のラベルが猫からの線と重なる。配置の既存の振る舞い。

## 確かめていないこと

- Mermaid.jsとの見た目の一致（`<`・`>`の文字で示すだけ。Mermaidも同じ表記）。
- 関係の中で1つのクラスに別々のジェネリクスを書いた場合（`Box~T~`の後に`Box~U~`）は、後の書き方で名前を置き換える。Mermaidの振る舞いとの照合はしていない。
