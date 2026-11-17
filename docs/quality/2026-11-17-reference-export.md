# 参照形式の画像を含むノートのZIP書出しの品質記録 — #865

2026-11-17。開始main `d2fabba`（#863統合後）。試験だけの変更（製品の変更なし）。
作業木E:/NeNeFolio/worktrees/865-reference-export（branch test/865-reference-export）、対象実行の原本E:/NeNeFolio/outputs/865-reference-export。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 目的

#863（PR864）の残るリスク「書出し全体（`image_export_append` → ZIP）を参照形式の画像で通す試験が無い」を閉じる。

## 試験

`tests/unit/image_tests.c`の`export_references`（`export_contract`と同じ偽port、外の画像も含める選択）。
定義が段落の前、同じ段落に同じ定義を参照する2つの画像と、その間の行内の画像（別のdirectoryの同名の画像）。
書出しが成功し、同梱の画像が2つ（同じ定義の画像は1回）、定義の行先と行内の画像が書き換わり、titleと参照の文字は残り、元の原文は変わらず、ZIPが作れることを確かめる。

| 試行 | 結果 |
| --- | --- |
| unit-01 | 成功（現在のmain） |
| red（有効でない） | #863の直しの前の`markdown_image_copy.c`へ戻すと失敗したが、先に走る#863の単体試験`reference_relocation`が落ちたので、今回の試験が欠陥を捉えるかは示せていない |
| red（無効） | `reference_relocation`の呼び出しを外すと「未使用の関数」で構築が落ち、古い実行物が走った。無効と記録 |
| red（有効） | `reference_relocation`を呼ばずに参照だけ残す形で構築し、直す前の実装で「a note with shared and out-of-order reference images exports」が失敗することを確かめた。ファイルを戻した |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/863/verify-865.ps1 -Tag v1`）。build（clang-tidy込み）・`eng/coverage.py`（分岐90.96%、閾値・除外不変）・`eng/symbols.py`・`eng/conformance.py`（source/`--build-dir`）・`git diff --check`が成功し、
`ctest`は61件すべて成功（182.5秒。他製品の席の負荷で前回より長い）。
