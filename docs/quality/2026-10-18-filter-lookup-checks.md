# #328 全ノート絞り込みの本文宛先探索

Issue: [#328](https://github.com/hideyukiMORI/nene-folio/issues/328)。基点main: `9db5d2bb26919d324b612a3762f60380ff8b24a0`。
契約: FR-032、ADR0024（保存済み本文の写し修復を含む）。
規則: ARC-001/003/004/007/008/010/012、C-002/003/005/007/008/012/013/014/016、QLT-008/009/010/012/013、GIT-001〜004。

## 根拠と設計

`folio_state`の`collect_entries`は台帳順に全ノートを列挙し、各件で`note_corpus_body`を引く。
基点の`note_corpus`はカテゴリ名・ノート名を先頭から線形走査するため、全N件が載っていると1回の組立にN(N+1)/2回の宛先比較を要する。
[旧計測](2026-10-15-external-review-performance.md)の1万本entry組立中央値1453msは焦点を選ぶ根拠であり、今回の基点との回帰比較には使わない。

既存の3所有配列をカテゴリ名→ノート名の`strcmp`順で厳密昇順に保ち、同じ位置の本文との対応を維持する。
宛先は二分探索で引く。追加・削除・改名・移動の成功時だけ必要な区間を詰め、名前変更でも本文オブジェクトを移し替えたり複製したりしない。
追加に必要な確保は配列移動前に終える。改名/移動の宛先残留の除去と元写しの名前複製失敗時の扱いは既存どおりとする。

台帳順はapplication、一致判定と一致集合は`index_filter`が引き続き所有する。本文の遅延読込と保存後の修復も既存経路を使う。
公開API、所有者、検索意味、保存schema、依存、ゲートは変更しない。承認済み型の内部詳細なので新しいADRは作らない。
hash索引や永続entry cacheは、衝突/確保/別同期状態を増やすため今回採用しない。

改善対象はwarm組立の宛先比較をO(N²)からO(N log N)へ減らすこと。
単一更新の配列移動はO(N)、任意順の初回追加には最悪O(N²)の移動量が残る。本文の部分一致アルゴリズムも変更しない。

## 受理条件と検証範囲

- 公開操作から、順不同入力、同名別カテゴリ、prefix/case/UTF-8の宛先、欠落、削除/再追加、左右への改名/移動、確保失敗後の状態を確認する。
- 既存filter/application/cache retryの対象試験で、結果・台帳順・遅延読込・保存済み本文・現在文書の保持を確認する。
- 正典toolchainの対象build/tidy、対象単体/確保失敗試験、format/conformance/diffを完成時にまとめる。
- 現mainと完成候補を同じfixture/ビルド条件で比較し、1万本のentry組立中央値を80%以上短縮する。
  1千本/1万本、warmup 1回＋30回の中央値/p95/最大を保存する。warm全検索のz/alpha/aaaaabも個別に比較する。
- 計時中にcompiler等を同時実行しない。source/lib/fixture/toolchainを識別し、初回本文取得はwarmとは別に扱う。
  cold OS cache、Release、実データ分布、UI全体の応答を測ったとは主張しない。

作業木: D:/NeNeFolio/worktrees/328-filter-lookup。
恒久証跡: D:/NeNeFolio/agents/328-filter-lookup/。仮出力: D:/NeNeFolio/outputs/328-filter-lookup/。

## 対象実装と検証

製品差分は`src/core/note_corpus.c`のみ。対象SHA256は
`403b04cafb20f68b536982923ed3497bfe9dbedd014e1fbafdba37a0de6671ab`。
`filter_tests.c`と`allocation_tests.c`に不足する公開操作の境界を加え、既存のapplication試験を対象実行へまとめた。
`state_tests.c`・`folio_tests.c`・`unit_tests.h`の追加は対象選択と既存試験の呼出しで、別の製品入口を作らない。

| コマンド | 結果 |
| --- | --- |
| `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | 成功。正典eng/toolchain.ps1、clang-cl 19.1.5 |
| `cmake --build build --target folio_tests --parallel 2` | 成功。64工程、正典clang-tidyとASan/UBSan/nullability付き |
| `build/folio_tests.exe --filter-lookup` | 成功。core/filter/BOMと保存・新規・改名・移動・削除・カテゴリ改名・写し修復の既存境界 |
| 外部`target-allocation.py` attempt 2 | 成功。正典の測定設定で38個のcore/application単位を計装し、通常/確保失敗の対象引数を実行 |
| `clang-format --dry-run --Werror --style=file`（変更したC/header 6件） | 成功 |
| `python eng/conformance.py --build-dir build` | 0 violations |
| `python eng/symbols.py --build-dir build --require core application` | 2 libraries、0 violations |
| `git diff --check` | 成功 |

確保失敗は新規growth 7点、既存置換2点、左改名2点、既存宛先への右改名2点、移動2点を各1回失敗させ、同じcorpusで保持と再試行を確認した。
既存の保存後修復6点と、同本文保存のloaded/unloadedの確保回数6/6も確認した。
対象corpusの測定は59/60分岐、16/16関数、186/189行。これは対象測定であり、最終CIのQLT-009全体結果の代用ではない。
実コマンド・終了コード・出力は恒久証跡の`implementation-report.json`、成功試行は`allocation-attempt-2/`を参照する。

独立SOL `filter_review`が同じ製品hashを読み取り専用で確認し、blocker/修正要求なし。
ゼロ件、左右への移動、宛先破棄後の元位置変化、確保失敗、本文寿命、台帳順と呼出し側同期を確認した。
レビュー担当は試験やビルドを重複実行していない。詳細は恒久証跡の`review.md`。

## 失敗した計測器設定と制約

外部確保失敗試験の初回は通常DebugのUBSan/nullabilityを残したため、既存`note_text.c:46`で
失敗注入のNULLを`_Nonnull`メンバーへ渡した時点で停止した（exit 1）。
外部計測器を正典`eng/coverage.py`と同じASanのみの確保失敗設定へ揃え、attempt 2で成功した。
製品・ゲート・通常Debugのsanitizer設定は変更していない。初回の設定、実行物、ログも保持する。
成功したASan測定には既存の`interception_win: unhandled instruction`起動診断があるため、stderrを収載する。

## 現在の到達点

対象実装・対象検証・独立レビューは完了。性能比較は準備中で、80%短縮条件はまだ未確認。
最終PR headの必須CI、main/通常exe反映、専用作業物の証跡収載と整理も未完了。
成功済み結果は関係入力が不変なら工程間で再利用する。Waivers: none。
