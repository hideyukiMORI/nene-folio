# 編集で保てないノートの知らせ（#258 / #242 / #245）の実機確認 — 2026-10-12

ゲート外の確認。設計席（Fable）の指示書で probe 席（Opus・1 席 1 仕事）が PostMessage と PrintWindow（`tools/post-keys.ps1` / `tools/capture-window.ps1 -TargetPid`）で撮った。
SetForegroundWindow・SendKeys・実マウスは使っていない。hide の実機（root の `build/NeNeFolio.exe`）は起動していない。

## 方法

- exe: #258 の枝 `D:/NeNeFolio/worktrees/258-notice/build/NeNeFolio.exe`（source `35655b7`・3,251,712 B・SHA-256 `4e0095f371d5555e0a087686932a8b78bce45ef09f5ff4f6b18d53ec7306d62a`）を `D:/NeNeFolio/check/2026-10-12/258/` へ写し、`fonts/` を隣へ写して実行
- msftedit.dll 10.0.26100.8875・画面 125%（主窓 1200×800、ドロワー 300px = 240 × 1.25）・Windows 11 Pro 10.0.26300
- `data/` の用意: `data/` 無しで起動 → `:newcat 仕事` → `:enew`「plain」Ctrl+S「きれい」→ `:enew`「A B」Ctrl+S「保てない」→ `:q`。
  その後 `data/仕事/保てない.md` を `41 E2 80 A8 42 0A`（A U+2028 B LF）に書き換えて雛形にした
- 場面ごとに雛形から `data/` を作り直し、`data/settings.json` を `{"version":3,"number":false,"theme":"<theme>","language":"<lang>"}` で置いた
  （雛形の作成時点では settings.json は存在しなかった）。v10 だけ「きれい.md」を `あいうえお`×20＋LF の繰り返し 524,342 B に置き換えた
- 操作は索引の `j` / `k`・`i`・`Esc`・Ex。札とノート行のクリックは使っていない
- v09 は `SetWindowPos(100×100, NOMOVE|NOZORDER|NOACTIVATE)` で最小寸法へ（窓は 700×450 = 560×360 @125% に止まった）
- 補助スクリプトと log: `D:/NeNeFolio/agents/258-visual/`（`scene.ps1` / `k.ps1` / `listwin.ps1` / `run.log`）

## 結果

| 番号 | 操作 | 見たこと | 判定 | 絵 |
| --- | --- | --- | --- | --- |
| v01 | 「きれい」で `i` → `Esc` → `j` | 「保てない」が閲覧で開き「閲覧」の札が選ばれ、右ペイン下端に「閲覧で開きました。編集で保てない文字があります。」 | PASS | `v01.png`（前段 `v01-b.png`） |
| v02 | v01 の後 `k` | 1 行が消え、「きれい」が閲覧のまま | PASS | `v02.png` |
| v03 | v01 を作り直して `:` | 1 行が消え Ex の欄が出る | PASS | `v03.png`（前段 `v03-pre.png`） |
| v04 | 閲覧のまま `j` `j` で「保てない」 | 1 行は出ない | PASS | `v04.png` |
| v05 | v04 の後 `i` | 箱「この本文には編集で保てない文字（U+0007・U+2028・U+2029）があります。閲覧のままにしました。」 | PASS | `v05.png` |
| v06 | v04 の後 `:startinsert`（指示書の `:edit` は Ex に無く「一致する操作がありません」→ `v06-edit-unknown.png`） | 箱は出ず欄の 1 行。ただし描かれた字は「この本文には編集で保てない文字(U+0007 U+2028 U+2029があります。閲覧のままにしました」で、**「・」2 つ・「）」・末尾の「。」が描かれていない**（ui_text の文字列は箱と同じ） | FAIL（表示の欠け・既存・#260） | `v06.png` |
| v07 | English で v01 | "Opened in view mode. It has characters the editor cannot keep." | PASS | `v07.png` |
| v08 | 简体中文で v01 | 「已以查看模式打开。正文含有编辑时无法保留的字符。」 | PASS | `v08.png` |
| v09 | 最小寸法で v01 | 日本語は 700px 幅でも全文が収まり省略は起きない（先頭は残る）。追加の `v09-en.png` で English は「Opened in view mode. It has characters...」と末尾省略 | PASS | `v09.png` / `v09-en.png` |
| v10 | 「きれい」512 KiB で `i` | 編集に入り（「編集」の札）本文の先頭が出る。`number:false` だったので番号帯は `:set number` 後の `v10-number.png` で確認（1〜5）。終了後も md は 524,342 B のまま・`.history` なし | PASS | `v10.png` / `v10-number.png` |
| v11 | light / dark で v01 | 両テーマで 1 行が読め、地に沈まない（コントラスト比は測っていない） | PASS | `v11-light.png` / `v11-dark.png` |

## 残ること

- v06 の欠け: 欄の 1 行（Ex の失敗行）が「・」を空白に、「）」を次の字に重ね、末尾の「。」を描かない。同じ文言・同じ `mono_font` の右ペイン下端（v01）では正しく描けるので、Ex の欄の 1 行の描画経路に固有の既存の欠陥。#258 の差分ではないので切り離し、Issue #260 に記録した（原因は未調査）
- クリック経路（ノート行・「編集」の札）は撮っていない。パレットでの消え方も見ていない
- 简体中文は母語話者の確認なし。hide の目視は未
