# テーマ一覧の候補移動とホイール — #469

- Issue: [#469](https://github.com/hideyukiMORI/nene-folio/issues/469)
- 基点: main `66523e36eabc9b064329a84b1b886c63d38fc7e5`
- 規則: ARC-001/004/008/011/012、C-008/012/013/016/017、QLT-008/010/012/013、GIT-001〜004。
- 方針: [ADR0061の補正](../adr/0061-theme-live-preview.md)。ADR0060/0063/0065の状態・配置・候補対応を維持。
- 原本: `D:/NeNeFolio/agents/theme-navigation-20261030/`。作業木/出力はD側の`469-theme-navigation`。

## 依頼と原因

hideの実機報告は、テーマ選択のホイールがおかしく、カーソル選択が直感的でないというもの。
NeNeNibの解決方法を読取参考にする依頼に従い、ADR0150、ListWindow.hpp、CommandPalette、
EditorControllerPalette、WheelTurn.hpp、EditorWindowPointerを確認し、入力hashを`research-inputs-v1.json`に残した。
NeNeNibの変更は0。この個別修正は、停止済みの品質改善goalや未完#466を再開するものではない。

Folioはプレビュー後の配置が`command_first`を0へ戻すため、下端へ送ったあと↑を押しても一覧ごと戻り、
候補枠が下端へ張り付いた。ホイールはdeltaの符号だけを上下キー一回へ変換し、一覧の独立スクロールも端数保持も無かった。

## 変更

製品変更は`src/ui/win32/folio_window.c`だけ。既存`command_first`と`command_selection`を正本として保つ。

- テーマの配置はfirstを維持する。選択が可視範囲内なら位置を保持し、外へ出たときだけ最小限寄せる。
- 候補数/可視行数が変われば末尾の有効範囲へ丸める。位置計算では可視0行を1行として扱う。
- ホイールは1刻み3行で一覧を送る。選択は可視範囲内なら保持、外へ出たときだけ端へ寄せて既存previewへ渡す。
- signed16bit deltaの3倍を累積し、1行未満の余りを保持する。逆向き・開き直し・検索変更で旧余りを持ち越さない。
- 既存の静止pointer抑止、IME、Enter/クリック/UIA Invokeの確定、Esc/戻る/×の取消、再着色と本文状態の経路を維持する。

公開API、保存schema、版、依存、確保、製品の試験口、ゲート変更なし。Waivers none。

## 修正前の再現

正典Debugの実compile_commands/Ninja/File APIからcompile/link vectorを派生した。
全folio_window.c bytesへ外部fixtureを付加し、製品内の置換/試験口は0。元archiveのwindow member非採用と単一export ownerを確認する。
ASan/UBSan/nullability・recover禁止・4runtimeを維持し、resource/残るlibraryも正典を使う。

`observe-baseline-01`は267checks/41fail、native1・record child1/wrapper1・記録error0。
下へ14回のあと↑へ戻す11回で期待firstとの不一致を再現し、通常/複数/小deltaのホイールにも不一致があった。
ただしv1の`narrow-up`には、1可視行でもfirstを保持する誤った期待が1件含まれた。これは製品欠陥に数えない。
v2で上端を跨ぐときのfirstへ訂正し、1行から10行へ戻すclamp条件とUIA/取消の確認を追加した。
旧fixture/画像/rawは保持し、旧製品を再実行していない。同一fixture bytesの単純な前後比較とは呼ばない。

## 対象検証

`record-debug-v1.ps1 -Stage Build -Attempt build-candidate-01`は、`scroll_theme_content`の認知的複雑度14が閾値10を超えて拒否された。
child1/wrapper125、未生成metadata/zipの記録error2。成功buildの証拠には使わない。
移動先の算術を`scrolled_theme_first`へ分離し、ゲートを変えずに修正した。
初回formatのPATH未設定は正典toolchain読込で訂正し、製品試験の失敗と分けた。

以下のコマンドは上記原本directoryのdriverを使用し、各結果を`tools/record-run.py`で記録した。

| 実行 | 結果 |
| --- | --- |
| `record-debug-v1.ps1 -Stage Build -Attempt build-candidate-02` | 正典Debug/tidy、3actions、child/wrapper0・記録error0 |
| `record-derived-v2.ps1 -Stage Inspect -Label candidate -Attempt inspect-candidate-01 -BuildAttempt build-candidate-02` | 正典vector/全製品bytes/派生sourceを固定、0/error0 |
| `record-derived-v2.ps1 -Stage Compile -Label candidate -Attempt compile-candidate-01` | 実vectorのcompile/linkとmap/nm、0/error0 |
| `run-native-v1.py prepare candidate --attempt observe-candidate-01` | 430checks/0fail、native/child/wrapper0・記録error0 |
| `audit-record-v1.py`による上記各recordの照合 | 全current/snapshot hash-size一致、入力after不変 |

入力/成果物数はBuild796/9、Inspect814/4、Compile817/13、Observe831/8。
Build metadata ZIP21entryを照合し、baseline/candidateの元compile vectorは同一、link vectorの差は派生出力先だけと確認した。
受理製品SHA256は`2d7885fa3bf0e89b31f2222146273410f3392277f3500a3be8f2441b54fe0338`。

専用の非入力desktopで実Win32部品へ合成messageを送り、通常dataへ触れず、SendInput/共有cursor操作は0。
通常960×640と最小320×360のlogical窓、実120DPI、128候補、固定合成本文を使用した。
上下/PgDn、端を越えた後の逆方向、末尾resize、0件/絞込、正負/0/小/複数刻み/最大signed delta、
検索EDIT/一覧の配送、静止MOVE、composition中の抑止、UIA bounds/selected/hit/Select/Invokeを確認した。
Enter/クリック確定、Esc/戻る/×取消、未保存本文/選択/dirty/実Redo→Undo、EDIT focusを保持した。
4枚の実GDI描画bitmapの候補枠pixelを判定し、親も画像を視認した。timeoutなし、所有process終了・desktop閉鎖済み。
物理ホイール/物理IME/実reader/DWM呈示の確認ではない。速度計測や一般UIの改善保証は行っていない。

## 統合・通常反映と整理

成功済み対象検証は関係入力不変で再利用し、最終headの必須CIだけで正典`eng/check.ps1`を要求する。
文書差分/参照/conformanceと最終head/CI/mainの確定値は同原本の`integration-completion.json`へ残す。
22:36の通常exeはPID12332で起動中だったため上書き・終了をせず、hideへ保存/終了を依頼した。
通常反映は正典`tools/prepare-real-machine.ps1`だけを使用し、反映可否/版/hashを同完了原本で確定する。
全成果物の保全と未保存/未追跡/ignored/唯一性/link/稼働参照の監査後に新作業物を整理し、branch/commitは保持する。
旧削除拒否52箇所と未完#466は操作しない。通常data/save、公開beta、他案件の変更なし。
