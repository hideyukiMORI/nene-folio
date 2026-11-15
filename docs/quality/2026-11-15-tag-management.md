# タグ全体管理 — #821

2026-11-15。hideの停止まで継続する実装目標に基づくロードマップ段階2b。
先行設計は[ADR0093](../adr/0093-global-tag-management.md)。保存・準備・逐次適用の対象確認まで実施。
管理画面と実Win32保存境界の対象確認を進め、最終CIは未完了である。

## 調査と設計

- #819はPR820最終head `e79cefeca87c9df125c045c7683c00e2358332de`、必須CI43676165610成功で
  main `11080b0c75cf75f85ccb6de4006af8926fb4d730`へ統合した。50 CTest、分岐90.9666731631261%。
  通常exe反映と34実dataファイルの不変照合が完了。確定原本はD:/NeNeFolio/agents/tags-20261115/completion.json。
  同作業木の全ファイルとGit登録を整理したが、OSが最後の空directory削除を拒否したため
  D:/NeNeFolio/worktrees/819-note-tagsだけ0ファイルで保持する。outputsは整理済み。全試行は同HANDOFF.md。
- #821は上記mainからD:/NeNeFolio/worktrees/821-tag-management、branch `feat/821-tag-management`を作成。
  試行はD:/NeNeFolio/outputs/821-tag-management、恒久記録はD:/NeNeFolio/agents/tag-management-20261115。
- 背景Claudeの読取専用`design:design-critique`は終了0。相談原稿・raw stream・応答を恒久記録へ保存。
  プレビュー兼確認、新しい走査による再試行、段階的な面、付与面に管理入口を混ぜない案を採用した。
  画面を操作した検証ではない。
- 独立レビューは読取専用で4点を指摘。比較/履歴/公開の単一入口、管理入口保存の期待値照合、
  公開済みと後処理失敗の分離、選択native本文の即時採用を先行ADRへ取り込んだ。編集・build/testは委任していない。
- Win32のCreateFileW/ReplaceFileW公式仕様を照合し、最終照合から公開までをCASと呼ばない契約を採用。
  POSIX renameやTxFは採用していない。
- 調査中に推測したADR/file名が存在しない読取、PowerShellに渡したglobの不成立があった。
  `rg --files`で実在を確認して読み直した。製品変更や検証実行は発生していない。

## 検証

全試行は上記outputsの`run.json`、入力snapshot、stdout/stderr、`result.json`に保存する。
下記の成功はchild/wrapperがともに0、recordingErrorsが空であることを確認した。

| 試行 | 結果と判断 |
| --- | --- |
| configure-01 / core-build-01 | configureと`cmake --build build --target folio_tests nenefolio_adapters --parallel 2`成功 |
| core-unit-01 / tags-regression-01 | `folio_tests --tag-management`と`--tags`成功。後者stderrは既存の比較補助が不一致候補を列挙する出力で、assert失敗ではない |
| batch-build-01 | 失敗。fake readとcorpus準備がC-012複雑度を超えた。本文解決と一件準備を分離 |
| batch-build-02 | 失敗。corpus準備の複雑度超過が残った。停止時の結果処理を集約し、catalogの重複失敗設定を除去 |
| batch-build-03 / batch-unit-01 | build成功、対象実行失敗。選択中ノートの適用境界のassertで停止 |
| batch-build-04 / batch-unit-02 | 診断assertを分けてbuild成功。実行はプレビューから適用へ移れず失敗。既存検索結果の再構築がUI行世代を進め、対象を誤って失効させていた |
| batch-build-05 / batch-unit-03 | 台帳同一性の世代を表示行世代から区別し、成功。改名/統合/解除、外部変更/削除、履歴/公開失敗、中止/再走査、選択EDITの即時採用、無題と条件保持を確認 |
| batch-build-06 | 境界試験のmalloc結果に対する静的解析で失敗。明示したnull分岐を追加 |
| batch-build-07 / batch-unit-04 | 成功。32タグの置換、64 KiBヘッダーの更新後再検証、16 MiB本文上限を追加確認 |
| measurement-build-01 / measurement-unit-01 | 正典`eng.coverage.build_measurement`、Debugのaddress/undefined/nullabilityとno-recover、正典link options、nullability/確保interception証明を再利用して構築。`--tag-management`だけ実行し成功。原文更新52、準備154、適用43の確保境界を走査 |
| ui-build-01 | 失敗。管理面を接続した8関数の複雑度/引数数が規約値を超えた。描画・ラベル・入力準備・Tab位置・UIA通知と操作を責務で分割 |
| ui-build-02 | 失敗。UIA surface_actionの複雑度11が残り、一覧操作への分岐を分離 |
| ui-conformance-01 | 失敗。通常configureにはCMake File API返信がなく、`--build-dir build`指定のARC-002検査が実行条件不足。製品違反や成功とは扱わない。ソース検査と最終CIの全検査を区別する |
| ui-unit-01初回呼出し | 入力exeを相対pathで渡したためrecord-runの事前検査が拒否。子プロセスは未実行、結果directoryなし。絶対pathへ修正して同名で実行する |
| ui-build-03 / ui-conformance-02 | build成功。`eng/conformance.py --root .`のソース検査は0違反。前記File API未生成の全architecture検査を成功へ読み替えない |
| ui-unit-01 | 対象実行は失敗。core/applicationの試験後、ui_text期待表の終端が旧IDのままで全ID数assertに失敗。文言自体の欠落ではなく、終端を新しい最後のIDへ更新 |
| native-management-01 | 実Win32で89チェック成功。改名/統合、UIA単一選択、行Enterの非実行、1 idle最大1保存、選択EDITへの即時採用/Undo基準更新、解除/中止/再走査を確認 |
| ui-unit-02 / commands-unit-01 | 成功。修正した全ui_text期待表とタグ管理のcommand/menu接続を確認 |
| ui-build-04 / measurement-build-02 / measurement-unit-02 | 成功。新しい走査による統合判定を追加確認し、確保注入は原文更新52・準備154・適用43件を再確認 |
| native-management-02 | 失敗。外部変更拒否をモーダル表示としたfixtureの誤認。既存契約のinline noticeを確認するよう修正し、本文/Undo/選択/履歴不変の条件は維持 |
| management-visual-01 | 687チェック、3言語×明暗/高コントラスト×4段階の36画像を保存。新名入力段に不要な「一致なし」、中止結果の未処理行に「変更予定」が残ることを確認し、入力段の候補描画を除き、結果専用の「未処理」文言を追加 |
| ui-build-05 / ui-unit-03 | 成功。上記表示と3言語の期待表を確認 |
| native-management-03 / native-build-06 | nativeは新規ノートにSelection actionを送ったfixture誤りで失敗。Invokeへ修正しbuild成功。外側の時計とphase通知を使う計測driverを追加 |
| native-management-04 / native-build-07 | nativeは終了処理の非同期dispatchを待たずに取消結果を検査して失敗。既存pump_refused_closeを使って待機し、build成功 |
| native-management-05 | 288チェック成功。clean/dirty EDIT入口の外部変更拒否、プレビュー後競合、実ファイルの読取lock/不正UTF-8/削除/履歴lock/公開lock/再試行、無題を保つ終了取消と残り処理の停止を追加確認 |
| management-visual-02 | 701チェック、上記36画像と再描画診断2画像を保存。入力段と未処理表示の修正を確認。見かけ上のprefix欠落は、元画像と再描画画像の全画素比較で差分なし。独立した読取レビューでも全文表示を確認し、製品描画を変更しなかった |
| management-measure-01 | `python tools/measure-tag-management.py <tags_native_tests.exe>`成功。下記の7回計測を保存 |
| native-build-08 / native-management-06 | build成功。実EM_STREAMINをfixture側で拒否し、公開1件・未処理1件で停止することは確認。その後「保存ボタンが無効」というfixtureの誤認で失敗。既存UIは保存操作に失敗理由を返すため、実Saveの拒否と履歴/本文不変を確認する条件へ修正 |
| native-build-09 / native-management-07 | build成功。native投入失敗後のSave拒否・履歴不変・既存EDIT操作による復旧は成功。後続試験でfixtureがEDITのまま再利用されfocusの初期条件が違い失敗。復旧確認後に通常のVIEWへ戻してから次の窓を作るよう修正 |
| native-build-10 / native-management-08 | 成功、348チェック。実native投入失敗でも公開済み1件・未処理1件で止まり、Saveの拒否、本文/履歴不変、再投入後の新しいUndo基準まで確認。製品側へ試験フックを追加していない |
| native-tags-regression-01 | 成功。単票タグ編集/Undo/VIEW保存・条件操作の既存境界を再確認 |
| native-history-regression-01 | 失敗。exeをbuild直下と誤指定し、入力snapshotが不存在を拒否。子未実行、125。実在するverification-imagesへ修正 |
| native-history-regression-02 / native-locked-regression-01 / native-filter-save-regression-01 / native-save-as-regression-01 | 全て成功。共通保存の変更に直接関係する履歴採用と保存、lock失敗、絞り込み前の保存、別名保存を確認 |
| table-proof-01 | 成功。登録したTAG_APPLY表の現物をDの専用fixtureへ複写し、正例→LIMIT行削除をCNF-009が拒否→bytes完全復元の正例を確認。製品ソースやゲートを緩めていない |
| final-conformance-01 / final-format-01 / final-diff-01 | 全て成功。`python -B eng/conformance.py --root .`は0違反、変更C/headerの`clang-format --dry-run --Werror`と`git diff --check`が成功。全architecture/カバレッジを含む最終CIの代替ではない |
| CI43685362831 / merge-01 | 最初のhead `9bdfea46729524452a36ab74aea7964147f7cc99`は51 CTest成功。その後の確保注入測定で既存タグ候補の「失敗後に自動再試行しない」assertが失敗し、必須CIと正典mergeは失敗、未統合。catalog入力配列の確保失敗がPREPARINGのまま残る実装欠陥であり、全体管理対応の準備処理整理で失敗状態の設定を落としていた。全ての早期失敗もFAILEDとなるようcatalog生成入口で終了状態を設定し、成功時だけAVAILABLEへ採用する。期待値/ゲートは変更しない |
| catalog-fix-build-01 / measurement-build-03 / measurement-tags-03 / measurement-management-03 | 全て成功。修正したapplicationの通常build/静的解析と、正典sanitizer/確保interceptionによる`--tags`、`--tag-management`を実行。CIで失敗した候補確保失敗と自動再試行の禁止が成功し、全体管理の52/154/43確保境界も成功。core/UI/native実装が同じ検証は再利用し、フルゲートは修正後headの必須CIで行う |

## 表示と費用

最小窓の実寸は700×450 pixels（125%環境、560×360 DIP）。3言語、light/dark/合成high contrastで
選択・新名・プレビュー・中止結果を確認し、一覧・通知・2ボタンの矩形が重ならないことをassertした。
最小面では一度に見える行数を減らす。全文の名前はUIAへ公開する。
合成IMEのEnter/Esc保護を確認したが、物理IMEや実screen-readerの聴取ではない。

3管理ノート（変更対象2・未対応1）の実ファイルを7回処理した。phase通知のpipe、UI更新、OS schedulingを含む。
準備/プレビューは30.26〜32.45 ms、最初の公開と選択本文への投入は126.62〜185.13 ms、
残りの公開と結果表示は126.05〜147.89 ms。fixture復元は計測区間外で、製品Cコードへ時計を追加していない。
大量ノート・大本文の上限や全環境の速度を示す値ではない。1回の同期読取/履歴/公開はUIスレッドに残る。

復帰時に消えたexec sessionへ照会したが、既存result.jsonで完了を確認した。既存native-management-03の
run-dirを指定した重複呼出しは記録側の準備検査が起動前に拒否し、元の記録を上書きしていない。
推測したtools/record-run.ps1、eng/record-run.ps1、docs/SPEC.mdとPowerShell globの読取は失敗した。
実在するtools/record-run.py、SPECIFICATION.md、rgの-g指定へ戻した。Pillowの非推奨API警告は
画像比較結果を変えず、製品には同APIを導入していない。

管理UIの読取専用レビューは3件を指摘し、窓終了を取消しても残りの書込が再開しないよう終了経路で中止、
SelectionPatternの選択返却を単一選択行と一致、統合警告を新しい走査結果から再評価する修正へ取り込んだ。
画面に描いた範囲/統合注意と対象名をUIA statusにも含める。レビューは編集・build/testを行っていない。
新規のタグ適用結果表はCNF-009の対象へ登録した。推測した`eng/exhaustive_tables.json`の読取は不存在で失敗し、
実在する`eng/conformance-rules.json`を読んで変更した。

通常Debugの確保注入件数0を注入成功とは扱わず、measurementの結果を根拠とする。
GUIは専用合成dataと隔離desktopだけを操作し、実dataの変更はない。読み取りで推測した存在しないファイル名/record-run拡張子は、
実在するファイルを確認して読み直した。消えた旧exec sessionへの照会は履歴JSONを根拠に置き換え、
同じ成功済みコマンドを復旧のためだけに再実行していない。品質表のpatchは旧文言の一致失敗で一度停止し、
正しい文言を指定して適用した。
最終headの必須CIを完了条件とする。Waivers: none（追加）、WVR-0001継続。

品質表の追記patchは不要な一致条件で一度拒否され、変更を行わず正しい箇所へ再適用した。
.githooks/pre-commitの推測読取は不存在で失敗し、hookを新設/無効化していない。
