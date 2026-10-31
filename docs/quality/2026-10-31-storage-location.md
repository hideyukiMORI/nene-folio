# ノートの保存先 — 2026-10-31

- Issue: #477。hideの#390方針と、#475の実機受理後の対応継続依頼。
- 規則: ARC-001〜012、C-002〜018、CNF-009〜011、QLT-008/009/012/013、GIT-001〜004。
- 設計: [ADR0077](../adr/0077-note-storage-location.md)。単体実行、Waivers: none。
- 作業木: `D:/NeNeFolio/worktrees/477-storage-location`、証跡原本: `D:/NeNeFolio/agents/storage-location-20261031`。
  一時出力: `D:/NeNeFolio/outputs/477-storage-location`。

## 変更と契約

coreの`storage_path`/`startup_settings`、applicationの`storage_state`/`storage_port`、Win32の
`storage_adapter`を通して、確定rootを既存persistenceへ一回注入する。旧exe位置の再解決は撤去した。
設定画面の「ノートの保存先…」は初回/復旧と同じ`storage_prompt`を開き、直接入力とOS folder pickerを共通確定へ渡す。
native名、既存modal owner、終了要求、描画の`dialog_theme`/`ui_part`を共有する。

portable既定はexe隣data、起動設定不在なら従来の起動を保つ。package identityを持つ版はDocuments/NeNeFolioを
初回候補にし、LocalAppData/NeNeFolioへ起動設定を置く。`nenefolio-startup.json`版1はversion/directoryだけで、
空directoryは配布形式の既定場所。ノート側settings版4、台帳、履歴のschemaは不変。
次回用の場所だけを変え、実行中のroot・未保存本文・Undoを保つ。ノートの自動移動/複製/削除はしない。
明示先の消失・読書き不能・使用中は復旧面へ戻し、消えたrootを起動だけで作り直さない。
未知版/不正設定は起動を断り、専用lockと期待値照合で他プロセスの変更を上書きしない。

## 対象検証

以下は各`tools/record-run.py`の`result.json`、入力snapshot、stdout/stderr、成果物hashで対応を固定した。
成功入力は工程が変わるだけでは再実行しない。最終headのフルゲートは必須CIで別途要求する。

| 記録・コマンド | 結果と確認範囲 |
| --- | --- |
| `build-03` / `build-04`: `pwsh -NoProfile -File <agents>/build.ps1` | 正典toolchainのDebug/clang-tidy、conformance、`cmake --build build --target nenefolio folio_tests`成功。後者はCOM/文字計測修正後 |
| `unit-02`: `pwsh -NoProfile -File <agents>/unit.ps1` → `folio_tests.exe --storage-location` | path/JSON・portable/installed phase・復旧・同値・設定書込/競合失敗・次回のみ変更・既定へ戻す・3言語の対応に成功 |
| `measurement-run-02`: `<agents>/measurement.ps1` | `eng/coverage.py`の正典測定builderとDebug由来sanitizer vectorを使用し、対象selectorだけ実行。各確保失敗も注入。分岐はpath 88/94 (93.62%)、codec 52/54 (96.30%)、state 56/58 (96.55%)、関数100%。全件の下限判定とは区別する |
| `compile-adapter-03` / `adapter-run-01`: `<agents>/probe.ps1` | 41 checks / 0 failures。実KnownFolder解決、既定/明示root、消失の非再作成、1階層作成、非再帰、拒否、probe後始末、lock競合、既存file不変、指定rootでの合成ノート読書き、設定の変更検出/未知版/再起動 |
| `compile-ui-04` / `ui-run-02`: 同上 | 258 checks / 0 failures。初回Enter/取消、native名、Esc/Enter、合成IME通知、実OS picker取消、現在/次回の再表示、ノート非移動、本文/選択/dirty/focus/実UndoRedo保持、3言語小窓、prompt中の主窓WM_CLOSEから既存保存/終了への合流 |
| `compile-ui-05` / `ui-visual-01`: 同上、visual selector | 82 checks / 0 failures。同期WM_PRINTによる非黒画像、初回とJA/EN/ZH小窓のpath欄・説明・buttonを目視確認。機能試験は繰り返していない |
| `main-run-01`: `python -B <agents>/main-smoke.py` | 実NeNeFolio.exeの専用desktop起動4 cases / 0 failures。既定data不在・明示root・消失rootの取消・未知版の拒否、既存設定/合成ノートのhash不変、終了code 0/0/0/1 |

adapter/UI fixtureは正典の対象translation unit末尾へ試験だけを結合し、既存compile/link vectorとlibraryを使用した。
mapで元memberの重複選択が無いことを確認し、製品hook・別の製品経路を加えていない。
UIは`CreateDesktop`の専用desktopで、通常dataやhideの入力を使っていない。BMPからPNGへの変換は形式変換だけ。

## 失敗試行と修正

- `build-01/02`: promptの複雑度、icon headerのnullability、mainの引数数で停止。helper分割・注釈・既存ports集約で修正。
  未生成成果物のsnapshot要求はwrapper125として記録し、成功扱いにしていない。
- `unit-01`: UI文言の末尾ID期待値が追加前の値だったため失敗。列挙末尾へ更新し、`unit-02`で確認。
- `measurement-run-01`: 診断driverのimport準備に失敗し、exeは未実行。正典engをimportする位置を直した。
  成功測定にはWindows ASan interceptorのinstruction警告が1件あるが、実行exit0・test failure0。ログを保持する。
- adapter compile1/2とUI compile1/2はfixtureのnullability/戻り値/引数/符号変換等で停止し、fixtureを修正した。
- `ui-run-01`: 211 checks / 1 failure。主窓より前の初回promptでCOMが未初期化となりnative名設定に失敗した。
  prompt自身でCOM寿命を対にし、GDI+寿命も共通icon経路へ集約。機能の再検証は`ui-run-02`で成功した。
- 最初のPrintWindow画像はcapture時点に黒くなるため描画成功とは扱わず、fixtureを同期WM_PRINTへ直し、
  非黒判定付きのvisual selectorで確認した。

## 制限と統合の確定

物理IME入力、実readerの聴取、DWM実画面呈示、異なるDPI間移動、実ネットワークUNC、
実MSIXの初回/更新/アンインストールは未確認。KnownFolderの実照会とinstalled phaseの試験を、実インストール成功とは呼ばない。
保存先選択のprobeは全台帳/mdの健全性を保証せず、選択後の権限/使用状態も変わり得る。ノート実読込は既存経路で行う。
公開済みv0.3.0 ZIPの再生成、#466と停止済み継続品質改善の再開は行わない。

最終PR head・必須CI・main・通常exeの版/hash/反映・証跡全hash保全・終了整理は
`D:/NeNeFolio/agents/storage-location-20261031/integration-completion.json`を確定原本とする。
稼働中の通常版を強制終了せず、未保存の本文がある場合は停止を待つ。
