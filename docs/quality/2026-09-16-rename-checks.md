# 名前変更の確認（#56 / ADR0022）

2026-09-16。ゲート内の単体（偽ポート）、ゲート外の実アダプタ測定、ゲート外のWindows部品測定の3つに分けて記録する。
いずれも利用者の `data/` と起動中のアプリには触れていない。物理キー・IME候補窓・高DPIの目視は含まない。

## ゲート内の単体（偽ポート・決定性）

`tests/unit/state_tests.c` に改名の7本を追加した。

- 未選択は `NOTHING_SELECTED`、無題は `NAME_REQUIRED`（初回保存が先）。どちらも `data/` を触らない。
- 同じ名前は `READY` で改名を呼ばない。`ONE` のように大小文字だけ違う名前と、既存の他ノート名は `NAME_TAKEN`。
- 編集中の未保存本文は `archive` → `write`（旧名）→ `rename` の順で先に確定する。
  本文が書けなければ `NOTE_STORE_FAILED` で意図を確保しない。
- 台帳未同期（別名保存の `LEDGER_STALE` 後）は `LEDGER_UNSYNCED` で改名を呼ばず、修復後は同じ改名が通る。
- 記録公開前の5つの拒否（ロック無し・同名・非対応・識別子・記録書込）はそれぞれの理由を返し、
  意図を残さないので次の選択・トグルが再試行なしで通る。
- `RENAME_PENDING` のあいだ、保存・切替・トグル・カテゴリ移動・新規・終了確認がすべて先に同じ改名を再試行して
  `FOLIO_STATE_RENAME_PENDING` を返し、本文の書込も台帳の書込も行わない。別の名前の改名も始まらない。
  パンくずは `recovering` と「名前変更の復旧待ち」になる。完了すると準備済み台帳を受け取り新名になる。
- `folio_state_destroy`（`:q!` 相当）は公開済みの記録を捨てない。
- 起動は走査より先に復旧を呼び、`NONE` / 完了なら進み、PENDING・記録不正・不一致・ロック無しでは
  走査を1回も行わずに理由付きで起動しない。

`tests/unit/command_tests.c` は登録表10件・`rename` の別名・名前引数・日本語表示名の一致を確かめる。
`tests/unit/allocation_tests.c` の state シナリオに改名を足し、確保失敗の経路を測定ビルドで通した。

固定toolchainで `cmake --build build`（clang-tidy込み）、`ctest --test-dir build --output-on-failure
--no-tests=error` が成功（2/2）。`python eng/coverage.py`・`python eng/conformance.py` の結果は本文末尾。

## 実アダプタの測定（ゲート外・`out/design/2026-09-16/rename_probe.c`）

専用の `data/`（probe exe の隣）を毎回作り直し、実ファイルで測った。結果は `rename-probe.log`。
**35項目すべて成功、失敗0、exit 0。**

- 履歴なし／履歴ありの改名が完了し、md・履歴ディレクトリ・`index.json` が新名になり `.rename.json` が消える。
  履歴の版番号と本文は変わらない。
- 各段階の直後で止めた fixture 4通り（記録だけ／履歴だけ移動済み／mdまで／`index.json`まで）から
  `recover_rename` だけで同じ最終状態に到達する。
- 移動先のmdがある／移動先の履歴ディレクトリだけ残っている、のどちらも `RENAME_NAME_TAKEN` で、
  `.rename.json` を作らず元mdも動かさない。大小文字だけ違う移動先もファイルシステムの照合で断る。
- 別ファイルの識別子を書いた記録は `RENAME_MISMATCHED` で停止し、記録を消さず何も動かさない。
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
結果は `rename-window-probe.log`。**12項目すべて成功、失敗0、exit 0。**

- 共通登録表が `FOLIO_COMMAND_RENAME` と日本語表示名を持つ（GUIの「操作」メニューはこの表から出る）。
- F2 は主窓の `HACCEL` で解釈され、題「名前を変更」の同じ入力面を開く。取消では何も改名しない。
- 本文が composition 中の F2 は奪わず、RichEdit / IME へ渡す。
- `:rename 新しい 名前` は面を開かずに同じ意図を実行し、md・履歴が新名へ移り、記録が消え、
  パンくずが新名になる。

## 限界（成功と扱わないもの）

- 電源断・OSクラッシュ時の名前空間更新の耐久性は未証明（ADR0022の根拠節のとおり）。
- 物理キーボードのF2、HHKB、日本語IMEの候補窓、マウスでの「操作」メニューのクリック、
  高DPI・両テーマでの目視、最小寸法での入力面は未確認。
- 記録公開後に止まった状態の**画面**（復旧待ちのパンくず・面の説明文・取消しても意図が残ること）は
  単体の値としては確認したが、実機の目視は未確認。
- ネットワークドライブ・ReFS・同期フォルダ上の `data/` は決定4のとおり対象外で、測っていない。
- 測定プログラムは計装なしの `clang-cl /clang:-std=c23 /MT /utf-8` で製品ソースを直接コンパイルしたもので、
  正規ゲートのASan/UBSan付きビルドとは別物である。

## 実行物

| 原本 | 結果 |
| --- | --- |
| `out/design/2026-09-16/rename_api_probe.c` | `FileRenameInfo` のパス形の実測（3形すべて成功） |
| `out/design/2026-09-16/rename_probe.c` | `out/design/2026-09-16/rename-probe.log`（35/35） |
| `out/design/2026-09-16/rename_window_probe.c` | `out/design/2026-09-16/rename-window-probe.log`（12/12） |
| 最終フルゲート | `out/design/2026-09-16/rename-final-gate.log` |

最終HEADの `pwsh -NoProfile -File ./eng/check.ps1` の結果・前後のclean状態・CIは#56のPR本文へ記録する。
永続化スキーマは `categories.json` / `index.json` の版1と5版履歴を維持し、`data/.rename.json`（版1）と
`data/.nenefolio.lock` を加えた。依存・ゲートの変更なし。Waivers: none。
