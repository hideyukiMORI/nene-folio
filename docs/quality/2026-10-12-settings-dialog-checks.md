# #286 — 中央の設定ダイアログの確認

- 日付: 2026-10-12
- 基点: main `d2e9504`、作業枝`feat/286-settings-dialog`
- 要件: FR-013/014/016/021/022/024、[ADR0046](../adr/0046-distinct-settings-dialog.md)
- 規則: ARC-001/002/004/011、C-002/018、QLT-008/012/013
- 証跡: `D:/NeNeFolio/agents/ui-settings-dialog-20261012/`
- Waivers: none

## 対象とレビュー

設定だけの矩形、暗幕、項目の配置とクリック/キー操作、固定見出し/footer、閉じた後の編集状態を対象とする。
設定値の保存・採用、テーマ変更時のRichEdit復元は既存経路を維持し、schemaは版3のまま。
Astra設計席が完成差分を読み、項目表と描画/hitの共用、暗幕の所有・破棄、閉じる際のフォーカス復帰を確認した。
選択印の24px正方形、項目group/position、footerの高さ、補助文の専用色を実測前に調整した。
最終headの正典フルゲートは必須CIへ集約し、成功済みの無関係な回帰はローカルで繰り返さない。

## ビルドと規約

入力は未コミットの完成候補。`candidate-inputs-region.json`にsource・tests・eng・CMake・font等の入力hashを記録した。
後続の文書・commit・統合で同じ入力集合/hashを照合し、対象結果を再利用する。

| 確認 | 実コマンド | 結果 |
| --- | --- | --- |
| 整形 | 固定toolchainの`clang-format --dry-run --Werror --style=file`を変更6 C/headerへ実行 | exit 0。最終変更の`folio_window.c`は修正後に再確認。他5ファイルの成功を再利用 |
| Debug対象build | `cmake --build D:/NeNeFolio/outputs/286-settings-dialog/candidate-20261012-1823/build --target nenefolio --verbose` | 最終region修正後exit 0。コンパイラとclang-tidyを含む。`check-region-console.log` |
| 規約 | `python eng/conformance.py --root D:/NeNeFolio/worktrees/286-settings-dialog --build-dir D:/NeNeFolio/outputs/286-settings-dialog/candidate-20261012-1823/build --today 2026-10-12` | exit 0、0違反。`check-graph-console.log` |
| 差分 | `git diff --check` | exit 0（規約確認と同じscript） |

初回buildは暗幕のWM_PAINT内の分岐がC-012の深さ3を超えたため失敗した。
描画helperへ責務を分け、対象ファイルだけの増分buildで復帰した。閾値や抑制は変更していない。
続く規約確認は新規buildのCMake File API query不足でARC-002を1件報告したため、queryを追加してconfigureと規約確認だけを再実行した。
製品を再buildする必要はなかった。初回ログも証跡に保持する。

## 実OSの表示と操作

Windows build 26300、120 DPI（125%）。通常窓1000×750、最小窓700×450物理px（560×360 logical px）。
候補exeのSHA256は`A50635DF1231751826E3CDABF640C4E99C0699736C3956CB6FE64E17CE3394CA`。
作業専用の新規dataとPID/HWNDだけを操作し、前面windowは変えない。hideの起動中アプリは操作していない。

実コマンドは`pwsh -NoProfile -File D:/NeNeFolio/agents/ui-settings-dialog-20261012/probe-settings.ps1`、
残りの確認は同scriptへ`-Runtime D:/NeNeFolio/outputs/286-settings-dialog/candidate-20261012-1823/runtime-remaining -Remaining`を指定した。
標準の撮影は`tools/capture-window.ps1 -TargetPid <所有PID> -Title 'NeNe Folio' -Out <証跡PNG>`。
helperは合成入力と観測だけで、製品の別経路を作らない。

| 境界 | 確認結果と証跡 |
| --- | --- |
| 通常窓の面・文言 | 日本語/英語/简体中文×明暗6画像。`final-*-normal.png`。暗幕修正後はカードと文字が本来の色で、周囲だけが暗い |
| 開閉と背後操作 | 本文hash/長さ・modify・canUndo・selection・最初の論理可視行が一致。背後hitが暗幕を指し、click/wheel後も一致。Esc後は面/暗幕が隠れ本文focusへ戻る |
| 設定採用 | テーマ/言語のclick、方向キーのgroup/column移動、Tabの循環、Enter採用、Spaceで行番号ON/OFF。実theme採用後もeditor snapshotが一致 |
| 最小窓 | `remaining-*-minimum.png`等。3言語×明暗を確認。固定header/footerを保ち、選択は内容scrollで可視化。上端へ出た見出しのclipは内容scrollの結果で、wheelで戻せる |
| 失敗と再試行 | 作業用settings.jsonを排他lockし、設定値が不変、理由を固定footerで読める。lock解除後に同操作が成功。`remaining-ja-light-minimum-save-failure.png` |
| 閉じるとUndo | ×で暗幕を隠し本文focusへ復帰。設定操作前のDIRTY挿入が残り、EM_UNDOでその挿入が消える。owned appはexit 0、stderrは空 |

`gui-final-results.json`は製品境界41件成功後、helperの期待列誤りで中断した。
NUMBERからTab→SYSTEM→RightはLIGHTだが、DARKを期待していた。製品source変更はなく、
成功済みの通常matrixを再実行せず、残りだけを`gui-remaining-results.json`で全件成功まで確認した。
したがって最初のscript全体を成功とは表記しない。

初回は開閉と背景操作の遮断で本文・modify・canUndo・selection・最初の論理可視行が一致し、閉じた後の本文focus復帰も成功した。
ただし画像で設定カード自体の色が半減した。診断では内点hitと最上childがともに`NeNeFolioCommandLayer`で、
Z順に問題がないのに明るいカードの単独PrintWindowでも画素が127/125/126（本来255/250/253）だった。
このため暗幕regionから設定矩形を除外する修正を行った。`dimmer-diagnostic.json`と単独/全体画像が根拠。
300ms後の設定読取は採用処理の完了前だったため、操作helperは期待値の到達を待つよう修正した。

## 統合と反映

最終headの必須CI、main、通常exe反映、追加作業木と一時出力の整理は、紐付くPRと
`D:/NeNeFolio/agents/ui-settings-dialog-20261012/completion.json`で確認する。未実施を成功としない。

## 確認の限界

実OS確認は記録したWindows/DPIと合成した操作に限る。物理IME、別OS/DPI、過去のhide目視項目の受理を意味しない。
後続のカテゴリ追加と3表示モードは、この設定変更の確認には含めない。
