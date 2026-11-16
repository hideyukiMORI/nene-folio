# リンクの色をテーマ色へそろえる品質記録 — #852

2026-11-17。開始main `f690d9e`（#850統合後）。設計は[ADR0100](../adr/0100-web-link-instruction-scheme-case.md)。
作業木E:/NeNeFolio/worktrees/852-link-color（branch fix/852-link-color）、対象実行の原本E:/NeNeFolio/outputs/852-link-color、
probeと下書きの原本D:/NeNeFolio/agents/link-color-20261117（完了後に対象実行の記録もここへ移す）。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 発見の経緯

#850の実画面の採取で、表示名がURLそのもののリンク（囲まないURL・`<URL>`）だけが明ではWindowsの青、暗では本文色の白に下線で描かれ、
`www.`と表示名付きのリンクはテーマのリンク色だった（`D:/NeNeFolio/agents/bare-url-20261117/visual/`）。ADR0079で既知の挙動。

## 調査（製品の外のprobe、`probe.c`）

隔離desktopの`RICHEDIT50W`（Msftedit 10.0.26100.9949）へ、明・暗の2配色 × RTFのfield 4形 × 読込後の処置5つを流し、
TOMのlink typeと`WM_PRINT`で描いた画素を数えた。

| 試行 | 結果 |
| --- | --- |
| probe-01 | link typeは取れたが、子窓への`PrintWindow(PW_CLIENTONLY)`が黒を返し、全画素が「その他」になった。画素の数は無効 |
| probe-02 | `WM_PRINT`へ変えて取り直し。結果はADR0100の表。命令のschemeだけ大文字の形はlink type 3（friendly name）で、明暗ともテーマ色だけ・青0。読込後の色の変更は効かず、TOMの変更は`E_ACCESSDENIED`（読み取り専用の窓） |

照合した資料: Murray Sargent「Autolink color contrast」（autoURLの色はクライアントが指定できず、コントラスト不足では白へ落とす）、
Microsoft Q&A（CFE_LINKへの`crTextColor`が効かない報告）。web検索の結果はIssue #852のコメントにも残した。

## 実装の試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| build-01 | clang-tidy `readability-function-size`: `emit_field`の入れ子が深い（switch→case→for）。命令を書く`append_instruction`へ分けた |
| build-02 | clang-analyzer: `web_uri_create`で複写先の`owned`を読むのを初期化前と判定。元の`bytes`から畳む形にした |
| build-03 | clang-tidy: 試験の`verify_uri_cases`が60行を超えた。受理側を`verify_uri_accepted`へ分けた |
| unit-04 | 1件失敗: 山括弧の行先に`{x}`を含む表示名付きリンクの期待が小文字のscheme。期待を大文字へ直した（他の30件は機械的に置換） |
| unit-05 / 06 | `folio_tests.exe`全体成功。stateの偽portへ大文字の`HTTPS://`を渡すと小文字で届くことを追加 |
| native-01 | 予想どおり2件失敗: paneが返すリンク先が命令の`HTTPS://...`になった（`minimap_structure_tests`・`image_display_tests`）。期待を更新 |
| native-02 | 新しい色の検査が失敗（青151）。窓全体を数えたため、日本語の黒い字のClearTypeの縁の青を拾っていた（DIAGで座標を出して確認）。URLの文字の矩形の中だけを数える形に直した |
| native-04 | 成功（275 checks、テーマ色256・青0） |
| 変異 | 命令のschemeを小文字へ戻す変異で、色の検査が「テーマ色0・青525」で失敗することを確認（mutant-run-02）。最初の変異（mutant-run）はリンク先の文字の検査で先に落ちたので、色の検査を先に置いて取り直した |

## 実画面（`image_status_tests --bare-url-visual`）

`visual/after-{light,dark,contrast}.png`。#850の時点の採取（before）と並べ、表示名がURLそのもののリンクも`www.`・表示名付きと同じテーマ色になったことを目で確かめた。
高コントラストは飾らない（文字色と下線）まま。

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/852/verify.ps1 -Tag v1`、構築`--parallel 4`、coverage・CTestは順に1回ずつ）。

| コマンド | 結果 |
| --- | --- |
| `cmake --build <E:build> --parallel 4`（全target・clang-tidy込み） | 成功 |
| `python -B eng/coverage.py` | 成功。分岐90.88%（下限90・除外不変）。`web_uri.c` 89.47%、`markdown_inline_tokens.c` 90.88% |
| `python -B eng/symbols.py --build-dir <E:build> --require core application` | 成功（違反0） |
| `python -B eng/conformance.py` / `--build-dir <E:build>` | v1では2件失敗（CNF-006: この記録の中に書いたリンクの書き方の例を相対リンクと判定）。言い換えて再実行し、違反0 |
| `ctest --test-dir <E:build> --output-on-failure --no-tests=error` | 61件すべて成功（95.3秒） |
| `clang-format --dry-run --Werror`・`git diff --check origin/main` | 成功 |
