# 品質ゲート — NeNe Folio

> Status: normative（規範）/ 2026-09-09 初版
> 本書は「いま何が機械で守られているか」の**正本**である。
> 規範の本文は [ARCHITECTURE_CONSTITUTION.md](ARCHITECTURE_CONSTITUTION.md) と
> [CODING_RULES.md](CODING_RULES.md)、機械側の実体はこの文書に対応する。

`pwsh -NoProfile -File ./eng/check.ps1` がローカルと CI で共通の**唯一の完了定義**である。
統合には最終PR headの必須CI成功が要る。完成時の対象検証や個別の診断の成功は、その代わりにならない。
実行時点と結果の再利用はQLT-008 / QLT-012に従う（[ADR0044](adr/0044-tests-at-completion-and-integration.md)）。

---

## 1. ゲートの整合性規則（QLT-0xx）

### QLT-001 — ゲートは一つ

フルゲートをローカルで実行する場合も CI も、**同じ 1 つのコマンド**（`pwsh -NoProfile -File ./eng/check.ps1`）を呼ぶ。CI のワークフローに品質判断のロジックを書かない。
第 2 の完了定義を文書化しない。道具の版は 1 か所（eng/tool-versions.json）にだけ書く。

- 機械強制: **planned**（CI が `pwsh -NoProfile -File ./eng/check.ps1` を呼ぶだけであること）

### QLT-002 — 警告は失敗する

コンパイラの警告・整形差分・静的解析の指摘はすべて失敗にする。重大度の引き下げは禁止。

- 機械強制: **active**（clang-cl `/WX`、clang-tidy WarningsAsErrors、clang-format --Werror。実ツールの反例と復帰で検証）

### QLT-003 — baseline を作らない

lint・静的解析・アーキテクチャ・依存・テストのいずれについても baseline を持たない。既存の違反を「無かったこと」にする経路を作らない。
**唯一の例外機構は期限付きの狭い waiver である。**

- 機械強制: **planned** → CNF-005

### QLT-004 — 整形は検査であって修復ではない

整形は設定から決定的に決まる。CI がソースを書き換えて成功させることはしない。整形の正本は 1 つ（設定ファイルは置かないか 1 つだけ）。

- 機械強制: **active**（eng/check.ps1 の clang-format --dry-run --Werror）

### QLT-005 — ローカルと CI は同一

CI で必要なゲートは、すべてローカルで実行できる。検査を足したくなったら、まずローカルのゲートに足す。

- 機械強制: **planned**

### QLT-006 — アーキテクチャは実行可能

モジュール境界・import 規律・状態の経路は機械が検査する。散文だけのアーキテクチャ規則は、**「不能」と明記されない限り未完成**として扱う。

- 機械強制: **planned**

### QLT-007 — カスタムゲートには negative proof が要る

自作の検査は、「わざと違反させた入力で、意図した規則によって落ちること」と「正しい入力では通ること」の両方を示すまで信用しない。
証拠は [quality/gate-proofs.md](quality/gate-proofs.md) に記録する。ゲートを変えたら証拠も同じ変更で更新する。

🔴 **設定が在ることは、検査が効いていることの証明ではない。** 前例で 2 度、設定だけあって検査が効いていない状態が証明手順で発覚した。

- 機械強制: **planned**（検査器自身の正例・反例テストをゲートに結線する）

### QLT-008 — 振る舞いの変更はテストを伴う

振る舞いを変えたら、利用者の振る舞い・守る契約・直接影響する境界と受け入れ条件を決め、いちばん狭い安定した境界の有意味な既存または追加テストで確認する。
各テストは、どの具体的な誤動作を検出するか説明できるものにする。既存のテストで受け入れ条件を満たすなら重複して追加しない。
実装の行をなぞるだけのassertionや件数の水増しを目的にしない。不具合の修正は、可能なら**先に落ちる回帰テスト**で再現してから直す。
既存テストの整理は守る契約を調べた焦点Issueで行い、本方針を一括削除の根拠にしない。実行時点はQLT-012に従う。

#504の設定UIA循環はCTestの`settings_accessibility`で固定する。全3言語の前後巡回が同じ一意な全行を訪れて終了し、
名前・親子・buttonのfocus/矩形・閉鎖後の失効を実windowで確認する。修正前UI libraryで重複を検出してexit1、
修正版で318checks成功を確認した。専用desktopと`build/verification-settings`だけを使い、30秒timeoutを設ける。
[品質記録](quality/2026-11-01-settings-uia-cycle.md)に対象・反例・外部UIA確認を残す。

#479の編集clipboardはCTestの`paste_native`で固定する。専用WindowStation/desktop/dataを使い、
4入力の書式非混入、Unicode/選択/後続入力/UndoRedo、文字形式無しと入力停止時の保持を確認する。
修正前は11ptへ18ptが混入して失敗、修正後は414checks成功。30秒timeoutと既存data lockを使う。
native本文出力とviewport撮影の違い、全試行は[品質記録](quality/2026-11-06-editor-paste.md)に残す。

#709の段落書式キーはCTestの`editor_format_native`で固定する。専用desktop/dataで実HACCEL経路を通し、
Ctrl+R/2/5の書式不変、未保存本文/選択/UndoRedo、既存移動・選択・文字入力・製品キーを確認する。
修正前は段落書式変更で失敗、修正後339checks成功。30秒timeoutと既存data lockを維持する。
初期のfixture誤判定・全試行・限界は[品質記録](quality/2026-11-06-editor-format.md)へ。

#711の表示切替はCTestの`layout_edit_native`で固定する。専用desktop/dataで可視3択と狭幅chooserを使い、
6ステップと同表示再選択を変更状態/Redo保有状態で巡回する。本文・選択・dirty・UndoRedo・保存履歴と、
既存仕様に沿う本文/一覧/headerのfocusを486checksで確認した。30秒timeoutと既存data lockを維持する。
最初のfocus期待誤り・全試行・未検証範囲は[品質記録](quality/2026-11-06-layout-native.md)へ。

#715の別名保存はCTestの`save_as_native`で固定する。専用target/dataの実Ex入口から、変更EDIT/Redo保有/VIEW原文を確認する。
native本文/選択/UndoRedoと元md/全履歴を保ち、新mdの全bytes/初回履歴なしを照合。164checksと再実行170checksが成功した。
再実行で前回3コピーのbytes/hash/作成更新日時不変と新3コピー追加を照合した。30秒timeout/data lockを維持する。
全試行・未検証範囲は[品質記録](quality/2026-11-06-save-as-native.md)へ。
#733は同じnative試験で、既知同名とOSのcase alias衝突を確認する。台帳なしのsave_as_native302checks/1.39秒、台帳ありの再実行332checks/1.03秒成功。両stderr空・child/wrapper0・入力不変。旧3コピーのbytes/hash/作成更新日時と履歴不在も保持。
Ex語/focus/理由、native本文/選択/modified/UndoRedo、元md/全履歴/revision、全台帳/実名前集合、一時write不在を固定する。
拒否後の実UndoRedoと3コピー成功を維持する。[全試行と限界](quality/2026-11-06-save-as-collision-native.md)。
#735は実Ctrl+Shift+Sから名前面を開き、変更EDIT/Redo保有EDITの入力後取消を固定する。save_as_native389checks/1.83秒、CTest計1.84秒成功。stderr空・child/wrapper0・入力不変。
所有面のbutton/WM_CLOSE、modal中の内容disableと終了後の再enable/focus/native状態、元md/履歴/全名前集合を確認。
[全試行と限界](quality/2026-11-06-save-as-cancel-native.md)。
#737は空/入力済み/Redo保有の無題で次の新規を取消し、入力済みからの明示CtrlS取消も固定する。first_save_native385checks/1.52秒、save_as_native377checks/1.56秒、CTest計3.08秒成功。stderr空・child/wrapper0・入力不変。
新規だけのdiscard表示、native本文/選択/UndoRedo/focusと保存物保持、その後のEx初回保存を確認。
[全試行と限界](quality/2026-11-06-untitled-cancel-native.md)。
#739は入力済み/Redo保有の無題で「保存しない」を承認し、元カテゴリの次の空無題へ進む。first_save_native485checks/1.99秒、save_as_native377checks/1.82秒、CTest計3.81秒成功。stderr空・child/wrapper0・入力不変。
旧nativeUndoRedoの切断、実UndoRedo無効、元保存物保持と後続初回保存を確認。
[全試行と限界](quality/2026-11-06-untitled-discard-native.md)。
#741は実名前面から日本語名で別カテゴリへ初回保存し、native本文/選択/UndoRedo/focus・既存cursorを保持する。first_save_native485checks/2.18秒、save_as_native377checks/2.15秒、first_save_modal_native219checks/1.59秒。前2件はtest-01の成功結果を再利用。新規caseはtest-02でCTest1.61秒成功、stderr空・child/wrapper0・入力不変。
first_save_modal_native233checks/1.05秒、CTest1.06秒成功。旧target1fileの全bytes/hash/作成更新日時保持、旧/新とも履歴不在、新名1件のみ追加。stderr空・child/wrapper0・入力不変。
新md/全台帳/初回履歴不在/revision1回を確認。[全試行と限界](quality/2026-11-06-first-save-modal-native.md)。
#743は同じ名前面で空名/CON/既存oneを拒否し、本文/UndoRedo・入力・保存物を保持して有効名へ訂正する。first_save_modal_native308checks/1.19秒、first_save_native485checks/1.76秒、save_as_native377checks/1.98秒、CTest計4.93秒成功。stderr空・child/wrapper0・入力不変。
first_save_modal_native339checks/1.13秒、CTest1.13秒成功。旧target1fileの全bytes/hash/作成更新日時保持、新旧history不在、新名1件のみ追加。stderr空・child/wrapper0・入力不変。
[全試行と限界](quality/2026-11-06-first-save-retry-native.md)。

#717の改名はCTestのrename_nativeで固定する。専用target/dataの実Ex入口から同名無操作/理由と入力focus保持、
変更EDIT/Redo保有状態の先保存・履歴移動・native UndoRedo、VIEWの非保存改名、旧名/journal不在を252checksで確認した。
直接共用したEx/履歴helperのsave_as164checks、同data再実行252checksと前回md/4履歴のbytes/日時保持も成功した。
30秒timeout/data lockを維持する。全試行・初回complexity拒否・限界は[品質記録](quality/2026-11-06-rename-native.md)へ。
#731でノート行のselectedを主窓の共通選択設定がfalseへ上書きする不具合を発見し、設定順を修正した。
行の選択値はdrawer/applicationから導いた値を保持し、layout/mode専用値で上書きしない。
rename_native273checks、note_switch_native197checks、layout_edit_native606checksで
Unicode行名と全台帳/履歴、選択/非選択/category/Treeの一致、focus独立、表示/mode選択を機械的に固定する。
既存Unicode名保持の再実行も成功。red・全試行・外部client等の限界は[品質記録](quality/2026-11-06-unicode-rename-native.md)へ。

- 機械強制: **不能**（テストの有意味性・契約の充足は判断であり、記録とレビューで確認する。QLT-009のカバレッジ下限だけでは代替できない）

#719のreplace_save_nativeは、置換2欄からのCtrl+Sを各2回送り、語/選択/focusと本文native状態を保ちながら
履歴/revisionを1回だけ更新する221checksを確認した。Undo/Redo後の保存基準も固定する。
実置換/IME/I/O失敗/無題modalは再検査しない。[品質記録](quality/2026-11-06-replace-save-native.md)。

#721のmove_edit_nativeは実drawerのdragによるEDIT/Redo保有状態の往復移動と、各宛先の保存/履歴を固定する。
358checksと再実行398checksで本文native状態、非保存の移動、保存先追随、旧履歴保持を確認。製品経路は不変。
物理mouse/IME/全DPI/衝突/I/O失敗は対象外。[品質記録](quality/2026-11-06-move-edit-native.md)。

#723のunicode_save_nativeは署名1＋本文FEFF2、FFFD/VT/日本語/絵文字/tab/RTF escape/CRLFのnative保存を固定する。
251checksで未編集no-write、保存1回、raw md/history1の独立literal比較、UndoRedo/VIEW→EDITを確認。
直接共用search_native246checksも成功。物理IME・不正文字拒否・全改行・fresh-process再起動は対象外。
[品質記録](quality/2026-11-06-unicode-save-native.md)。

#725のfirst_save_nativeは変更した名前付きノートから新規へ進む保存と、無題の初回保存を固定する。
213checksで元履歴/架空行なし/初回履歴なし/全ledger/native本文・選択・modified・UndoRedoを確認。
前試行で作成したmdの全bytes/hash/作成更新日時保持も照合。modal/IME/衝突/I/O失敗は対象外。
[品質記録](quality/2026-11-06-first-save-native.md)。

#727のempty_save_nativeは全削除した本文の0bytes実在保存・全履歴1世代更新・実UndoRedo・空VIEW/EDIT再投入を固定する。
236checksでnative全文/選択/modified/UndoRedo/focusとNAMEDを保持。共通snapshotの正常0を終端確認付きで受理し、
直接共用unicode_save_native254checksも成功。getter/I/O失敗注入・物理IME・fresh-process復旧は対象外。
[品質記録](quality/2026-11-06-empty-save-native.md)。

#729のlocked_save_nativeは専用mdの共有違反による履歴read拒否→themed失敗面→編集保持→解除後の再保存を固定する。
156checksで元md/全履歴/revision不変、native本文/選択/modified/UndoRedo/focusとNAMEDを確認。直接共用empty_save_native253checksも成功。
履歴回転途中/write失敗・容量不足・ACL改変・全OS/DPI・物理IME・OS fallback面は対象外。[品質記録](quality/2026-11-06-locked-save-native.md)。
#745は同試験へVIEW/SAME+Redo編集の実Ex :trashnote BUSY拒否と入力再有効化を追加。locked_save_native 291checks/0.94秒、CTest0.95秒成功。台帳あり・source追加2file/target追加1fileのrepeatは350checks/0.73秒、CTest0.74秒成功。両runtime stderr空・child/wrapper0・入力不変。3既存mdと台帳1fileの全bytes/hash/作成更新日時不変。
[全試行と限界](quality/2026-11-06-trash-busy-native.md)。
#747はfilter_save_nativeで実Ctrl+Shift+Fの共有違反拒否/解除後保存/検索copy更新/SAME再focus/UndoRedoを固定。locked_save_native 290checks/0.9秒、filter_save_native 213checks/0.65秒、計1.56秒成功。別カテゴリを含む既存一致2件/非一致1件/台帳ありのrepeatはfilter_save_native 213checks/0.66秒、CTest0.67秒成功。runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。
[全試行と限界](quality/2026-11-06-filter-save-native.md)。
#749はlocked_save_nativeへ通常WM_CLOSEの共有違反拒否/窓と編集履歴保持/WM_QUIT未発行/再終了成功を追加。locked_save_native 426checks/1.08秒、CTest1.08秒成功。台帳あり/source2追加md/target1追加mdのrepeatは458checks/1.17秒、CTest1.17秒成功。成功runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。
[全試行と限界](quality/2026-11-06-close-busy-native.md)。
#751はsession_save_nativeで専用窓のOS終了通知の保存拒否/承認/重複QUERY/入力凍結/取消/UndoRedo/最終破棄を固定。session_save_native 233checks/0.78秒、CTest0.78秒成功。台帳あり/source追加2md/target追加1mdのrepeatは253checks/0.63秒、CTest0.63秒成功。runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。 既存振分け確認locked_save_native 459checks/1.14秒、CTest1.15秒成功。
[全試行と限界](quality/2026-11-06-session-save-native.md)。

### QLT-009 — カバレッジは下げられない

中核の分岐カバレッジ下限を 90% とする。閾値は上げてよいが下げてはならない。下げるには ADR が要る。
**置いていない層は「置いていない」と書く。**

- 機械強制: **active**（`eng/coverage.py` / `eng/coverage-policy.json`。core / application の全 `.c` を測定ビルドで計装し、LLVM の分岐 90% 下限。対象欠落・空の集計も拒否。テストを省いた反例が落ちることを毎回確かめる。中核の確保失敗の経路は測定ビルドでだけ注入して走らせる。2026-09-09・ADR 0004）

#407で測定用sanitizerをDebugの`compile_commands.json`から導出する。全対象core/application/test recordの
vector一致とASan/UBSan/nullability・recover禁止を必須にし、欠落・不一致・無効化を拒否する。
同vectorをcompile/linkと独立したbinding検出proofへ使い、正例成功・反例の該当診断と非zero終了を要求する。
proofのobjectは製品/testのobject集合へ入れない。90%下限・source inventory・負例profile判定は維持する。
実結果は[品質記録](quality/2026-10-29-nullable-allocation.md)へ残す。

#713は固定LLVMのCRT interceptor未検出をDebugの`/FUNCTIONPADMIN:6`で防ぐ。
link optionsは`eng/targets.cmake`が全Debug executableへ適用し、同変数の生成metadataを測定経路へ渡す。
`eng/sanitizer_probe.py`の一つのsource/判定器を既存`eng/prove-gates.py`と`eng/coverage.py`から使う。
正常呼出しは結果一致・stderr空、直接storeと`frexp`出力先はpoison到達・該当ASan書込診断・非zero終了を要求する。
直接storeの反例は`frexp`へ進まず、timeout/起動失敗の部分記録も残す。設定欠落・未検出・別原因失敗は拒否する。
全CRTや任意入力の安全性を保証するものではない。実反例・全試行・Release非適用は[品質記録](quality/2026-11-06-asan-interceptor.md)へ。

### QLT-010 — ゲートの弱体化はアーキテクチャ変更

重大度・除外・閾値・モジュール境界・必須 CI ジョブを変えるには明示的な根拠が要る。MUST 規則を弱める変更には ADR か waiver が要る。
**無関係な作業を通すために検査を切ることは禁止。**

- 機械強制: **不能**（判断そのものが対象。PR の手続きで担保する）

### QLT-011 — 依存は再現可能

依存の版は 1 か所にだけ書き、lock ファイルのドリフトはビルドを落とす。
同梱の資産（`fonts/`）は CNF-012 が守る。QLT-011 はライブラリの版の lock を対象にしたままで planned である。

- 機械強制: **planned**

### QLT-012 — 検証の頻度は変更に従う

**対象検証は機能の一区切りの完成時、フルゲートは最終PR headの必須CIに集約する。** 完成とは、受け入れ条件を確認できる候補を実装し、自己レビューで整えた状態を指す。
編集中は一行・一ファイル・一commitごとにテストを起動せず、完成時に変更機能と直接影響する境界の対象検証を一度まとめて行う。
修正前red、コンパイル不能、原因を切り分けなければ実装判断ができない場合は、理由と結果を残して途中でも必要な最小の再現・診断を実行できる。

成功済み結果には、対象commitまたは入力の識別、コマンド・結果・対象範囲・必要な環境条件・証跡を残す。
関係する実装・テスト入力・依存・ビルド設定・環境条件が不変なら、担当交代・レビュー・commit・push・mergeだけで対象検証を再実行しない。
関係入力の変更、失敗、具体的な未解決の疑いがあれば、その影響範囲を再検証する。SHAの変化だけを再実行の理由にせず、関係入力の差分を確認する。

#424の `tools/record-run.py` は、明示input/artifactのsnapshot・hashとraw出力・終了値を新規directoryへ記録し、
既存保存先への再実行を子起動前に拒否する。使い方と保証の限界は[ワークフロー](DEVELOPMENT_WORKFLOW.md#対象実行の記録)、
境界確認は[品質記録](quality/2026-10-29-run-record.md)に残す。入力選定・検証の妥当性や、全実行がこの道具を使ったことの機械強制ではない。
#432の`--input-list`は多数のinput指定を同じ記録処理へ渡す。解析した一覧bytes自身もinputとして保全し、
読込後の変更を再読snapshotで隠さない。準備拒否とsnapshot後の失敗、残る観測限界は
[対象品質記録](quality/2026-10-29-run-record-input-list.md)とワークフローに記す。新しいゲートや完了条件は設けない。

文書・コメント・運用規則だけの変更では、ローカルは差分・参照・関係する規約検査で完結させ、製品テストや再ビルドを行わない。
ツール変更はそのツールの動作を確認する。実機検証は、変更したOS境界・表示条件を対象テストで保証できないときに行う。
全件回帰や大規模matrixは、共有基盤などへの具体的な影響と狭い確認では不足する理由を説明できる場合に限る。
無関係な既存テストの失敗は分離して記録し、全件再試行や別件修正を独断で追加しない。必須CIの失敗が残れば統合は保留する。

PRは実装・レビュー中Draftのままにし、head・タイトル・本文を確定してからReadyにする。
**Ready前のローカルフルゲートは一律必須にしない。** 共通基盤の問題の診断やCI失敗の再現など、具体的な理由がある場合だけ同じコマンドを実行する。
統合には最終headで `pwsh -NoProfile -File ./eng/check.ps1` と必須 `check` が成功していることを要求し、文書PRもこの条件に従う。
CIの起動条件は `ready_for_review` / `synchronize` / `edited` / `reopened` で、非Draftのときだけフルゲートを実行する。
Ready後にheadを変える必要があれば先にDraftへ戻し、修正と必要な対象検証をまとめてから再びReadyにする。
CI結果はCIと外部証跡に残し、PR本文への結果追記だけを理由に `edited` で再起動しない。
失敗・取消・未起動は原因を確認して必要な再実行を行う。古い成功SHA・スキップされたジョブ・対象検証は、最終headの必須CI成功の代わりにならない。
対象検証が済んだ段階と、必須CI・統合・必要反映まで済んだ完了を区別して報告する。

必須 check `check` はフルゲートの job `full-gate` を見る**判定 job**で、`full-gate` が `success` 以外（Draft の skipped・cancelled・failure）なら
failure になる。GitHub は skipped の必須 check を「満たした」と数えるので、Draft の head はこの判定で `BLOCKED` になる（ADR 0037）。

- 機械強制: **active**（ruleset の必須 check `check` と strict up-to-date ＋ `.github/workflows/check.yml` の判定 job。
  実測は [2026-09-23 の確認記録](quality/2026-09-23-required-check-checks.md)）
- 実行時点・対象範囲・再利用の妥当性の機械強制: **不能**（判断。記録とレビューで確認する。activeは最終headの必須CI条件を指す）

### QLT-013 — 環境依存の主張は正直に名付ける

単体テストは表示サーバ・実機・実 OS 資源を必要としない。単体テストが通ったことを「実機で動く」の証拠として扱わない。
表示を伴う確認は [quality/gate-proofs.md](quality/gate-proofs.md) に環境と手順を書いて別に記録する。

- 機械強制: **planned**

---

## 2. 規約検査の規則（CNF-0xx）— `eng/conformance.py`

汎用の lint が見ないもの、つまり**このリポジトリ固有の規約**を検査する。依存ゼロで書き、ゲートから常に呼ぶ。
各規則には正例・反例の単体テストを付け、それもゲートに結線する（QLT-007）。

### CNF-001 — 禁止された総称名

`struct` / `enum` / `union` / `typedef` の名前とモジュール（ディレクトリ）名を字句検査する。snake_case と CamelCase の両方の語尾を見る。マクロで生成した名前は未対応。

- 対応する規則: C-010
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-002 — 1 ファイル 1 主要宣言

ファイル直下の型定義の数、ヘッダの主要型名とファイル名の一致、実装ファイルの外部関数の接頭辞（`<stem>_`）を字句検査する。`src/app` は例外（エントリポイント）。

- 対応する規則: C-011
- 機械強制: **active**（`eng/conformance.py` の `source_checks` / `top_level_types` / `external_functions`、既存正反例。ファイル直下の字句検査）

### CNF-003 — 抑制と waiver の結び付け

`#pragma clang diagnostic` / `#pragma warning` / `_Pragma` / `__pragma` / `NOLINT` 系のファイル単位の抑制を拒否。行単位（`NOLINTNEXTLINE`）には直前行の有効な waiver と検査名を要求する。

- 対応する規則: C-015
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-004 — waiver 台帳の整合

命名・必須メタデータ・索引・Scope・期限を検査。期限は UTC 日付で、Expires 当日まで有効。解除条件本文の妥当性はレビュー。

- 対応する規則: C-015
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-005 — baseline とゲート無力化の禁止

禁止された設定ファイル名と既知の無力化オプション（`/WX-`・`/W0-3`・`/wd`・`-Wno-error`・`-w`・`ignoreFailures`・`continue-on-error` 等）を検査。名指しの `-Wno-<診断名>` は無力化ではなく規則の選択なので対象外。任意のスクリプトによる迂回を網羅したとは扱わない。

- 対応する規則: QLT-003
- 機械強制: **active**（`eng/conformance.py` の `configuration_checks`、`eng/conformance-rules.json` の禁止名・既知オプション、既存正反例）

### CNF-006 — 文書整合

規則 ID の重複定義・未定義参照・マトリクス行と本文状態・active の証明行・相対リンク・未置換の雛形値を検査する。文言の真偽そのものは機械では判定しない。

- 対応する規則: QLT-007
- 機械強制: **active**（`eng/conformance.py`）

### CNF-007 — 検査設定の読込

`eng/config-bindings.json` の設定と参照先を照合する。文字列参照だけでは実行を証明しないため、整形・lint・CMake・リンカ検査は実ツールの反例も実行する（`eng/prove-gates.py`）。

- 対応する規則: QLT-007
- 機械強制: **active**（`eng/conformance.py` の `configuration_checks`、既存正反例と `eng/prove-gates.py` の実ツール反例。参照照合自体は文字列検査）

### CNF-008 — TODO と FIXME に Issue 番号

製品ソースと eng 内のコードにあるタスクマーカーは同じ行の Issue 番号を必須とする。検査器の正例・反例 fixture は対象外。

- 対応する規則: QLT-008
- 機械強制: **active**（`eng/conformance.py`）

### CNF-009 — 値ごとの表の網羅

閉じた `enum` の値ごとに 1 行を持つ表（`eng/conformance-rules.json` の `lineTables` で列挙のヘッダと表のファイルを名指しする）について、
列挙の本体にある値が表のファイルに `[値] =` としてちょうど 1 回現れることを字句で検査する。無い値と重複した値はどちらも違反。
`switch` を離れた表は `-Wswitch-enum` の網羅性（C-002）を失うので、この検査がその代わりを務める（ADR 0027）。
注記とコメントは対象から外れるので、行を消してコメントに残しても通らない。

- 対応する規則: C-002
- 機械強制: **active**（`eng/conformance.py` ＋ `eng/prove-gates.py` の実ツール反例）

### CNF-010 — 表示文言の置き場所

`src/` の C ソース（`.c` / `.h`）の**文字列**リテラルのうち、(a) 非 ASCII のバイトを含むもの、
(b) ワイド・`char16_t`・`char32_t` のリテラル（`L"…"` / `u"…"` / `U"…"`）で `\x` `\u` `\U` のエスケープを含むものは、
`eng/conformance-rules.json` の `textCatalog` が名指しするファイル（`src/core/ui_text.c`）以外に置けない。
利用者に見える文言の正本を 1 か所に保ち、第 2 の経路を**見つけやすくする**検査である（ADR 0030）。
ナローの `"\xEF\xBB\xBF"`（UTF-8 の BOM のバイト列）は (b) に当たらないので掛からず、例外の設定は要らない。
文字リテラル（`'\x1b'`）は対象外。注記は対象外だが、`c_code()` は文字列も潰すので使わず、
注記の選択肢を先に置いた 1 本の字句走査で文字列リテラルだけを取り出す。`tests/` / `out/` / `eng/` は対象外。
結合や `#define` による回避は字句では見えない。

- 対応する規則: C-018
- 機械強制: **active**（`eng/conformance.py` の `text_catalog_checks` ＋ `tests/conformance` の 10 件 ＋ `eng/prove-gates.py` の実ツール反例 P22）

### CNF-011 — 文言の表の列の網羅

`eng/conformance-rules.json` の `textCatalog` が名指しする表のファイル（`src/core/ui_text.c`）の
指示付き初期化子 `[値] = { … }` について、**`{…}` の中を深さ 0 のカンマで区切った要素の数**が
`textCatalog.languages` が名指しする列挙（`src/core/folio_language.h`）の値の数と一致し、
各要素は**隣接する文字列リテラルを連結してから**空でないことを字句で検査する。
訳し忘れ（列の欠落）と空の列を拒む。`textCatalog.emptyAllowed` に挙げた ID（`UI_TEXT_EMPTY`）だけが
3 列とも空でよい。列は添字で引くので `switch` が無く、C-002（`-Wswitch-enum`）では守れない（ADR 0032）。
要素の数を数えるのは、`clang-format` が長い行を隣接リテラルに割るため（リテラルの数では列を数えない）。

- 対応する規則: C-018
- 機械強制: **active**（`eng/conformance.py` の `text_column_checks` ＋ `tests/conformance` の 10 件 ＋ `eng/prove-gates.py` の実ツール反例 P23）

### CNF-012 — 同梱の資産は manifest と一致する

`eng/conformance-rules.json` の `bundledAssets` が名指しする manifest（`fonts/manifest.json`）について、
(a) `entries` と `licenses` の各 `filename` が manifest と同じフォルダに在り、**バイト数と SHA-256（小文字 hex）が一致**する、
(b) そのフォルダに manifest に無いファイルが無い（`bundledAssets.ignore` の `README.md` と `manifest.json` を除く）、
(c) 各 entry の `license` が `licenses` の `filename` に在る、を検査する（ADR 0036 決定 2）。
書体を取り違えたり、差し替えて manifest を直し忘れたり、出所の記録が無いファイルを置いたりすることを拒む。
版を上げるときは manifest とファイルを同じコミットで差し替える。manifest の値そのものが upstream と一致するかは検査しない（取得時に照合した記録を正本とする）。

- 対応する規則: ARC-001
- 機械強制: **active**（`eng/conformance.py` の `bundled_asset_checks` ＋ `eng/prove-gates.py` の実ツール反例 P24 / P25）

🔴 **検出語は検査器のソースに直書きしない**（検査器が自分自身を違反として報告する。前例: xi-tools 初版で 7 件の自己検出）。
🔴 **テストソースは検査対象から外す**（テストは意図的な違反を書く場所）。

---

## 3. 強制マトリクス

規範の各規則が、いまどの層で守られているか。**この表が実装と食い違ったら merge を止める。**
CNF-006 が「本文に定義があるのにここに行が無い」を拒否する。

| 規則 | 状態 | 機械強制の実体 |
| --- | --- | --- |
| ARC-001 | planned | レビュー事項（CNF-001 が温床を減らす） |
| ARC-002 | active | eng/targets.cmake（configure で宣言外の依存と OS ライブラリを拒否）＋ eng/conformance.py --build-dir（File API の実グラフ・include） |
| ARC-003 | active | eng/symbols.py --require core application（llvm-nm ＋ eng/symbol-allowlist.json）＋ 字句検査 |
| ARC-004 | planned | |
| ARC-005 | planned | |
| ARC-006 | planned | |
| ARC-007 | active | eng/symbols.py（非決定シンボル。src/adapters/win32 だけ対象外）＋ 字句検査 |
| ARC-008 | planned | レビュー事項 |
| ARC-009 | planned | |
| ARC-010 | planned | |
| ARC-011 | planned | |
| ARC-012 | 不能 | QLT-010 の手続き |
| C-001 | planned |  |
| C-002 | planned | clang-cl `-Wswitch-enum` / `-Wcovered-switch-default` |
| C-003 | planned | 不完全型・`-Wcast-qual` |
| C-004 | planned | `-Wuninitialized`・clang-tidy NullDereference・nullability |
| C-005 | planned | `[[nodiscard]]`＋`-Wunused-result` |
| C-006 | planned |  |
| C-007 | planned | 不完全型 |
| C-008 | planned |  |
| C-009 | planned |  |
| C-010 | planned | CNF-001 |
| C-011 | planned | CNF-002 |
| C-012 | planned | clang-tidy readability-* |
| C-013 | planned | `eng/symbols.py`（スレッド API） |
| C-014 | planned |  |
| C-015 | planned | CNF-003 ＋ CNF-004 |
| C-016 | planned | `-Werror=vla`・clang-tidy insecureAPI・ASan/UBSan |
| C-017 | planned |  |
| C-018 | active | eng/conformance.py（CNF-010。列の網羅は CNF-011） |
| GIT-001 | planned | PR テンプレート＋CI（`Closes #N`） |
| GIT-002 | planned | ruleset＋CI（head ブランチ名） |
| GIT-003 | planned | `.githooks/commit-msg`＋CI（全コミットと PR タイトル） |
| GIT-004 | planned | PR テンプレート＋ruleset（squash-only） |
| QLT-001 | planned | |
| QLT-002 | active | clang-cl・clang-tidy・clang-format と実ツール反例 |
| QLT-003 | planned | CNF-005 |
| QLT-004 | active | clang-format --dry-run --Werror |
| QLT-005 | planned | |
| QLT-006 | planned | |
| QLT-007 | planned | |
| QLT-008 | 不能 | テストの有意味性・契約の充足を記録・レビュー |
| QLT-009 | active | LLVM 計装・llvm-cov・eng/coverage.py（分岐 90%・確保失敗の注入・反例） |
| QLT-010 | 不能 | PR の手続き |
| QLT-011 | planned | |
| QLT-012 | active | 最終headの必須CI: ruleset の `check`（strict）＋判定 job（ADR0037）。実行時点・範囲・再利用の判断は不能（ADR0044） |
| QLT-013 | planned | |
| CNF-001 | planned | eng/conformance.py / tests/conformance |
| CNF-002 | active | eng/conformance.py / tests/conformance |
| CNF-003 | planned | eng/conformance.py / tests/conformance |
| CNF-004 | planned | eng/conformance.py / tests/conformance |
| CNF-005 | active | eng/conformance.py / tests/conformance |
| CNF-006 | active | eng/conformance.py / tests/conformance |
| CNF-007 | active | eng/conformance.py / tests/conformance / eng/prove-gates.py |
| CNF-008 | active | eng/conformance.py / tests/conformance |
| CNF-009 | active | eng/conformance.py / tests/conformance / eng/prove-gates.py |
| CNF-010 | active | eng/conformance.py / tests/conformance / eng/prove-gates.py |
| CNF-011 | active | eng/conformance.py / tests/conformance / eng/prove-gates.py |
| CNF-012 | active | eng/conformance.py / eng/prove-gates.py |

---

## 4. `pwsh -NoProfile -File ./eng/check.ps1` に入っている層

| 層 | 目的 | 実体 |
| --- | --- | --- |
| コンパイル | 型安全・網羅性・警告ゼロ | clang-cl（C23）・`eng/targets.cmake` の警告集合 |
| 整形 | 文字列としての正本 | clang-format / .clang-format |
| API 禁止 | 時刻・乱数・環境・OS import・宣言外の外部シンボル | `eng/symbols.py --require core application`（`llvm-nm` ＋ `eng/symbol-allowlist.json`。アーカイブ内と宣言済み依存で解決してから照合）。字句検査は補助 |
| 静的解析 | null 逆参照・複雑度・関数の大きさ・危険な API | clang-tidy / .clang-tidy |
| アーキテクチャ | 宣言グラフ・実ターゲット・ソース所有 | targets.cmake / architecture.json / CMake File API |
| 規約検査 | NeNe Folio 固有 | `eng/conformance.py` |
| 検査自身のテスト | 規約検査・シンボル検査・実ツールの正例・反例 | unittest / eng/prove-gates.py |
| 版と配布検査器 | 実exeの版・manifest・AMD64と、配布ファイル/資産/ZIP/MSIX/出力先の拒否境界 | `eng/test-release.py`（`eng/release.py`の正例・反例。ゲートが作ったexeを使い、追加ビルドを行わない。ADR0045/0080） |
| 単体テスト | 振る舞い | CTest / tests/unit（ASan / UBSan / nullability 付き・`-fno-sanitize-recover=all`。Debug 構成の全 target を計装） |
| 設定UIAの回帰 | 重複・循環・名前/矩形/focus・失効 | CTest / tests/win32（同じDebug計装・製品library/manifest、専用desktop/data。#504） |
| カバレッジ | 中核の検証密度 | `eng/coverage.py` / `eng/coverage-policy.json`。測定ビルド（ASan 付き・確保失敗の注入）で LLVM の実分岐を 90% 以上要求 |
| 依存 | 道具の版と実行時依存 0 | tool-versions.json / architecture.json / `/MT`（R1） |

配布時は`eng/package-release.ps1`が別のRelease buildを作り、`eng/release.py`で版・manifest・AMD64に加え、
OS DLLのimport許可集合、完全な同梱一覧、font manifest、ZIPまたはMSIXの実内容を検査する。
MSIXは固定SDKのMakeAppxによるpack/unpackと、全payloadのhash・Store版・identity・PNG・SHA256 block mapを照合する。
実インストール/更新/アンインストール/WACK/Store受理はこの生成検査とは別の対象確認である（ADR0080）。
Debugのsanitizerを含むexeのimportをRelease用許可集合に通すものではない。
配布の正例・反例の実測は[品質記録](quality/2026-10-12-release-prep-checks.md)へ残す。
この成果物検査は最終headの必須CIを置き換えない。

---

## 5. マージゲート

`main` は次を必須とする。

- Pull Request 経由であること
- `pwsh -NoProfile -File ./eng/check.ps1` が成功していること（CI の必須 check）
- 生成物のドリフトが無いこと（作業ツリーが汚れないこと）
- 期限切れ waiver が無いこと
- 未解決のレビュー指摘が無いこと
- squash merge のみ

`main` への直接 push・force push・ブランチ削除は禁止する。

🔴 **リポジトリ設定（ruleset）を適用したら、その事実を [quality/gate-proofs.md](quality/gate-proofs.md) に記録する。**
**設定していないものを「必須になっている」と書かない。**
