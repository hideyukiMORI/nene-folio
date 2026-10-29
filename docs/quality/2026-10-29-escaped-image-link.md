# escapeした画像印と通常link — 2026-10-29

Issue: [#416](https://github.com/hideyukiMORI/nene-folio/issues/416)。基点main `bb6dc0176441623adb176112b0182b71c8b846b5`。
規則: ARC-001/004/008/010/012、C-002/005/008/011/012/016/017、QLT-001/007/008/012、GIT-001〜004。
[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)と[既存の対応範囲](../MARKDOWN.md)の契約回復。Waivers: none。

## 不具合と修正

旧`pair_links`は`[`直前のraw `!`だけで画像記法を抑止していた。backslashでescapeした感嘆符の後の通常linkも画像と見なされ、labelの既存link書式が付かなかった。旧productionへ回帰unitを追加した限定redは、期待するRTF本文との差でexit 1。入力はbackslash＋`!`＋`[label]`＋`(url)`の連続文字列。実RTFは感嘆符の後にlink装飾のないlabel・destinationを残した。red source/unit/hash/logと実行exeは保存した。

`src/core/markdown_inline_tokens.c`の`pair_links`条件に既存`escaped_at`による未escape確認を追加した。別のslash偶奇判定、画像parser、owner、allocation、公開API、schema、依存、上限、gate変更はない。`start != 0`と直前raw `!`を確認してから`start - 1`を渡すため範囲外参照はなく、code span skip・括弧pair・未閉じlinkの原文保持は既存経路を保つ。

## 共用unitと結果

`run_markdown_link_escape_tests`へ通常linkとraw画像記法の既存期待を移し、全体Markdown runと`folio_tests.exe --markdown-link-escape`が同じentryを呼ぶ。`expect_fragment`/`expect_body`を共用し、新しいparser seamやfixture複製を作らない。

- backslash 0/2本では画像記法の全原文を維持、1/3本ではliteral `!`と通常linkの書式を確認。
- 空白を挟むliteral `!`＋link、code span内、label/destinationが未閉じの2境界を確認。
- 通常linkの外側装飾とdestination中のdelimiterは既存期待を共用。RTF本文全体でslash・括弧・感嘆符の出力、labelの色/下線、destination非表示を確認。

| 対象 | コマンド/結果 |
| --- | --- |
| Debug configure | `cmake -S D:/NeNeFolio/worktrees/416-escaped-image-link -B D:/NeNeFolio/outputs/416-escaped-image-link/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、exit 0 |
| 旧production red build | `cmake --build D:/NeNeFolio/outputs/416-escaped-image-link/build --target folio_tests -j 4`、exit 0 |
| 限定red | `folio_tests.exe --markdown-link-escape`、期待するRTF不一致でexit 1 |
| green build/tidy | 同じtarget/引数、clang-tidy込みexit 0。修正sourceと共用unitを再compile/link |
| 限定green | 同じselector、exit 0 |
| C/header format、`git diff --check` | 10:21:58 JST、両方exit 0 |
| Full conformance | 初回10:22:04 JSTは記録例2件をCNF-006がrelative linkと誤認しexit 1。分割表記へ修正・mainへrebase・親の状態文書更新後、`python -B eng/conformance.py --build-dir D:/NeNeFolio/outputs/416-escaped-image-link/build`は0件/exit 0。原本は`parent-conformance.log`と`parent-static-result.json` |
| 品質記録の文書検査、diff | 記録完成後、変更文書だけに`eng.conformance.document_checks`を適用し0 findings/exit 0、`git diff --check` exit 0。C/header formatは既成功結果を再利用 |

実コマンド、環境、結果JSON、入力hash、red sourceとlogは`D:/NeNeFolio/agents/416-escaped-image-link/`に保全。greenの4入力hashは作業木の現ファイルと一致し、redの4入力hashは保存snapshotと一致。red exe hashは`red-binary-hash.json`と一致。self-reviewと一覧は同directoryの`self-review.md`、`commands.json`、`final-hashes.json`、`completion.json`に記録する。

## 再利用・残る範囲

RTFの既存link制御語とnative描画/投入経路は不変なので、[基本構文の既存native検証](2026-10-26-markdown-syntax.md)を再利用する。新しいnative操作は行わず、core RTF生成の対象selectorのみ実行した。通常desktop、別native harness、無関係なOOM/UI/検索/性能計時、full local gateは行っていない。最終head必須CI、rebase、commit/PR、統合・通常反映と保全後の安全監査/整理は親担当。CommonMark完全互換、画像表示、link先openは対象外。

作業木`D:/NeNeFolio/worktrees/416-escaped-image-link`とbuild/output`D:/NeNeFolio/outputs/416-escaped-image-link`は統合・保全まで保持する。red source snapshotとred exeは旧実装失敗の唯一の再現原本。稼働process参照はない。文書検査で生成されたignored `eng/__pycache__/conformance.cpython-312.pyc`とそのdirectoryの削除は、自動承認レビューが`blocked by policy`で拒否した。追加理由は返されていない。該当pathの削除は再試行せず、含まれるworktreeも保持する。通常data/Nibと既存の削除拒否済み28箇所は操作しない。


