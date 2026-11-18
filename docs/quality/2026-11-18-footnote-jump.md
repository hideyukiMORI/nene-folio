# 品質記録 — 2026-11-18 脚注の番号から脚注へ飛び、脚注の番号から本文へ戻る（#945）

hideの判断（2026-11-18、選択のページの5番「全部おすすめで」・案A）。決定は[ADR0103の補足](../adr/0103-footnotes.md)。

## 変更

- core: `footnote_target`（内部の宛先`nenefolio-note:<n>`・`nenefolio-noteref:<n>`を書く・読む）。本文の上付きの番号と脚注の行の番号をリンクのfieldにする（`markdown_inline_tokens`・`markdown_blocks`）。
- 表示: リンクの索引（`note_pane_links`）が脚注の番号の見える区間を文書順に覚える。`note_pane_show_footnote`が対の番号を選択して寄せる。
- 宛先を意図へ分ける`activate_link_target`に脚注を足した。右クリックのメニューはWebリンクの項目を出さない。
- 文書: ADR0103の補足、docs/MARKDOWN.md。CTestに`footnote_jump_native`を追加。

## 試験と結果

作業木 E:/NeNeFolio/worktrees/945-footnote-jump、構築 E:/NeNeFolio/outputs/945-footnote-jump/build。証跡の写しは D:/NeNeFolio/agents/945-footnote-jump/。

| 回 | 内容 | 結果 |
| --- | --- | --- |
| b1 | 最初の構築 | 失敗。試験の振り分けの関数がclang-tidyの長さの上限を超えた。閾値は変えず、振り分けを小さい関数へ分けた |
| b2 | 構築・整形 | 成功 |
| t1 | `folio_unit`（宛先の往復・正規形でない13通り、脚注のRTFの期待値を新しい形へ）と`footnote_jump_native` | 2件成功 |
| 直接 | `--footnote-jump` | 終了値0（44 checks）。本文の「1」（位置629）からCtrl+Enterで脚注の「1.」（702..704）が選択されて可視、そこからCtrl+Enterで本文の「1」へ戻る。「2」も同じ。番号の上の右クリックのメニューは「コピー／すべて選択」の2行 |
| visual | `--bare-url-visual`（明・暗・高コントラスト） | 終了値0。本文の番号と脚注の番号がリンクの色で、他のリンクと同じ下線が付く（bare-url-light-end.png） |

## 確かめていないこと

- マウスのクリック（試験はキャレットとCtrl+Enterの経路。クリックとCtrl+Enterは同じ`activate_link_target`へ入る）。
- 表・引用の中の脚注の番号、画像の読み込みで位置がずれた後の移動。
- 読み上げで、移った先がどう伝わるか（選択の変更として伝わるはず。未確認）。
