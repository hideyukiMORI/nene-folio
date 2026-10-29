# Markdown link destination scope — 2026-10-29

Issue: [#420](https://github.com/hideyukiMORI/nene-folio/issues/420)。基点main `88c441f2575fb003861b3fba2692d29d2909f9db`。
規則: ARC-001/004/008/010/012、C-002/005/008/011/016/017、QLT-001/007/008/012、GIT-001〜004。Waivers: none。
[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)の既存LINK契約・範囲安全性の修正。

## 不具合と修正

`pair_links`が成立したLINKのdestination終端まで走査を進めず、label閉じtokenの次からdestination内を再走査していた。そのため非表示destination内の括弧/link候補が後続の可視linkを誤って飲み込み、delimiter処理に不完全なscope状態を渡す場合があった。

旧sourceの限定selectorはexit 1。保存済みsanitizer logは`markdown_inline_tokens.c:302:15`のunsigned offset overflowを示し、親の独立static traceと一致した。redは再実行していない。旧source/testの4 snapshot hash、red executable hash、build/selector logは`D:/NeNeFolio/agents/420-link-destination-scope/`と`D:/NeNeFolio/outputs/420-link-destination-scope/`に保全した。

修正は`pair_links`の走査位置を`close`から既に求めているdestination終端`end`へ進める1行。`after=end+1`と同じ消費範囲でdestinationを再分類しない。画像判定、scope欠落fallback、mate、parser、owner、確保、公開APIや保存形式は変えない。

## 共用unitと対象検証

`run_markdown_link_tests`を全体Markdown runと`--markdown-links` selectorが共用する。完成済みのunit diffは期待値を維持し、以下の範囲を公開RTF本文で確認する。

- destination内部の角括弧が後続の可視linkへscopeを漏らす境界と、destination内で括弧が閉じる境界。
- 連続通常link、label内強調、外側強調、inline code、destination内code span。
- #416で追加したescaped `!`、raw画像記法の原文保持、backslash偶奇、空白と未閉じlinkの期待。

| 対象 | 実行と結果 |
| --- | --- |
| 旧source red build/tidy | 固定Debug configure後、`cmake --build D:/NeNeFolio/outputs/420-link-destination-scope/build --target folio_tests -j 4`、exit 0（保存済） |
| 旧source限定red | `folio_tests-red.exe --markdown-links`、UBSan診断でexit 1（保存済、再実行なし） |
| green build/tidy | 同build target、exit 0。修正sourceを再compile/link |
| green selector | `folio_tests-green.exe --markdown-links`、exit 0。`Markdown link scope and escape tests passed` |
| format / full conformance / diff | `run-static.ps1`の結果は`D:/NeNeFolio/agents/420-link-destination-scope/static-result.json`を正とする |

green buildと同selectorは2026-10-29 11:07:49 JSTに成功した。red/green入力hashとred executable hash、全コマンド/raw log/resultは同agents directoryに保存する。自己レビューと最終hash/cleanup保持理由はself-review/manifestへまとめる。

## 再利用・残る範囲

変更はcoreのRTF変換と既存unitのみ。native control、GUI、通常desktop、OOM、検索、性能計時、全local gateは対象外。既存Markdownのnative検証は[基本構文の既存検証](2026-10-26-markdown-syntax.md)を再利用し、今回native検証を増やさない。CommonMark完全互換、画像表示、link先を開く動作は対象外。

作業木`D:/NeNeFolio/worktrees/420-link-destination-scope`、build/green-red executable、red snapshotは親の統合・保全監査まで保持する。日報/current/handoff、commit/push/PR、rebase/main、通常反映とcleanupは親担当。通常data/Nib・削除拒否済み31箇所は操作しない。cleanupは試さない。
