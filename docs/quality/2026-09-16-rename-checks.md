# 名前変更の確認（#56 / ADR0022）

2026-09-16。ゲート内の単体（偽ポート）、ゲート外の実アダプタ測定、ゲート外のWindows部品測定の3つに分けて記録する。
いずれも利用者の `data/` と起動中のアプリには触れていない。物理キー・IME候補窓・高DPIの目視は含まない。

## ゲート内の単体（偽ポート・決定性）

`tests/unit/state_tests.c` に改名の8本を追加した。

- 未選択は `NOTHING_SELECTED`、無題は `NAME_REQUIRED`（初回保存が先）。どちらも `data/` を触らない。
- 同じ名前は `READY` で改名を呼ばない。`ONE` のように大小文字だけ違う名前と、既存の他ノート名は `NAME_TAKEN`。
- 編集中の未保存本文は `archive` → `write`（旧名）→ `rename` の順で先に確定する。
  本文が書けなければ `NOTE_STORE_FAILED` で意図を確保しない。
- 台帳未同期（別名保存の `LEDGER_STALE` 後）は `LEDGER_UNSYNCED` で改名を呼ばず、修復後は同じ改名が通る。
- 記録公開前の5つの拒否（ロック無し・同名・非対応・識別子・記録書込）はそれぞれの理由を返し、
  意図を残さないので次の選択・トグルが再試行なしで通る。
- `RENAME_PENDING` と `RENAME_HALTED` のあいだ、保存・切替・トグル・色変更・カテゴリ移動・ノート移動・新規・
  別名保存・終了確認の 9 つがすべて先に同じ改名を `RENAME_RESUME` で再試行し、同じ理由を返して本文も台帳も
  書かない。別の名前の改名も始まらない。パンくずは実名のまま `recovering` が立つ。完了すると準備済み台帳を
  受け取り新名になる。2 つの理由は別々の 1 行になる。
- `RENAME_HALTED` でも意図は捨てず、`folio_state_destroy` でも記録は残る（`:q!` と同じ）。
- `folio_state_document_name` は台帳の実名を返し、未完了のあいだも旧名を返す。
  `folio_state_rename_pending` は旧名と新名の組を返し、完了すると false になる。
- 起動は走査より先に復旧を呼び、`NONE` / 完了なら進み、PENDING・HALTED・記録不正・ロック無しでは
  走査を1回も行わずに理由付きで起動しない。

`tests/unit/command_tests.c` は登録表10件・`rename` の別名・名前引数・日本語表示名の一致を確かめる。
`tests/unit/allocation_tests.c` の state シナリオに改名を足し、確保失敗の経路を測定ビルドで通した。

固定toolchainで `cmake --build build`（clang-tidy込み）、`ctest --test-dir build --output-on-failure
--no-tests=error` が成功（2/2）。`python eng/coverage.py`・`python eng/conformance.py` の結果は本文末尾。

## 実アダプタの測定（ゲート外・`out/design/2026-09-16/rename_probe.c`）

専用の `data/`（probe exe の隣）を毎回作り直し、実ファイルで測った。結果は `rename-probe.log`。
**41項目すべて成功、失敗0、exit 0。**

- 履歴なし／履歴ありの改名が完了し、md・履歴ディレクトリ・`index.json` が新名になり `.rename.json` が消える。
  履歴の版番号と本文は変わらない。
- 各段階の直後で止めた fixture 4通り（記録だけ／履歴だけ移動済み／mdまで／`index.json`まで）から
  `recover_rename` だけで同じ最終状態に到達する。
- 移動先のmdがある／移動先の履歴ディレクトリだけ残っている、のどちらも `RENAME_NAME_TAKEN` で、
  `.rename.json` を作らず元mdも動かさない。大小文字だけ違う移動先もファイルシステムの照合で断る。
- 記録を公開した後（履歴だけ移った状態）に外部が移動先の md を作ると `RENAME_HALTED` で止まり、
  記録と元 md の両方が残り、次の起動も同じ理由で止まる。成功に丸めない。
- 記録が外から消えた状態の再開（`RENAME_RESUME`）は `RENAME_HALTED` で、新規開始へ落ちて
  `RENAME_NAME_TAKEN` になることはない。同じ意図は記録が戻れば同じ `RENAME_RESUME` で完了する。
- 別ファイルの識別子を書いた記録は `RENAME_HALTED` で停止し、記録を消さず何も動かさない。
  版2の記録は `RENAME_JOURNAL_BROKEN` で、やはり消さない。
- junction 経由のカテゴリは `RENAME_UNSUPPORTED` で、記録を公開しない。
- 同じ `data/` の二重起動は `PERSISTENCE_ADAPTER_DATA_IN_USE`。別プロセスが錠を持つあいだも同じで、
  そのプロセスが終われば取れる。錠ファイルは残ってよい。
- 錠ファイルを読み取り専用にした「書けない `data/`」は起動でき、改名は `RENAME_UNLOCKED` で記録を作らない。
  その状態で `.rename.json` があると `PERSISTENCE_ADAPTER_RECOVERY_LOCKED` で起動しない。

前段として `rename_api_probe.c`（`rename-api-probe` 相当）で `SetFileInformationByHandle(FileRenameInfo)` が
Win32パス・`\\?\`・`\??\` のいずれでも成功することを実測し、製品は既存の `\\?\` 組み立てをそのまま使っている。

## Windows部品の測定（ゲート外・`out/design/2026-09-16/rename_window_probe.c`）

タスク自身の主窓を画面外に作り、名前入力面はCBTフックで題を控えてから取消で閉じた。
結果は `rename-window-probe.log`。**19項目すべて成功、失敗0、exit 0。**

- 共通登録表が `FOLIO_COMMAND_RENAME` と日本語表示名を持つ（GUIの「操作」メニューはこの表から出る）。
- F2 は主窓の `HACCEL` で解釈され、題「名前を変更」の同じ入力面を開く。取消では何も改名しない。
- 本文が composition 中の F2 は奪わず、RichEdit / IME へ渡す。
- `:rename 新しい 名前` は面を開かずに同じ意図を実行し、md・履歴が新名へ移り、記録が消え、
  パンくずが新名になる。
- 記録を公開した後で止まった（HALTED）状態では、既存の 1 行の失敗表示が出て意図が 1 つ保持され、
  パンくずは実名のまま `recovering` が立つ。そこで F2 を押すと**最初から固定状態**で面が開き、
  名前欄は「旧名 → 新名」で入力不可、ボタンは「再試行」と「閉じる」になる。閉じても意図は残る。

## 限界（成功と扱わないもの）

- 電源断・OSクラッシュ時の名前空間更新の耐久性は未証明（ADR0022の根拠節のとおり）。
- 物理キーボードのF2、HHKB、日本語IMEの候補窓、マウスでの「操作」メニューのクリック、
  高DPI・両テーマでの目視、最小寸法での入力面は未確認。
- 記録公開後に止まった状態の**画面**（復旧待ちのパンくず・面の説明文・取消しても意図が残ること）は
  単体の値としては確認したが、実機の目視は未確認。
- ネットワークドライブ・ReFS・同期フォルダ上の `data/` は決定4のとおり対象外で、測っていない。
- 測定プログラムは計装なしの `clang-cl /clang:-std=c23 /MT /utf-8` で製品ソースを直接コンパイルしたもので、
  正規ゲートのASan/UBSan付きビルドとは別物である。
- **決定 4 / 5 の adapter 経路（reparse point・NTFS 照会・128bit 識別子・置換なし rename・段階の確定）は
  ゲート内の単体に無い。** ポートの文脈が不完全型なので偽ポートでは再現できず、実ファイルの probe だけが
  この経路を通る。ゲート内で守られているのは application 側の分岐と core の codec までである。
- `finish_rename` の「記録削除が `ERROR_FILE_NOT_FOUND` なら完了扱い」は、`write_note_ledger` と
  `DeleteFileW` の**あいだ**で外部が消す競合を作る必要があるため、probe で直接は踏んでいない。
  probe が測ったのは同じ所見の観測できる帰結（記録の無い `RENAME_RESUME` が `RENAME_HALTED` になり、
  新規開始の `RENAME_NAME_TAKEN` へ落ちないこと）である。

## 実行物

| 原本 | 結果 |
| --- | --- |
| `out/design/2026-09-16/rename_api_probe.c` | `FileRenameInfo` のパス形の実測（3形すべて成功） |
| `out/design/2026-09-16/rename_probe.c` | `out/design/2026-09-16/rename-probe.log`（41/41） |
| `out/design/2026-09-16/rename_window_probe.c` | `out/design/2026-09-16/rename-window-probe.log`（19/19） |
| 最終フルゲート | `out/design/2026-09-16/rename-fix-final-gate.log` |

最終HEADの `pwsh -NoProfile -File ./eng/check.ps1` の結果・前後のclean状態・CIは#56のPR本文へ記録する。
永続化スキーマは `categories.json` / `index.json` の版1と5版履歴を維持し、`data/.rename.json`（版1）と
`data/.nenefolio.lock` を加えた。依存・ゲートの変更なし。Waivers: none。

## 独立レビューの所見への対応（同じ単位）

レビューが統合を止める所見を 2 つ（＋追補 1 つ）出したので、同じ PR で直して測り直した。

1. 記録を公開した後の `MISMATCHED` / `UNSUPPORTED` / `IDENTITY_FAILED` / OOM で application が意図を捨て、
   パンくずが旧名を実ファイル名として示し、以後の保存が旧名を作り直していた。
   → `rename_outcome` を「公開前の拒否」「公開後の `RENAME_PENDING` / `RENAME_HALTED`」「完了」に分け、
   adapter は記録の公開を境に以後のどの失敗もその 2 値へ写す。application は 2 値のどちらでも意図を保持する。
2. 未完了中に F2 を押すと「名前変更の復旧待ち」が編集可能な名前欄に入り、その名前への改名が走り得た。
   → 名前欄は `folio_state_document_name`（台帳の実名）から取り、未完了なら最初から固定状態で開く。
   `pane_title_view` は実名と `recovering` だけを持ち、言い換えは UI のパンくず 1 か所が持つ。
3. 記録が外から消えると再開が新規開始に落ち、`NAME_TAKEN` で再起動まで全操作が止まっていた。
   → `rename_attempt`（START / RESUME）を加え、RESUME で記録が無ければ `RENAME_HALTED`。
   記録削除の `ERROR_FILE_NOT_FOUND` は完了扱いにする。
