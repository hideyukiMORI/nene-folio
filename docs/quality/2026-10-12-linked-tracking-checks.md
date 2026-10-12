# フォントリンクで描く行の字間（#260）の前後の実機の絵 — 2026-10-12

ゲート外の確認。設計席（Fable）の指示書で probe 席（Opus・1 席 1 仕事）が PostMessage と PrintWindow（`tools/post-keys.ps1` の写しに PgDn を足した `post-keys-pgdn.ps1`／`tools/capture-window.ps1 -TargetPid`）で撮り、`tools/compare-screens.ps1` で比べた。
SetForegroundWindow・SendKeys・実マウスは使っていない。root の exe と hide の実機は起動していない。

## 方法

- before: main `9dae8e8` の `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe` を `D:/NeNeFolio/check/2026-10-12/260/before/` へ写した（3,251,200 B・SHA-256 `bec373abb5fae66feffe203bc4d112a7921f72068917a11a4f6981b4a6df86ce`）
- after: #260 の枝 `fix/260-linked-tracking`（HEAD `1da5c30`）の `D:/NeNeFolio/worktrees/260-tracking/build/NeNeFolio.exe` を `.../260/after/` へ写した（3,253,760 B・SHA-256 `00bd5e8d65478e47429b7da5fe82cf5a630879408fbe9ac2d010763f4534798a`）。exe の時刻 12:02:35 は commit 12:07:25 より前だが、作業木は clean で exe より新しい `src/` のファイルは 0
- `fonts/` は両方へ `C:/Users/info/WORKS/NeNeFolio/fonts/` から写した。主窓 1200×800（既定寸法。画面 125% は #258 の記録と同じ機械で、今回は測り直していない）・Windows 11 Pro 10.0.26300
- `data/`: #258 の雛形 `D:/NeNeFolio/agents/258-visual/data-template`（`仕事/きれい.md`「plain」・`仕事/保てない.md` = `41 E2 80 A8 42 0A`）を `D:/NeNeFolio/agents/260-visual/data-template` へ写し、`仕事/メモ（仕事）・下書き。.md`（`plain`）を置いて `index.json` の末尾に名前を足した。
  場面ごとに雛形から `data/` を作り直し `settings.json` を `{"version":3,"number":false,"theme":"dark","language":"ja|en"}` で置いた。1 場面 1 プロセス（撮影後に自分の PID だけを止めた）
- 補助スクリプト・log・拡大図: `D:/NeNeFolio/agents/260-visual/`（`scene.ps1` / `post-keys-pgdn.ps1` / `run.log` / `compare.log` / `z0*.png`）

## 結果

| 番号 | 操作 | before | after | 判定 | 差の画素（compare-screens） |
| --- | --- | --- | --- | --- | --- |
| t01 | `j` `j`（閲覧で「保てない」）→ `:startinsert` | 欄の 1 行が「文字（U+0007 U+2028 U+2029があります。閲覧のままにしました」:「・」2 つ・「）」・末尾「。」が欠ける（v06 と同じ） | 「文字（U+0007・U+2028・U+2029）があります。閲覧のままにしました。」が全部描かれる | PASS | 4,559（下端の 1 行・札「閲覧」「編集」・パンくず） |
| t02 | `j` `j` `j`（「メモ（仕事）・下書き。」） | パンくずは before でも欠けない（字間 0 の経路） | 欠けず重ならない。ドロワーの行は差 0 | PASS | 2,030（パンくず・札・ボタン列の日本語） |
| t03 | `:h` → Tab（キー操作を表示）。「ごみ箱へ移す（確認なし）」は 1 枚目の末尾で見えたので PgDn は 0 回 | 「:trash」に「ノ」が重なり、「:delcat」に「空」が重なる。多くの行で ASCII と日本語の境目が詰まる | 重ならず、全角括弧も揃う | PASS | 38,092（ヘルプの行の全面） |
| t04 | Ctrl+P（日本語） | 頭の 2 行で「する」「と」が重なる（「するど」に見える） | 重ならない。操作名の列は 1 字ずつ詰まる | PASS | 7,195。別名の列（x 700〜870）は差 0 |
| t05 | English で Ctrl+P | — | 一覧の行・別名・頭の 2 行・Show keys は差 0。差は 2 か所: ボタン列（`Actions ▾` が非 ASCII の ▾ を含むので字間が外れて 9 px 狭まり、`Help` と `Find in this note` の箱が 9 px 左へ）と、下端の `↑ Prev ↓ Next`（before は `Prev↓` がくっついていた。after は間が空き、字間は外れる） | PASS（ASCII だけの行は差 0）・ただし注記あり | 3,247（y 60〜94 と y 735〜743 だけ） |
| t06 | English で `j`（「きれい」） | — | 札 `View` / `Edit` は差 0。差はパンくずの「きれい」（日本語・1 字 1 px 詰まる）と t05 と同じボタン列の 9 px | PASS・注記あり | 1 回目 3,523、撮り直し（r2 同士）3,063 |

再現性: 同じ exe を 2 回撮ると t05 before は 17 px（x 336 の欄のキャレット）、t05 after と t06 after は 0、t06 before は 460 px（ドロワーの `Search all notes` と本文の `plain` の描き途中）。t06 1 回目のドロワー・本文の差はこの揺れで、r2 同士では消える。

## 残ること

- English の UI にも非 ASCII を含む文字列（`Actions ▾`・`↑ Prev ↓ Next`）があり、その 2 つは規則どおり字間が外れて幅が変わる（`Actions ▾` は 9 px 狭まる＠1200×800）。設計席はこれを受け入れ、チェックリスト 260-3 の期待を「ASCII だけの行は変わらない。▾ や ↑↓ を含む行は字間が外れる」に直した
- 日本語の札「閲覧」「編集」は after で詰まる（差の画素に入る）。字形の欠けはない
- t03 / t04 の撮影で、`:h{ENTER}` の Enter の WM_CHAR がパレットの欄へ 1 字入る（欄に「,」状の印・一覧が空になる）。before / after とも同じ操作の産物で、製品の欠陥かは見ていない
- クリック経路・ライトテーマ・简体中文・96 / 144 DPI・最小寸法は撮っていない。hide の目視は未
