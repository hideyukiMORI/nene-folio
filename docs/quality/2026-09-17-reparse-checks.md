# 改名の再解析ポイント判定の確認（#61 / ADR 0022 の 2026-09-17 の補正）

2026-09-17。`src/adapters/win32/persistence_adapter.c` の判定 1 か所を
「`FILE_ATTRIBUTE_REPARSE_POINT` があれば一律に拒否」から
「`GetFileInformationByHandleEx(FileAttributeTagInfo)` の `ReparseTag` が `IsReparseTagNameSurrogate`
（0x20000000 ビット）のときだけ拒否」へ変えた。関数名も意味に合わせて `plain_entry` → `not_link` にした。
呼び出し側（`identity_of` と `guard_parent`）は同じ 1 か所を見ており、第 2 の判定は作っていない。
親と対象を `FILE_FLAG_OPEN_REPARSE_POINT` で開く経路・NTFS 判定・128bit ファイル ID・置換なし rename・
記録と再開の契約は変えていない。照会そのものに失敗したときは安全側で拒否する。
利用者の `data/` と起動中のアプリには触れていない。

## ゲート内の単体（偽ポート）— この判定は通らない

**構造上の割り切り**。ゲート内の単体はポートを偽物へ差し替えるため、adapter の Win32 呼び出し
（`FileAttributeTagInfo` の照会・`FILE_FLAG_OPEN_REPARSE_POINT`・NTFS 照会・ファイル ID）は 1 本も通らない。
これは #56 の確認記録がすでに「限界」として記した範囲と同じで、今回も変わらない。
ゲート内で守られるのは application 側の分岐（`RENAME_UNSUPPORTED` の扱い）と core の codec までである。
実ファイルの probe だけがこの経路を通る。

## 実アダプタの測定（ゲート外・`out/design/2026-09-17/reparse_probe.c`）

probe exe の隣に専用の `data/` を毎回作り直し、実ファイルで測った。結果は
`out/design/2026-09-17/reparse-probe.log`。**17 項目すべて成功、失敗 0、skip 0、exit 0。**

| 区分 | 測ったこと | 結果 |
| --- | --- | --- |
| (a) 通常ファイル | 履歴なしのノートの改名が `RENAME_COMPLETED`。md と `index.json` が新名になり `.rename.json` が消える | 2/2 |
| (b1) junction | `mklink /J` で作った junction を親カテゴリにした改名が `RENAME_UNSUPPORTED`。元 md は残り、新名の md も `.rename.json` も作られない | 2/2 |
| (b2) シンボリックリンク | 対象の md をファイルのシンボリックリンクに置き換えると `RENAME_UNSUPPORTED`。リンクは残り、新名の md も `.rename.json` も作られない | 2/2 |
| (c) name surrogate でないタグ | `FSCTL_SET_REPARSE_POINT` と `REPARSE_GUID_DATA_BUFFER` で Microsoft 以外のタグ 0x00000042（surrogate ビット無し）を対象の md に付け、実際に `FILE_ATTRIBUTE_REPARSE_POINT` が立つことを確かめてから改名すると `RENAME_COMPLETED`。md は新名へ移り `index.json` も追随する | 3/3 |
| 判定表 | `IsReparseTagNameSurrogate` が 0xA000000C（symlink）と 0xA0000003（mount point）で真、0x9000001A / 0x9000101A / 0x9000901A（クラウド系）・0x80000017（WOF）・0x80000013（重複除去）・0x00000042 で偽 | 8/8 |

(c) は実 OneDrive / Dropbox に依存しない。この環境ではシンボリックリンクも junction も
`FSCTL_SET_REPARSE_POINT` も作れたので、代替の記録（判定表だけ）には落ちていない。

### 負の証明（変更前のコードで同じ probe が落ちること）

同じ probe を **変更前**の `persistence_adapter.c`（`git show HEAD:...`。ADR の補正だけを含む
6adc096 時点）へリンクして走らせると、(c) の 2 項目だけが落ちて **15/17・exit 1** になる
（`out/design/2026-09-17/reparse-probe-before.log`）。(a)(b1)(b2) と判定表は変更前でも通る。
つまりこの probe は「クラウドのプレースホルダ相当が通るようになったこと」だけを新たに測っており、
リンクの拒否は前後で同じである。

### 実行物

| 原本 | SHA-256 | 結果 |
| --- | --- | --- |
| `out/design/2026-09-17/reparse_probe.c` | `e07b8f87d5944d21f1c56654bab5562e86b17be4e19ed227aa6c72f9bd59fc98` | `out/design/2026-09-17/reparse-probe.log`（17/17・exit 0） |
| `out/design/2026-09-17/probe/reparse_probe.exe` | `dfa5330e4e44ff0e486fcae2ca793a27da92fd935214181fedca70dbc2603d64` | 同上 |
| 変更前のコードに同じ probe をリンクしたもの | — | `out/design/2026-09-17/reparse-probe-before.log`（15/17・exit 1） |
| 最終フルゲート | — | `out/design/2026-09-17/reparse-final-gate.log` |

probe は計装なしの `clang-cl /clang:-std=c23 /MT /utf-8` で製品ソースを直接コンパイルしたもので、
正規ゲートの ASan / UBSan 付きビルドとは別物である。

## 狭い検査と最終フルゲート

固定 toolchain（`. ./eng/toolchain.ps1`）で `cmake --build build`（clang-tidy 込み）、
`ctest --test-dir build --output-on-failure --no-tests=error`（2/2）、`python eng/conformance.py`
（0 violation）が成功した。最終 HEAD の `pwsh -NoProfile -File ./eng/check.ps1` の結果・前後の clean 状態・
CI は #61 の PR 本文へ記録する。

## 限界（成功と扱わないもの）

- **実際の OneDrive / Dropbox の同期フォルダに置いた `data/` では測っていない。**
  測ったのは「name surrogate でないタグの reparse point が通ること」までで、
  実サービスのプレースホルダの取得（オンラインのみファイルを開いたときの同期待ち）は含まない。
- 同期サービスの競合コピー（両側で編集したときに `... (競合コピー)` 等が増える）は、
  エディタを問わず起きる事象として残る。改名の途中で同期が走ったときの振る舞いは未測定。
- ネットワークドライブ・ReFS は決定 4 のとおり対象外で、測っていない。
- 0x00000042 は測定のために自分で付けた Microsoft 以外のタグであり、実在のクラウドタグそのものではない。
  クラウドタグ（0x9000001A 系）の分類は判定表で確かめているが、実タグの付いた実ファイルでは測っていない。
- 画面の目視（同期フォルダに置いた `data/` で F2 から改名する実機確認）は未確認。
- 電源断・OS クラッシュ時の名前空間更新の耐久性は #56 と同じく未証明。
