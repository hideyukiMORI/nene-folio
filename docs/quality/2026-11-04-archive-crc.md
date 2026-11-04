# ZIP生成のCRC計算費用 — 2026-11-04（#605）

## 判断と変更

実note_archive_createのRelease診断で、合成64MiBの同期生成が中央値225.68755msだった。
唯一のprivate CRC関数で1byteを2回のnibble参照で処理しているため、同じIEEE CRC32の
256-entry表による1byte参照へ置換した。ADR0082へ製品編集前に判断を補足した。
表のpolynomial 0xedb88320・初期UINT32_MAX・最後の反転・unsigned byte解釈は維持する。
immutable表は64→1024bytes（960bytes増）。公開API/owner/確保回数/依存/ISA/上限/版/schemaは不変。
ZIP32 STORE、UTF-8名、固定1980-01-01、原子的な新規公開も既存経路を維持する。

Issue: #605。規則: ARC-001/003/007/012、C-008/012/016、QLT-008/010/012。
製品変更はsrc/core/note_archive.c、unitはtests/unit/transfer_tests.c。
通常の保存形式・画面・Store提出物はこの対象検証で操作していない。

## 対象と結果

- 固定toolchainのDebug/tidyでfolio_testsをbuildし、既存`folio_tests.exe --images`が成功。
  このselectorはtransferも含むため、別の--transfer実行を重複させていない。
- 標準123456789 vectorを保持し、公開archive入口の全256singlebyteと14境界
  （0/2/3/7/8/15/16/17/255/256/257/4095/4096/4097bytes）でlocal/centralのCRCをtable-free referenceと照合。
  NUL/上位byteを含むpayloadの不変も確認。通常Debugのallocation injectionはinactiveで、全確保失敗を走破したとは扱わない。
- 表の生成は同polyの8step recurrence。全256entryから得たsinglebyte CRCをPython zlibと照合し、
  最終製品の表が検証済み全entryと一致することも監査した。製品へ生成器/第二CRC経路は追加していない。
- `clang-format --dry-run --Werror`（製品1/unit1）、`eng/conformance.py --build-dir build`は0違反、
  `eng/symbols.py --build-dir build --require core application`は2lib/0違反、`git diff --check`成功。
- 正規Release coreと既存scale.cで実note_archive_createを計測。全8ZIPが旧版と全bytes一致。
  Python zipfile/zlibでも全member bytes/CRC/順序/STORE/UTF-8 flag/固定日時を照合した。
  raw/ZIP24fileとrun artifactを再hash。子/記録側は全0、stderr/recordingErrorsは空。

## Release計測

Windows x64、eng/toolchain.ps1の固定clang-cl、Release core、/MT /O2 /DNDEBUGの同じ外部計測exe。
payloadはi%251の決定的binary、最大8MiBずつ借用した1/1/4/8assetと9byteのCRLF本文。
QPC境界はnote_archive_createの開始/終了で、確保・CRC・コピー・ヘッダ生成を含む。
fixture準備、破棄、ディスク保存、画像復号、ファイル公開、実UIは含めない。
各5sampleの初回を別記し、後4sampleの中央値を使用。旧coreのsourceはc83a3ca、
#603はapplicationだけなので関係core/eng/CMake入力不変を確認し、旧library/exe/証跡hashを照合して再利用。

| 合成payload | 先行した旧版の中央値ms | 候補の中央値ms |
| --- | ---: | ---: |
| 1MiB | 3.48840 | 2.06025 |
| 8MiB | 28.63290 | 15.55710 |
| 32MiB | 111.42085 | 61.99480 |
| 64MiB | 225.68755 | 123.39725 |

64MiBは旧→候補→候補→旧の順を追加し、時点差だけで採否を決めない。

| 順序 | 版 | 後4sample中央値ms |
| --- | --- | ---: |
| 1 | old | 221.94285 |
| 2 | current | 124.49360 |
| 3 | current | 123.44080 |
| 4 | old | 223.58690 |

process中央値の中央値は旧222.764875→候補123.9672ms、44.35065%短縮。
対象条件で互換性と費用削減が成立したため採用する。OS負荷/周波数等は分離しておらず、
全入力/全機種/書き出し操作全体の倍率やUI応答期限へ一般化しない。CIへ時間閾値を導入しない。

## 全試行・再利用・限界

原本は`D:/NeNeFolio/agents/605-archive-crc`、実行先は同名outputs/worktree。
先行診断の原本は`D:/NeNeFolio/agents/archive-generation-scale-20261104`。
旧ZIP初期4条件とCRC-only20sampleは各decision/observation/crc-observationへ保持する。
CRC-onlyの改善は完成ZIP性能とは区別し、今回は正規coreの実生成で評価した。

実行recordはbuild-green-01、green-01、build-release-01、verify-01、link-scale-01、scale-01、audit-01。
入力・artifact snapshot、raw stdout/stderr、子と記録側の終了値を各新run-dirへ保存。
編集中の最初の実装から製品入力は変わっておらず、push/統合だけで成功済み検証を繰り返さない。

準備時の失敗も残す。GitHub連携Issue作成は403でghへ切替え、#605作成成功。
prepare-checks初回はartifact fieldをpathと誤認しKeyError。元scriptを-01へ保全し、sourceへ訂正、
既生成の同一link script/inputsを照合して再利用後に完了した。製品検証の失敗とは数えない。
規約まとめ読取の出力切詰めは分割読取で補完、推測pathの不存在も全体journalに記録した。

全heap/任意入力/実UIの網羅を主張しない。独立レビューはこの局所変更では追加せず、現在のサナが実装・検証・自己レビュー。
追加waiverはnone、既存WVR-0001はWinHTTP callbackだけで不変。
最終head必須CIのeng/check.ps1成功を統合条件とし、確定head/CI/main/通常反映/data照合/全保全整理は
`D:/NeNeFolio/agents/605-archive-crc/integration-completion.json`へ収載する。
