# 外部画像の使用済み連番を再探索しない — #603

2026-11-04。ARC-001/002/003/004/010/012、C-002/003/005/008/011/012/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0085。

## 問題・正典・設計

外部画像ごとに番号1から候補を組み立て、候補ごとに`archive_assets_contains`で既存素材を線形走査していた。
同じカテゴリ/拡張子の画像が増えると、既に使用済みの範囲を毎回再探索する。画像I/OやZIP生成を含めない初期Release計測でも512件で約1.65秒になった。
Issue作成/最新main c83a3caからのD側branch/ADR0085の補足を先に行い、`src/application/image_export.c`だけを修正した。

既存`image_export`が素材と同じindexへ生成番号を所有し、containedは0とする。最後の同カテゴリ/拡張子の採用番号+1から既存候補探索を継続する。
ASCII大小同一視は既存`ascii_fold_byte`、拡張子は既存identity由来の16byte境界/`.img`fallbackを維持。
番号kを採用するまでに1〜kが使用済みとなり、export中に削除されないため後で空かない。同groupの番号は単調なので後ろからの最初の一致でよい。
途中のcontained占有は同じqueryが検出する。準備した番号はcount未満へ入るまで探索されず、archive追加成功後の既存count更新で採用する。
失敗slotは次の探索へ影響しない。contained候補もslotを0へ戻す。

size_t4096個（x64で32KiB）がownerに増える。確保回数/公開API/owner数/参照上限/同identityのbytes検査/親子衝突規則/原文とコピー/schema/依存/gateは不変。
既存素材のlookupとarchive衝突検査は線形のまま。全処理が線形時間になったとは主張しない。

## 対象検証と互換性

`tests/unit/image_tests.c`に20混在入力を追加。containedの穴/後から入る占有、複数カテゴリ、ASCII大小同一視と非ASCIIの区別、
複数拡張子/16byte境界/fallback/拡張子のない名前/ドットを含むdirectory、同identity再利用を確認する。
追加の3呼出しでは同identity異bytes拒否と既存bytes保持、次の素材が番号2になることを確認。既存の親子衝突と確保失敗契約も選択範囲に残す。
`folio_tests.exe --images`は画像/Markdown/link/transfer/UI textの既存selectorであり、今回と無関係な全ゲーム/全UI等は実行しない。
通常buildのallocation injection inactive表示は保持し、このrunで確保失敗が注入されたとは報告しない。

実nativeでは隔離D側に同じ有効1pixel PNGを9fileとして生成し、12notes/2カテゴリとcase alias、PNG/JPG表記/noextension/長いextensionを通した。
実`image_adapter_port`→image_export→note_archiveを旧/候補へ各1回。実ファイル読取・asset検査・ZIPを通し、非表示のまま完了。
12原文/コピーの独立期待、23ZIP member全bytes/CRC、9原画像SHAを照合した。両ZIPは3726bytes、SHA256 `fbe6c43f6b859892ba7960ff6c76cc8cda09a3a3cab56838f4877b7ddb6c10e5`で完全一致。
PNG bytesを.jpgや拡張子無しで置く場合も、名前のsuffixは内容種別と独立で既存identityに従うことを確認した。

別Release診断は4096外部素材の全番号/path/bytesを確認し、次の1参照を追加するとport呼出し/素材数を増やさず拒否した。
新しいownerへの4097参照一括投入もport0/asset0で拒否し、どちらも原文不変。4096コピー138157bytesはPython独立期待と一致。
旧版の4096連番生成は実行しない。上限の期待は既存契約と独立構築に基づき、旧版全入力との同値証明ではない。

## Release計測

初期診断と同じ外部scale.c、決定的な1pixel PNGを返すasset port、同じ正規Release coreを使う。
旧applicationは#589保全、coreは#597保全のarchive全SHA/entry SHAと製品入力不変を照合済み。
候補applicationは固定toolchain/正規CMake Release targetで新規build（coreもCMakeの依存としてbuildされたが比較linkは同じ保全coreを使用）。
Windows x64、clang-clのリポジトリ固定toolchain。生成/取得は計測外、時計は外部helperのQueryPerformanceCounter。
測るのはimage_export_appendだけ。実fileI/O/WIC/ZIP生成/UI/周波数や電力管理を含めた測定ではない。
各process5sample、初回を別に保存して後4件の中央値を使う。成功した初期8条件をred根拠として再利用した。

| 件数 | 外部・初期旧→候補 ms | contained・初期旧→候補 ms |
| --- | --- | --- |
| 64 | 4.4803 → 0.4458 | 0.5224 → 0.28675 |
| 128 | 27.07055 → 1.34675 | 1.2421 → 0.72775 |
| 256 | 207.0133 → 4.6669 | 3.62875 → 2.1052 |
| 512 | 1648.1433 → 16.56495 | 12.1576 → 6.9178 |

全8条件のasset path/bytes/count・原文不変と、全コピーbytesが旧/候補/独立期待で一致した。
contained対照も約43%変動したため、初期時間比だけで効果を断定せず、512件だけ旧→新→新→旧の順に追加した（8process×5sample）。

| 条件 | 旧2processの中央値 ms | 候補2processの中央値 ms | process中央値同士の中央値・旧→候補 |
| --- | --- | --- | --- |
| 外部 | 1128.6131 / 1136.596 | 23.2856 / 18.16735 | 1132.60455 → 20.726475 ms、98.17%減 |
| contained | 6.342 / 6.81765 | 7.0461 / 7.0204 | 6.579825 → 7.03325 ms、6.89%増 |

新版の再比較は18.17〜23.29msで、初期候補の16.56msと区別する。対照の小増加と32KiB増を隠さない。
各sample/範囲はscale-observation.jsonとpaired-observation.jsonに保持。OS負荷/CPU周波数等は未分離で単一原因・全入力の速度・実UI遅延の保証をしない。
追加の性能最適化や時間のCI閾値は導入しない。

## 実行記録・失敗・制限

原本は`D:/NeNeFolio/agents/603-export-name-search`、初期は`D:/NeNeFolio/agents/image-export-scale-20261104`。
対象runは各同名outputsに`tools/record-run.py`の実行前source/header/lib/exe snapshot、raw stdout/stderr、終了値、指定artifactを保存する。

| run/コマンド | 結果 |
| --- | --- |
| build-green-01: 固定Debug CMake、folio_tests + nenefolio_adapters | 成功、clang-tidy含む |
| green-01: build/folio_tests.exe --images | 成功、追加契約と既存関連境界 |
| build-release-01: 正規Release nenefolio_application | 成功 |
| link-native-old/current-01 + native-01 | 成功、実画像2ZIP完全一致/独立照合 |
| link-scale-01 + scale-01 | 成功、8条件×5sampleと全コピーbytes |
| paired-01 | 成功、512件8process×5sample、全コピーbytes一致 |
| link-boundary-01 + boundary-01 | 成功、4096/4097/次参照の境界 |
| verify-01: clang-format --dry-run --Werror、conformance、symbols、git diff --check | 成功、conformance0、2lib symbols0 |
| audit-01 | 成功、前12runの子/記録0・stderr/記録エラー空、artifact99file再hash、138157byte独立コピー照合 |

準備時にasset.hをhex列と誤認してSHA照合が失敗。元scriptを保存し、実際のdecimal列へ訂正して同じ既知SHAを確認した。
その訂正をinline shellへ渡した1回はPowerShell構文エラーで未実行。patchへ切り替えて訂正。存在しないADR名の静かな読取とrgのWindows glob誤指定もjournalへ分離した。
これらを製品検証成功へ算入しない。製品build/test/nativeの失敗やgate緩和はない。

有限入力/同一ホストの確認で、任意カテゴリ/全Unicode/全heap/任意I/O競合/全Windows/実UI/一般性能を保証しない。
実ノート/実app/Storeを試験に使用しない。既存WVR-0001はWinHTTP callbackだけ、追加waiverなし。
最終head必須CIと正典merge後、通常exeの正規反映/data内容・作成更新日時照合、3root全保全/hash照合と安全確認を経て整理する。
branch/commitは保持し、確定原本はintegration-completion.json。成功入力が不変の対象検証はpush/mergeで再実行しない。
