# #220 — 別名保存後の空の履歴面

Issue: [#220](https://github.com/hideyukiMORI/nene-folio/issues/220)。規則: ADR0038 決定10、ADR0016、ARC-001 / ARC-011、C-012、QLT-013。Waivers: none。

基点は `4330745`。`folio_window.c` の `finish_save_command` に、履歴0件の面を保存の成否共通で閉じる後処理を加えた。
既存の保存・失敗通知・面を閉じる経路を使い、失敗箱から戻った後にも有効な戻り先focusを復帰する。公開前失敗と取消は履歴面を保持する。
application / API / スキーマ / 依存 / ゲートは変更していない。

## 隔離 Win32 / RichEdit probe

成果物は `D:/NeNeFolio/agents/220-empty-history-surface/` の `ui_probe.c`、`build-probe.ps1`、`probe-*.log`、`probe-results.json` に保持する。
製品の `folio_window.c` をそのまま取り込み、製品applicationとUIライブラリを結合した。保存はfake portsで制御し、user app/dataは操作していない。
名前面・失敗箱は実Win32部品、本文は実RichEdit。thread限定CBT hook / timerで自分のmodalだけを操作する。
Ctrl+Shift+Sは隔離threadのkeyboard状態と製品HACCELを通す。公開前失敗は同じSAVE_AS dispatcherの引数付き入口を使う。
物理キー・IME・画面の目視を実測済みとは扱わない。既存renameの成功試験は再実行していない。

| ケース | PASS / FAIL | 履歴件数 / 面 | focus | modal / 理由 |
| --- | --- | --- | --- | --- |
| 修正前 LEDGER_STALE | 35 / 0 | 0 / HISTORY（欠陥再現） | 履歴layer | 名前面1・失敗箱1・台帳失敗理由一致 |
| 修正後 READY | 34 / 0 | 0 / CLOSED | 元の本文 | 名前面1・失敗箱0 |
| 修正後 LEDGER_STALE | 36 / 0 | 0 / CLOSED | 元の本文 | 名前面1・失敗箱1・台帳失敗理由一致 |
| 公開前 NOTE_STORE_FAILED | 32 / 0 | 5 / HISTORY | 履歴layer | 名前面0・失敗箱1・保存失敗理由一致 |
| 名前面取消 | 32 / 0 | 5 / HISTORY | 履歴layer | 名前面1・失敗箱0 |

修正後4ケースは134 PASS / 0 FAIL。すべて本文・選択・Undoを保持し、実際の `EM_UNDO` 1回でprobeが加えた末尾編集を戻せた。
公開後は新ノート `fresh`、公開前失敗/取消は元ノート `alpha` のままであることを確認した。失敗理由は実STATICの文字列と正典文言を照合し、1回だけ表示した。

## 実行コマンド

- `cmake -S D:/NeNeFolio/worktrees/220-empty-history-surface -B D:/NeNeFolio/agents/220-empty-history-surface/product-build -G Ninja -DCMAKE_BUILD_TYPE=Debug` → exit0。
- `cmake --build D:/NeNeFolio/agents/220-empty-history-surface/product-build --target nenefolio_window nenefolio_adapters` → 修正前exit0。
- `cmake --build D:/NeNeFolio/agents/220-empty-history-surface/product-build --target nenefolio_window` → 修正後exit0、対象compile/clang-tidy成功。
- `pwsh -NoProfile -File D:/NeNeFolio/agents/220-empty-history-surface/build-probe.ps1` → 修正前・修正後とも最終exit0。ASan / UBSanを付けてprobeをcompile/link。
- `ui_probe.exe before-stale` → exit0（欠陥の状態を期待する修正前再現）。`ui_probe.exe ready|stale|prepublish|cancel` → 各exit0。個別引数で実行し、表へ集計した。
- `git diff --check` → exit0。

probe準備中には結果enumの取り違えによるcompile失敗が2回あり、製品の `unpublished` の写像を読んで `NOTE_STORE_FAILED` へ訂正した。
これはprobeだけの修正で、製品ゲートの除外や抑制は追加していない。probe translation unitの既存nullability警告は製品compile/clang-tidyの成功とは区別する。

## 最終ゲート

source checkpoint `51d0c62` の静読受理後、正典 `pwsh -NoProfile -File ./eng/check.ps1` を1回実行してexit0。
製品compile/clang-tidy、conformance、CTest 2/2、分岐カバレッジ3214/3430 = 93.70%（下限90%）、負例の7.35%拒否、ゲート証明19件、diff --checkが成功した。
ログは隔離成果物の `gate.log`。`out/coverage/complete.json`・`negative.json`・`results.json` と `out/proofs/results.json` を `gate-json/` へ保存し、source/保存先のSHA256一致を `gate-json-hashes.json` に記録した。
製品source/tests/CMakeはcheckpointから不変。文書のみの結果追記で製品ゲートを反復しない。

## 残る範囲

IME中のmouse置換は別の監査候補であり、未修正。この枝は変更していない。今回の公開後失敗・公開前失敗・取消の直接境界に試験を限定した。
