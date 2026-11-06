# 編集入力のCJK書体を維持する（#783）

## 根拠と採用した経路

hideから23:20頃、編集行の日本語だけ細く見える画像と継続対応の依頼を受けた。
画像の実ノート内容はリポジトリやIssueへ転記せず、独立した短い合成文で調べた。
平文貼り付けでも、74 UTF-16単位中45単位がNoto Sans JPから游ゴシックへ、記号1単位がSegoe UI Symbolへ切り替わった。
残る28単位はNoto Sans JP。全て高さ220twip、nativeのwWeight値は0であり、太さ属性の違いだけではない。
この再現は画像の原因として整合するが、実ノートの内部書式を直接採取した結果ではない。

最終変更は`note_pane_reface`の既存face適用へ、`SCF_ASSOCIATEFONT`による
SHIFTJIS_CHARSETとGB2312_CHARSETの関連付けを加える8行。
faceの正本は既存ui_font/ui_faceのまま。同じfaceを日本語・簡体字のnative bindingへ渡す。
既存TOMのUndo停止/再開と変更印の退避復元内で行い、本文/選択/RTFを再投入しない。
自動binding自体は有効なままなので、絵文字など同梱書体の範囲外はnativeの代替書体で表示する。
言語設定、UIA、保存形式、CMake/ゲート、版番号、同梱フォントファイルは変更しない。

根拠: [Microsoftのfont binding説明](https://devblogs.microsoft.com/math-in-office/richedit-font-binding/)、
[EM_SETCHARFORMAT](https://learn.microsoft.com/en-us/windows/win32/controls/em-setcharformat)、
[EM_GETLANGOPTIONS](https://learn.microsoft.com/en-us/windows/win32/controls/em-getlangoptions)。
APIの一般説明と、この機械での観測を区別する。

## 不採用候補と診断

IMF_AUTOFONTを消す候補は日本語を揃えたが、編集・閲覧の絵文字を欠落glyphにしたため不採用。
初期の2,051checks成功は文字列/書式比較の結果で、表示受理ではない。
保存した修正前exeをD専用領域へ展開し、同じnative印刷描画の絵文字が表示されることを再確認した。
これを受け、最終試験には絵文字のnative bindingと別faceの保持を追加した。

6条件のnative比較では、既定で游ゴシック、DEFAULT_CHARSETの指定ではさらに英数字がCalibriへ変化した。
全本文へSHIFTJIS_CHARSETを当てる条件は揃うが、本文全体の再書式化を必要とするため採用しない。
日本語の関連付けだけで45単位がNoto Sans JPへ戻り、絵文字はSegoe UI Symbolを保持した。
複数CJK関連付けも比較し、製品で使う日本語/簡体字の2集合に限定した。
診断で直接当てたCFM_SIZE/LCIDや全本文へのSCF_ALL、一時的な未使用関数注釈は完成候補に含めない。

## 完成候補の検証

Windows 11上の実Msftedit/RICHEDIT50W、正典clang-cl Debug、ASan/UBSan/nullability付き。
paste_nativeは独立window stationとdesktopを使い、通常clipboard・通常app・実ノートを操作しない。
描画はEM_FORMATRANGEによるnative文書のbitmapで、物理画面のスクリーンショットとは区別する。

- `cmake --build build --target image_status_tests`: build-association-03成功。製品8行は02から不変。
- `ctest --test-dir build -R '^paste_native$' --output-on-failure --verbose`: 3,715checks、2.57秒成功。
  WM_PASTE/Ctrl+V/Ctrl+Shift+V/Shift+Insert、複数行の日本語/簡体字/英数字/句読点/金額記号/絵文字を確認。
  日本語→English→简体中文→日本語の実設定操作と閲覧/編集往復後も確認した。
  各文字のface/高さ/weight/色/装飾、全本文/選択終端/modified、WM_CHAR追加入力、Undo/Redoを比較。
  絵文字の代替書体、閲覧見出しの太字、全保存履歴/store_revisionも維持した。
- `ctest --test-dir build -R '^(settings_edit_native|editor_format_native)$' --output-on-failure --verbose`:
  設定中の編集保持523checks/1.45秒、書式キー339checks/0.83秒、合計2.29秒成功。
- 完成候補のruntime stderrは空、child/wrapperは0、記録エラーなし、指定入力は不変。
  対象ソースを変えずに行うpush/レビュー/統合では成功結果を再利用する。

完成画像の日本語・簡体字・英数字の書体と、編集/閲覧の絵文字・見出し装飾を目視した。
最終PR headの必須CIで`pwsh -NoProfile -File ./eng/check.ps1`を要求する。
統合・反映・保全整理の確定結果は下記integration-completion.jsonを正とする。

## 全試行

原本は`D:/NeNeFolio/agents/editor-font-binding-20261106/`。各runはrecord-run.pyのinput/artifact snapshot、raw出力と終了値を保持する。
下表はchild/wrapper終了値であり、ラッパー記録失敗を製品成功へ読み替えない。

| run | 終了値 | 結果 |
| --- | --- | --- |
| build-baseline-01 | 0/125 | build成功、exe成果物path指定誤りで記録失敗 |
| build-baseline-02 | 0/0 | 正しいverification-images配下を記録 |
| test-baseline-01 | 8/125 | 描画完了位置とCRLF展開長を混同。期待bitmap pathも誤指定 |
| build-diagnostic-01 / test-diagnostic-01 | 各0/0 | raw native文字位置へ訂正、4option比較493checks/1.43秒 |
| build-red-01 / test-red-01 | 0/0、8/8 | 旧製品が新しい全字書体比較で失敗 |
| build-final-01 / test-final-01 | 各0/0 | IMF_AUTOFONT無効候補。2,051checks/1.21秒だが絵文字欠落で不採用 |
| baseline-render-01 | 0/0 | 保存済み旧exeで絵文字表示を再確認 |
| build-policy-01 / test-policy-01 | 0/0、8/125 | 診断の期待本文不一致で停止。後続5bitmap未生成 |
| build-policy-02 / test-policy-02 | 0/0、8/125 | 複数窓再作成時の開始本文不一致。後続5bitmap未生成 |
| build-policy-03 / test-policy-03 | 各0/0 | 各窓作成前にモデルを同じ保存本文のVIEWへ戻し、6条件541checks/1.41秒 |
| build-association-01 | 1/1 | 試験のnative長とsizeofの符号比較をコンパイラが拒否 |
| build-association-02 / test-association-01 | 各0/0 | 正の長さ検査後のsize_t比較へ訂正。2,131checks/1.66秒と絵文字描画成功 |
| build-association-03 / test-association-02 | 各0/0 | 簡体字・言語切替を追加、最終3,715checks/2.57秒 |
| boundaries-01 | 0/0 | 変更したrefaceと書式キーの直接境界、上記2試験成功 |

文書検査check-01は相対input pathを記録ツールが子起動前に拒否した。run/artifactは未生成で、
preparation-error原本を別保存し、絶対path一覧へ訂正したcheck-02で8file/参照/最終native入力同一性/規約検査が成功した。
この記録追記後の文書再検査も同じ経路を使い、製品試験は反復しない。

初期の画像長検証はGetWindowTextLengthWのCRLF展開長から、EM_GETTEXTLENGTHEXのnative文字数へ訂正した。
読み取り補助の存在しないpath参照、文字コード出力失敗、大き過ぎた出力の省略は製品試験失敗と区別する。
通常版は1.3.2（PID25132での稼働を23:52:15に観測）を保ち、強制終了しない。
必要反映の前には保存・終了を待ち、通常exeの停止と実data保全を確認して正典prepare-real-machine.ps1を使う。

## 規則・限界

FR-006、ARC-001/002/004/008/011/012、C-002/005/008/014/016/017、QLT-008/010/012/013、CNF-006、GIT-001〜004。
所有者/公開API/保存schema/配布版/ゲートは不変。追加waiver none、既存WVR-0001は別対象のまま継続。
物理IMEの変換中/候補選択、全OS/フォント欠損/全Unicodeの字形、実ノートの直接内部書式は未確認。
同梱フォントとOSの同名family衝突の限界はADR0036を維持する。既に開いた実行中appには適用されない。
公開済み1.3.2 ZIP/MSIXとStore申請を差し替えず、次の版への修正とする。
保留466/634/530と旧削除拒否領域には触れない。
