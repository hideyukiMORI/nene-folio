# 起動時の窓の位置と大きさの品質記録 — #891

2026-11-17。hideの依頼（再起動で窓の位置と大きさが戻る。初期値か前回の引き継ぎかを選べるように。画面から離れた・ヘッダーが見えない等の支障があれば初期値に戻す）。先行ADR0108。
作業木E:/NeNeFolio/worktrees/891-window-placement（branch feat/891-window-placement）、対象実行の原本E:/NeNeFolio/outputs/891-window-placement。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 段階

1. core: 記録の形（`window_placement`、`nenefolio-window.json`版1）と判定（`window_fit_resolve`）。
2. application / adapters: `storage_port`に窓のファイルの読み書き、`storage_state`が保持・選択の切り替え・閉じるときの記録。
3. ui/win32: `window_restore`（画面の作業領域の収集・作った後の倍率の合わせ直し・閉じるときの位置の取得）、主窓の作成と終了、設定の行「前回の窓の位置と大きさで開く」。

## 試行

| 試行 | 結果 |
| --- | --- |
| build-01 | 失敗: 単体試験の入口の関数が大きさの上限（clang-tidy）。新しい試験の呼び出しを保存先の状態の試験の入口へ移した |
| unit-01 / 02 | 失敗（試験）: 書き出しの期待を1行の文字列で比べていたが、既存のJSONの書き手は字下げ付きの複数行で書く。書いて読み直した値を比べる形（往復）へ直した |
| unit-03 | 成功。形（往復・負の座標・書けない値・14通りの拒否）と判定（画面内・はみ出し・画面をまたぐ・別の倍率・画面外・ヘッダーが上/下・つかめる幅未満・小さい/大きい・壊れた値・使えない画面） |
| conformance | CNF-002: 外部関数`window_fit`にファイルの接頭辞が無い。`window_fit_resolve`へ改名（1つ前のコミットに違反が入った。次のコミットで直した） |
| build-05 / unit-04 | 成功。保存先の状態（ファイルの値の保持・閉じるときの記録・書けないときは選択を変えない・オフでは記録しない） |
| build-06 | 成功（主窓・設定の行・終了時の記録。最初の構築で警告なし） |
| winplace-01 | 成功（`image_status_tests --window-place`、ctest `window_place_native`）。記録の位置と大きさで開く・最大化で開く・画面外の記録は初期値・ヘッダーが画面の上なら初期値・オフでは記録を使わない・閉じると位置を記録する |

設計の途中の判断: 画面ごとの倍率をOSに尋ねる関数はUIが結んでいないライブラリにあるため、ライブラリを足さず、窓を作った後に窓自身の倍率で大きさを合わせ直す形にした（ADR0108の決定5）。
既存の実窓の試験は、試験の窓が毎回初期の位置で開くよう、fixtureで選択をオフにしている（記録もしない）。

確かめていないこと: 倍率の違う複数の画面・タスクバーが上や左にある配置での実機の動き（単体の判定と、1画面の隔離desktopの実窓だけ）。hideの実機での確認に残る。

| v2（全体） | 失敗: `settings_accessibility`（設定の行を名前と順番で数える既存の試験）が、行が1つ増えたことで落ちた。試験の行の表に新しい行を足し、ボタンの行の位置（保存先・このアプリについて）を1つずらした。ほかは成功（分岐90.71%） |

## 最終の対象検証

原本は`v3-*.log`（`E:/NeNeFolio/temp/891/verify.ps1 -Tag v3`）。build（clang-tidy込み）・`eng/coverage.py`（分岐90.71%、閾値・除外不変）・
`eng/symbols.py`・`eng/conformance.py`（source/`--build-dir`）・`git diff --check`が成功し、`ctest`は63件すべて成功（114.4秒、`window_place_native`を含む）。

## 統合後に見つけた試験の副作用と直し（#894）

#891の統合・通常exeへの反映の直後に気づいた点。既存の実窓の試験のfixtureは、試験の窓を初期の位置で開くために選択をオフにしていたが、本物の`storage_adapter`を通るので、
試験のexeの隣へ`nenefolio-window.json`を`restore:false`で書いていた。試験を通常の`build/`（利用者の`NeNeFolio.exe`と同じ場所）で走らせると、利用者の選択を黙ってオフにしてしまう。
反映の時点で`C:/Users/info/WORKS/NeNeFolio/build/nenefolio-window.json`が無いこと（試験がそこで走っていないこと）を確かめた。
直し: 実窓の試験は`storage_port`の窓の読み書きを試験用のメモリ内の関数へ差し替える（`tests/win32/test_window_file.c`）。製品コードは不変。
検証（`E:/NeNeFolio/temp/894/verify.ps1 -Tag v1`）: build・網羅率・記号・規約・`ctest` 63/63が成功し、試験の後のbuildフォルダーに`nenefolio-window.json`は0件。
