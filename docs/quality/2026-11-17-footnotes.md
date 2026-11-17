# 脚注の品質記録 — #869

2026-11-17。開始main `efd952f`（#865統合後）。設計は[ADR0103](../adr/0103-footnotes.md)。
作業木E:/NeNeFolio/worktrees/869-footnotes（branch feat/869-footnotes）、対象実行の原本E:/NeNeFolio/outputs/869-footnotes、
下書きと完了後の置き場D:/NeNeFolio/agents/footnotes-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 選んだ理由

ロードマップ「MarkdownとMermaidの整理」の残りのうち、字下げコード（既存のノートの見え方を変える）とtaskのクリック切替（閲覧からファイルを書き換える）はhideの判断が要る。
脚注は`[^...]`以外の見え方を変えず、#861の定義の表の上に乗るので、次に選んだ。

## 試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| build-01 | 成功（最初の構築で警告なし） |
| unit-01 | 1件失敗（試験の期待）: `[a][^x]`を「両方とも原文」と書いたが、実際は`[a]`の後に脚注の番号。脚注のラベルはリンクの参照形式の名前にならず、後ろの`[^x]`は単独の参照として読まれる。GitHubも同じなので期待を直した |
| unit-02 | 成功（`folio_tests.exe`全体） |
| native-01 | 失敗（試験の期待）: 検索用の本文で、脚注の前の細い区切りが空の段落3つ（`---`の水平線と同じ）として現れた。期待へ入れた。途中、Bashのheredocでbackslashが崩れ、Editで直した |
| native-02 | 成功（784 checks）。番号の順・上付きの書式・脚注の文字が本文より小さいこと |
| 実画面1 | 脚注が窓の下端より下にあり写らなかった。見本の採取に末尾までスクロールした採取（`-end`）を足した |
| 実画面2 | `visual/footnotes-{light,dark,contrast}{,-end}.png`。番号がリンクの色で上付き、細い区切り、一回り小さい控えめな色の一覧、脚注の中の太字と参照形式のリンクを目で確かめた |
| v1（全体） | 成功（61/61、分岐90.88%）。`markdown_inline_tokens.c`は89.78%（下限はファイルごとではなく全体） |
| 補強 | 確保失敗の注入元へ脚注（太字とリンク入り）を足した。v2でも`markdown_inline_tokens.c`は89.78%のままで、未到達の分岐は脚注以外（既存の分岐）にある。閾値・除外は変えていない |

## 最終の対象検証

原本は`v2-*.log`（`E:/NeNeFolio/temp/869/verify.ps1 -Tag v2`、#867統合後のmainを取り込んだ後）。

| コマンド | 結果 |
| --- | --- |
| `cmake --build <E:build> --parallel 4`（全target・clang-tidy込み） | 成功 |
| `python -B eng/coverage.py` | 成功。分岐90.91%（下限90・除外不変）。`markdown_references.c` 91.45%、`markdown_blocks.c` 90.88%、`markdown_inline_tokens.c` 89.78% |
| `python -B eng/symbols.py --build-dir <E:build> --require core application` | 成功（違反0） |
| `python -B eng/conformance.py` / `--build-dir <E:build>` | 成功（違反0） |
| `ctest --test-dir <E:build> --output-on-failure --no-tests=error` | 61件すべて成功（96.0秒） |
| `git diff --check origin/main` | 成功 |
