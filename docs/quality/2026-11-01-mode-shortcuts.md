# #490 — モード切り替えキーとボタンの説明

基点main `90fe2c5e07ad77f14e464f24582bc9aee56cc4bb`。hideの依頼に従い単体実行。
[ADR0081](../adr/0081-mode-shortcut-and-action-hints.md)を実装前に固定した。
規則: ARC-001/004/011、C-002/008/012/017/018、QLT-008/012/013、GIT-001〜004。Waivers none。

## 変更

Ctrl+Eは現在modeを反転し、既存EDIT/VIEW dispatcherへ渡す。編集→閲覧では既存の保存が成功した後だけ切り替わる。
Ctrl+Mは「操作」ボタンと同じメニューを開閉する。保存Ctrl+S・検索Ctrl+F・i・Escの意味は維持した。
新しい2キーの自動繰り返しをmessageのprevious-state bitで消費し、IME/native modal/設定/本文投入/終了中は実行しない。
ノート未選択では何もせず、一覧のみで非表示の操作メニューも開かない。

coreの同じshortcut記述子をHACCEL・tooltip・UIA・helpで使う。既存のaction検索を公開し、
mode状態・保存処理・文字列・メニューの別経路は持たない。両modeボタンは「閲覧・編集を切り替える (Ctrl+E)」、
保存/検索/操作も「説明 (キー)」を3言語で表示する。VIEW単独commandのキーとtoggleを混同しない。
保存schema・CMakeの版入力・依存・ゲートは不変。提出済みStore MSIXは再生成/差替え/再提出していない。

## 対象検証

原本は `D:/NeNeFolio/agents/mode-shortcuts-20261101/`、一時実行記録は
`D:/NeNeFolio/outputs/490-mode-shortcuts/`。各実行は`tools/record-run.py`でinput/artifactを保存した。
終了整理後の全hash保全先と最終CI/main/通常exe反映は同agentsの`integration-completion.json`で確定する。

| 実行 | 結果と範囲 |
| --- | --- |
| `record-build.ps1 -Label build-02` → `build.ps1` | conformance違反0。固定toolchainのDebug `cmake --build build --target nenefolio folio_tests`、clang-tidy成功 |
| `unit-01` → `unit.ps1` → `folio_tests.exe --commands-ui` | command/shortcut/文言の対象unit成功。重複binding、実行key/modifier/route、3言語のhelp、VIEW専用キーとの区別 |
| `record-probe.ps1 -Label ui-02 -Stage compile` | 正典compile_commandsのASan/UBSan/nullability・回復禁止・警告エラーを維持。正典libraries/resourceへlinkし、元folio_window objectの重複linkなし |
| `record-probe.ps1 -Label ui-02 -Stage run` | 88 checks、0 failures、fixture process exit0。実Win32/実RichEditと合成key/message、専用desktop |

88checksの対象は両方向、保存した実ファイル、保存失敗のnative通知、無題の初回保存取消、本文/Undo保持、
Ctrl+S/Ctrl+F/i/Esc、modifier違い、repeat抑止、メニューのanchor/先頭選択/再打鍵/Esc/focus復帰、
未選択/一覧のみ、4入力所有者のIME通知、modal/session/close/removal/disabled/settings、実stream中の再入抑止。
tooltip通知とUIAのキーは5部品×3言語で照合した。日本語light・英語狭幅・簡体字darkの標準tooltip画像を目視し、
説明と括弧付きキーの欠けが無いことを確認した。文字描画は実tooltip HWNDをtrack表示してPrintWindowで取得した。
未変更のレイアウトや保存実装を全件再試験しない。

## 途中で検出したもの

- `build-01`: 追加したtooltip分岐で認知的複雑度12、上限10のため失敗。部品ごとの説明の選択と、
  字の有無による既存formatの合成を分けた。抑制/閾値変更なし。失敗時に未生成のexe/libのartifact保存エラーも記録した。
- `run-ui-01`: 84checks中1失敗。既存Ctrl+Sの検査がnative `EM_GETMODIFY`の解除を誤って要求した。
  保存済み本文との差は既存`settle_save_state`/`folio_state_unsaved`が正本で、native flagの解除は契約ではない。
  実ファイルの保存は同試行でも成功。検査を正本の未保存印解除・編集mode/Undo保持に修正し、ui-02で成功した。製品の保存処理は変更していない。

## 残る境界

物理キー/物理IME候補入力、readerの実聴取、異なるDPI間移動やDWMの通常desktop上の呈示までは保証しない。
通常exeへの反映前は起動を確認し、hideが保存して閉じたことを2026-11-01 01:23に確認した。強制終了なし。
Store認定/公開待ちと#479/#466の保留は維持する。
