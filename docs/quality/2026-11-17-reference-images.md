# 画像の参照形式の品質記録 — #863

2026-11-17。開始main `95f452f`（#861統合後）。設計は[ADR0102](../adr/0102-reference-images.md)。
作業木E:/NeNeFolio/worktrees/863-reference-images（branch feat/863-reference-images）、対象実行の原本E:/NeNeFolio/outputs/863-reference-images、
完了後の置き場D:/NeNeFolio/agents/reference-links-20261117（#861と同じ）。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 調べたこと

#861では、画像の読込経路が「いま描いているview」を前提にすると見て、画像の参照形式を後回しにした。
`markdown_images_append`を読むと、使うのはspanが指す原文のbytesの位置（`span->bytes - source`）だけで、同じノートの原文を指すspanならどの行でもよい。
定義の行を1つのspanにしたviewをそのまま渡せば、相対path・原文の位置・遅延読込・上限・書出しの書き換えが既存の経路のまま働く。
書出し（`image_export.c`）も同じ描画（`markdown_rtf_create`）から画像を集めるので、別の変更は要らない。

## 試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| build-01 | clang-tidyの認知的複雑度（`pair_reference_at`が11）。ラベルを引く所を`reference_of`へ分けた |
| unit-02 | 1件失敗: #861の試験「画像の参照形式は原文のまま」。今回の意図した変更なので、画像の枠になることを確かめる期待へ直した |
| unit-03 | 成功。`image_tests`の新しい`reference_contract`（3つの形・記録された範囲が定義の行先の原文を指す・展開は1回・見つからないラベル・同じ定義へのリンク） |
| native-01 | 一度目は試験の関数の追加が編集道具の事前読込の不足で入らず、呼び出しだけが入って構築が落ちた。足し直して成功（`image_display_tests` 200 checks。画像の枠として入り、定義も参照の文字も表示されない） |

## 統合の直前に見つけた欠陥（書出し）

最初のPR864の必須CIの待ちの間に、書出しの経路（`image_export.c` → `markdown_image_copy_create`）を読み直した。
書き換えは、受け取った位置が原文の順で重ならないことを要求していた。参照形式の画像では、
同じ定義を参照する2つの画像が同じ位置を持ち、行内の画像が参照の画像より先に集まるので順も崩れる。
このままでは、参照形式の画像を含むノートの書出し（ZIP）が失敗する。統合の手順を止め（PRをdraftへ戻した）、直した。

| 試行 | 結果 |
| --- | --- |
| red | 新しい単体試験`reference_relocation`（定義が段落の前、同じ段落に行内の画像と同じ定義への参照が2つ）が、直す前の実装で失敗 |
| 診断 | 最初の試験は、集める順（行内が先）を取り違えて行先を当てていた。画像ごとの行先を元の行先から決め、順が崩れて重複する入力になっていることを試験の中で確かめる形にした |
| red（無効） | 直す前の実装へ戻す手順でPowerShellが行を連結し、構築が落ちて古い実行物が走った（成功に見えた）。無効と記録し、`git show`で取り直した |
| red（有効） | 直す前の実装で「shared and out-of-order reference spans relocate」が失敗 |
| green | `markdown_image_copy_create`が位置の順に並べ（入力はほぼ整列済みなので挿入）、同じ位置を1つにし、同じ位置を別の行先へ書き換える指示を拒む。全体の単体試験が成功 |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/863/verify.ps1 -Tag v1`）。

| コマンド | 結果 |
| --- | --- |
| `cmake --build <E:build> --parallel 4`（全target・clang-tidy込み） | 成功 |
| `python -B eng/coverage.py` | 成功。分岐90.97%（下限90・除外不変）。`markdown_references.c` 93.33%、`markdown_inline_tokens.c` 90.44% |
| `python -B eng/symbols.py --build-dir <E:build> --require core application` | 成功（違反0） |
| `python -B eng/conformance.py` / `--build-dir <E:build>` | 成功（違反0） |
| `ctest --test-dir <E:build> --output-on-failure --no-tests=error` | 61件すべて成功（92.6秒） |
| `git diff --check origin/main` | 成功 |
