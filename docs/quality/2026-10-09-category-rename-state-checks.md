# D2 #210 — カテゴリ改名applicationの対象確認

- Issue #210 / 親 #169 / FR-037 / ADR0041。D1統合main `c4d6bce`を基点としたD2 source `2abba61`。
- 規則: ARC-001/003/008/009/010/011、C-002/003/005/012/014、QLT-008/009/012/013、CNF-009/011。Waivers: none。
- D2はapplicationと既存表示値の追随まで。新コマンド・カテゴリ行入口・名前面はD3 #211で接続し、D2だけでFR-037を利用可能とはしない。

## 対象と実測

`folio_state_rename_category`は対象添字と検証済みカテゴリ名の借用入力を受け、FILTERED→範囲→共通同期→完全同名→ASCII-fold衝突→対象NAMED+EDITの既存保存→plan/空corpus準備→唯一のSTARTへ進む。
PENDING/HALTEDは同じplanと空corpusを保持し、共通adoptのCATEGORY分岐だけで台帳/空corpusを採用する。旧cacheは破棄し、新cacheの読込は既存の遅延経路を使う。

対象unitは次を固定した。

- NONE / NAMED VIEW / 対象UNTITLED / 別カテゴリVIEW・EDIT・UNTITLEDで保存/再読込を起こさない。対象UNTITLEDの初回保存は新カテゴリ名へ進む。
- 対象NAMED+EDITの変更本文は既存archive/writeで先保存。履歴または本文書込失敗で改名を開始せず、先保存後の改名拒否では保存/履歴closeを巻き戻さない。同じ本文は再保存しない。
- 完全同名no-op、ASCII大小だけ/既存名衝突、FILTERED/範囲拒否、先行NOTE意図の同期順。全11種のSTART結果とPENDING/HALTEDの単一所有/RESUME。
- カテゴリの順・色・展開、ノート台帳/順序、選択/カーソル/文書種別/mode/本文/RTFを保持。NONEを仮の復旧タイトルへ変えず、対象CATEGORYだけカテゴリ部を復旧表示にする。
- 保留後の非空filterは語/scroll/cache変更とreadの前に同期。失敗時は表示保持、完了後は全9ノートを新名で再読込し、旧カテゴリ名のreadが増えない。
- 対象historyは旧行破棄/readの前に同期し、未完了では元行を保持。純粋採用では行を保持し、次の読込だけ新名へ。別カテゴリhistoryは保留中でも読める。
- 保留後に作り直した現在preview（有効0件も含む）を完了時だけ破棄。別カテゴリpreviewは保持し適用できる。toggleと保存など別同期入口でも同じ採用となり、保存は新pathを使う。
- generic retryは新保存/選択/新名再構築をせず、NONE/UNTITLED/別文書から動く。既存NOTE採用後のcache OOMではplanが消えた表示値を再取得できる。
- ui_textの全ID/3言語列、改名/STALE/復旧の指定文言。既存Win32描画は新回復enumで計測と描画の同じgetterを使い、非空置換パターンでREADYでもpreview無しならSTALEを出す。

## コマンドと証跡

ログの正本は `D:/NeNeFolio/agents/210-rename-state/`。

| 入口 | 結果 | 証跡 |
| --- | --- | --- |
| CMake Debug configure | exit0、clang-cl 19.1.5 | configure.log |
| `cmake --build .../build --target folio_tests nenefolio_window` | exit0、C23/clang-tidy | build-target-4.log |
| 追加直接境界後 `cmake --build .../build --target folio_tests` / `folio_tests.exe --rename-d2` | exit0/PASS | build-target-5.log / unit-target-2.log |
| toolchain環境の `python .../allocation-target.py`（同じ製品core/applicationをmalloc/calloc/realloc注入＋ASanでコンパイル） | exit0/PASS。3 scenarioの失敗点20/24/26を全て1回ずつ注入し、次の無失敗で完了。CATEGORY RESUME採用はfail_at(1)でもREADY/read無し | allocation-target.log |
| `python eng/conformance.py` | exit0、0違反 | 実行出力 |
| rebase前backupとmain基点D2の `git diff ... -- src tests CMakeLists.txt` | 差分0。基点だけ置換 | rebase-main.log |

最終単一ゲートの結果は下に実行後追記する。成功済みsourceに工程だけを理由とする再実行は行わない。

ASanはD1の既存測定と同じ `interception_win: unhandled instruction ...` を非致命で出力した。終了0/対象PASSとは区別して記録する。
新UIの実Win32表示/名前面/Undo/focus/物理入力はD3の対象probeへ残す。D1 adapter/B/C削除のprobeは繰り返していない。ユーザーapp/dataは操作していない。

## ドキュメントとschema

ADR0041決定4の実署名を4引数に補正し、category_rename_targetの借用・所有・範囲検証を明記した。意味と順序は同じ。
表示/結果headerのNOTE・版1限定コメントを現契約へ揃えた。enum値や保存schema・ゲート/依存/除外は変更していない。

## 最終単一ゲート（2026-10-09 20:13 JST）

`pwsh -NoProfile -File D:/NeNeFolio/worktrees/210-rename-state/eng/check.ps1` はD1統合後source `2abba61`で1回実行しexit0。
CTest 2/2、conformance 0違反、symbols 2ライブラリ/0違反、中核分岐3210/3426 = **93.70%**、反例7.36%は90%未満として拒否、実ツールproofs **19本**が成功した。
ログは `D:/NeNeFolio/agents/210-rename-state/gate.log`、網羅率詳細は担当worktreeの `out/coverage/results.json`、proof詳細は `out/proofs/results.json`。
この品質記録の追記はproduction/test/CMakeを変えていないため、source不変の成功結果を再利用する。
