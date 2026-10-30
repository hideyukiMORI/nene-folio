# ホイールを候補移動へ揃え、pointer通過では選ばない — #471

- Issue: [#471](https://github.com/hideyukiMORI/nene-folio/issues/471)、base main `53f71a8`。
- 規則: ARC-001/004/008/011/012、C-008/012/013/016/017、QLT-008/010/012/013、GIT-001〜004。
- 方針: [ADR0061の#471補正](../adr/0061-theme-live-preview.md)。ADR0060/0063/0065の既存経路を維持。
- 原本: `D:/NeNeFolio/agents/theme-selection-20261030/`。D側worktrees/outputsは`471-theme-selection`。

## hideの指摘と修正範囲

#469反映後、hideは「ホイールは一覧の独立スクロールではなく選択を移動すればよい」、
続けて「ポインタに当たる候補へフォーカスが行くことが問題かも」と指摘した。
#469の一覧位置保持は維持し、独立スクロールへ広げた判断を修正した。単体で実施し、#466とpaused継続goalは再開しない。

製品は`src/ui/win32/folio_window.c`だけ。key/Pageとwheelを`navigate_theme_selection`へ集め、
prepare/count判定→候補移動→reveal→既存previewの経路を一つにした。
wheelは1刻み1候補、小deltaは120未満を累積、複数刻みは対応する候補数を移動し上下端で止める。
逆方向、開き直し、検索変更で旧余りを捨てる。候補が見える間は一覧を保持し、端を跨ぐ時だけ追従する。

`preview_theme_pointer`と、それだけが読む座標/known履歴を削除した。
pointer通過だけでは選択・previewを変えず、button hover/押下、クリック選択・確定を維持する。
wheelの独立スクロールと、viewportに合わせて候補を端へ寄せる処理も削除した。
公開API/schema/版/依存/確保/ゲート変更なし。Waivers none。

## 同一fixtureによる修正前後

#469の外部fixtureを今回の操作契約へ適応し、実pointer座標を別行へ動かすケースを追加した。
修正前/後でfixture bytesと元compile vectorは同一、link vectorの差は派生出力先だけ。
全folio_window.c bytesへfixtureを付加し、元archive window member非採用・単一export ownerをmap/nmで確認した。
正典Debugのcompile_commands/Ninja/File APIから派生し、ASan/UBSan/nullability・recover禁止・4runtimeを維持する。

| 実行 | 結果 |
| --- | --- |
| `record-debug-v1.ps1 -Stage Build -Attempt build-baseline-01` | 正典Debug/tidy、0/error0 |
| baselineのInspect/Compileと`run-native-v1.py prepare baseline --attempt observe-baseline-01` | 513checks/48fail、native/child/wrapper1・記録error0 |
| `record-debug-v1.ps1 -Stage Build -Attempt build-candidate-01` | 正典Debug/tidy、3actions、0/error0 |
| `record-derived-v2.ps1 -Stage Inspect -Label candidate -Attempt inspect-candidate-01 -BuildAttempt build-candidate-01` | 0/error0 |
| `record-derived-v2.ps1 -Stage Compile -Label candidate -Attempt compile-candidate-01` | 0/error0 |
| `run-native-v1.py prepare candidate --attempt observe-candidate-01` | 513checks/0fail、native/child/wrapper0・記録error0 |

driverは上記原本directoryに置き、全stageを`tools/record-run.py`で保全した。
各`audit-record-v1.py`照合で全current/snapshot hash-size一致・input after不変を確認。
candidateのinput/artifact数はBuild797/9、Inspect815/4、Compile818/13、Observe832/8。Build metadata ZIP21entry一致。
受理製品SHA256は`f05b89e18cce2ee16ee7d1831587a958669ed7d3375cec45044e94682ff9c92d`。

旧版ではpointerの実座標変化だけでselected0→6、wheel後の別行へのMOVEでselected5への上書きを観測した。
候補移動を期待したwheelもfirstを3へ送って不一致となった。新しい版はpointer前後でselected0を保持し、
wheel一刻みでselected1/first0、続く実MOVE/静止MOVEでもselected1/first0を保持した。
不一致48件は固定fixtureの判定数であり、独立した48個の欠陥とは数えない。

上下/Page両方向、端を跨いだ後の逆方向、末尾resize、0件/絞込、正負/0/小/複数刻み/最大signed delta、
途中位置での逆方向余り破棄、検索/開閉reset、EDIT/一覧の配送、IME抑止を確認した。
UIA公開bounds/selected/hit/Select/Invoke、Enter/クリック確定、Esc/戻る/×取消、
未保存本文/選択/dirty/実Redo→Undo、EDIT focus、4枚のGDI描画pixelと視認も確認した。

専用非入力desktop、960×640/320×360 logical窓、実120DPI、128候補と固定合成本文を使用。
SendInput/共有cursor操作なし。timeoutなし、所有process終了・desktop閉鎖済み。
物理wheel/IME、実reader、DWM呈示の確認ではなく、速度の保証もしない。

## 統合・反映・保全

文書参照/format/diff/conformanceと最終headの必須CIを確認し、成功済み対象実行は再実行しない。
通常exeは23:10にprocess0を確認。反映直前も再確認し、正典`tools/prepare-real-machine.ps1`を使用する。
head/CI/main・通常exe版/hash・反映後の起動・全成果物保全と整理は同原本の`integration-completion.json`へ確定する。
旧削除拒否53箇所、未完#466、通常data/save、公開beta、NeNeNibを含む他案件は操作しない。
作業木/不要出力は統合・反映・全保全・未保存/ignored/唯一性/link/稼働参照監査後に整理し、branch/commitを保持する。
