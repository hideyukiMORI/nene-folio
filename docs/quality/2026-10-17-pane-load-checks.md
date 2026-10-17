# 本文投入失敗の保護 — 2026-10-17

Issue: [#310](https://github.com/hideyukiMORI/nene-folio/issues/310)。契約は[ADR0054](../adr/0054-checked-rich-edit-load.md)。
製品候補は`a1b6ba3f97f05803f65b53941f57d2056634dbf7`、開始mainは`9b6c34d318817ecb83460bca202c607c71ed8ebd`。
恒久証跡は`D:/NeNeFolio/agents/310-stream-failure/`。対象成功と最終head必須CI・統合・通常exe反映は区別する。

## 修正前と変更境界

10月15日のredはmain `55f08641`の実RichEditへ部分本文`part`と非0のdwErrorを注入したもの。
旧実装は編集開始を成功とし、後続保存で元のmdを4 bytesへ置き換えた。正常空本文の戻り0は成功対照だった。
旧`red-results.json`・source/hash・fixture前後・expected exit 1を保持し、今回のgreenで上書きしない。

投入を`note_pane_load`へ揃え、readinessの所有を同じ部品へ閉じた。未完成なら取得・置換・行番号cache採用・保存を拒否する。
失敗時はreadonly、readonly化自体も失敗すれば子窓だけをdisableし、同mode札から現在の保存済み本文で再試行する。
P0終了保護との統合では、無対話のnotice配置、投入中のEN_MSGFILTER拒否、終了phase中の受動的再描画失敗で箱を開かない境界も確認する。

## コマンドと結果

作業木`D:/NeNeFolio/worktrees/310-stream-failure`、固定環境`eng/toolchain.ps1`、Debug buildを使用。

- `cmake -S . -B D:/NeNeFolio/outputs/310-stream-failure/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`成功。
- `cmake --build D:/NeNeFolio/outputs/310-stream-failure/build --target nenefolio folio_tests -j 4`成功（clang-tidyを含む）。
  終了phaseの箱抑止追加後は`--target nenefolio -j 4`で変更UIだけ再buildし成功。
- `D:/NeNeFolio/outputs/310-stream-failure/build/folio_tests.exe --pane-load`と`--commands-ui`成功。
  最後のUIだけの修正では中核・文言・unit入力が不変のため再利用した。
- 変更C/headerの`clang-format --dry-run --Werror`、`python eng/conformance.py --build-dir D:/NeNeFolio/outputs/310-stream-failure/build`、
  `git diff --check`成功。conformanceは0違反。初回formatは短縮文言の行配置で失敗し、整形後に成功した。ゲート変更なし。
- 外部証跡の`prepare-green.ps1`、`compile-green.ps1`、`run-green.ps1`を順に実行。
  隔離desktopの初回native runはexit 0、130 assertion成功・0 failure、54回のstream呼出し。
  5回の期待した失敗箱呼出しを記録した。通常desktopのforeground値は69734のままで変化なし。

詳細は`resume-verification.json`、`resume-*.log`、`green-results.jsonl`、`green-verification-report.json`。
製品exe SHA256は`48285021092CCBF1FD9F43CD8924318DA353F6C4B984987AB35AFAD51430AF4F`。
source/lib等249件のhash不変を確認。`resume-inputs.json`/`green-inputs.json`、`resume-source.zip`、最終libs/exe、fixture前後を収載した。
`green-runtime.zip`は20ファイルの読戻しhash照合済み。通常exeは別build先なので候補hashとは区別する。

## 確認した振る舞い

- 正常空RTFの戻り0、非空Unicode・改行の保持。非空対照は文字数12、dwError 0。
- 部分・空・旧本文とdwError 30、callback未供給、負戻りを失敗として検出。取得の出力引数と失敗前の変更印を保つ。
  不完全本文の置換と行番号表示を拒み、readonly開始・編集許可・cleanupの失敗も安全側へ閉じる。
- 名前付き編集の失敗後、正典保存・離脱・Ctrl+S・OS QUERYは元のrawと履歴を変えない。
  QUERYでは箱・名前入力0増分、窓を保持。別modeのVIEWは保存拒否、選択済みEDITの再クリックで回復する。
- 入れ子投入、文書切替、テーマ・言語採用、root/layer/EDIT入力とEN_MSGFILTER/Escを投入中に拒否。
  外側が借用するRTFとreadinessを保ち、外側の正常投入は完了する。
- 正常EDITの同mode操作では再投入せず、未保存本文・選択・変更印・Undoを保つ。保存後の再試行は最新本文から再生成する。
- 編集のまま切替、新規無題、明示破棄、VIEW再描画、VIEWからの履歴採用で失敗を伝え、再試行で回復する。
  履歴の再クリックで回復し、正常EDITの履歴復元もUndo一回で直前の未保存本文へ戻る。
- VIEWでQUERY承認後、試験側appearance値変更と実WM_SETTINGCHANGEから再描画失敗を起こす。
  箱・名前入力0増分、保存済raw不変、END(FALSE)後の親復帰・通知保持・同VIEW札の再試行を確認。
- 初回投入失敗は窓生成失敗となり、出力窓なし・子HWND破棄を確認する。

## 表示・レビュー・限界

実Win32、120 DPI、最小BOTH 560×360 DIPで通知幅を測定。利用可能335pxに対しJA 211px、EN 288px、ZH_HANS 200px。
`green-ja-minimum.png`、`green-en-minimum.png`、`green-zh-minimum.png`をnative担当と設計席が目視し、全文収容・重なりなしを確認した。
独立静読の終了phase中の失敗箱を修正し、最終差分で未解決P1/P2なしを確認。`review-20261017.md`を保持する。

native fixtureは製品source/libsと実RichEditを使い、試験側subclassで故障を注入する。
失敗箱・名前入力は待機を防ぐ記録関数へ置き換え、必要/禁止呼出しの増分を検査した。ダイアログ描画の検証ではない。
OS明暗は試験側appearance値で切り替え、registryや通常の利用者dataは変更しない。
外部probeのcompile警告85件（nullability 82、試験側未使用戻り2・未使用関数1）とASanの`interception_win: unhandled instruction`起動診断1行を保存。
workerはexit 0でASan/UBSanエラー報告なしだが、完全なinterceptor coverageや警告ゼロとは主張しない。
集計用Pythonの初回JST取得はtzdata不足で失敗し、固定UTC+09:00へ修正。製品/native入力不変で再試験しなかった。
実メモリ不足・物理IME/実打鍵・実ログオフ/Update・電断・別OS/DPI・全テーマ・本文/Undoのrollback・hide実機受理は未測定。

規則: FR-005/006/015、ARC-001/004/008/010/011、C-002/005/008/012/018、QLT-008/012/013、ADR0053/0054。
保存schema・依存・ゲート・公開配布変更なし。Waivers: none。
