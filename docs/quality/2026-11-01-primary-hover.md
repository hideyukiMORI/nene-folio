# 主操作ボタンのhover — #496

- 日付: 2026-11-01
- 規則: ARC-001/002/003/004/011/012、C-002/003/008/012/016/017、QLT-008/009/012/013、GIT-001〜004
- 設計: [ADR0060の補正](../adr/0060-ui-part-styles-and-state-rule.md)
- Waivers: none

## 変更と自己レビュー

`ui_part`のPRIMARYだけを変更した。未保存の保存、名前入力の確定、保存先の確定が同じ経路を使う。
hoverで下辺を文字色に塗り替える処理を廃止し、外周2 logical pxを残して面色を変える。
文字・絵の配置と当たり判定は変更しない。押下の細い内枠とkeyboard focusの2px枠は別の位置に描く。
disabledはhover/押下を出さず、A/C/S様式の既存描画も維持した。

色対は既存`theme_contrast`から純粋に導き、`folio_palette`でCOLORREFへ写す。
通常の塗りは文字4.5:1を保つ1/8混色、枠退避は枠色の面へ切り替える。
高コントラストはOSのhighlight/button/window対を崩さず、静止のhighlightと同じhoverにならない方を選ぶ。
固定theme表・保存schema・版入力・依存・ゲート基準は不変。新規の動的確保・状態所有者・OS設定読取はない。

## 対象検証

実行と入力snapshotの原本は`D:/NeNeFolio/outputs/496-primary-hover/`、
fixture/scriptと完了記録は`D:/NeNeFolio/agents/primary-hover-20261101/`。
すべて`tools/record-run.py`で入力・終了値・raw出力を記録した。

- `pwsh -NoProfile -File .../build.ps1`: 最終build-04。正典Debug/tidy、製品と単体exe、core/UI libを保管。
- `build/folio_tests.exe --themes`: unit-03。全127固定配色のhover文字4.5:1、外周3:1、
  既存focus役割3:1、塗り/枠退避とOS色対の両分岐を確認。既存theme/stateの同じ境界を使用した。
- `probe.ps1 compile paint-03` / `probe.ps1 run paint-03`: 正典compile/linkの警告・sanitizer vectorと実libを使い、
  実GDIで438 checks / 0 failures。明暗、cobalt2、tokyo-night-light、OS高コントラスト明暗、
  96/144/192 DPIの指定描画を確認。全外周・内側面色・文字矩形・群の区切り・無効・押下/focus併用と、
  隣接するCOMMAND/CHOICEの既存描画を検査した。6枚の状態比較画像を確認した。
- `probe.ps1 compile native-01 -Native` / `probe.ps1 run native-01 -Native`: 15 checks / 0 failures。
  専用desktop上の実`storage_prompt`で、同梱書体と指摘時のtidebound-goldを使用した。
  同じUI threadのWM_MOUSEMOVE/LEAVE、実BUTTONの共有hover状態、実WM_DRAWITEM、外周と面色、矩形不変を確認。
  「次回から使う」の静止/hover画像を目視した。取消で閉じ、保存先を確定せず、desktopを復元/解放した。
  input desktop切替・SendInput・通常ノートの操作は行っていない。
- 最後の静読でOSのbuttonとhighlightも同色の場合を補い、window対を使う分岐をunit-03へ加えた。
  描画と通常配色のコード、paint-03/native-01の各入力に対する出力色対は不変のため、両描画試験は再利用する。

## 途中で見つけた条件と記録上の失敗

- 最初のpaint-01は402項目成功だったが、画像でOS highlightを静止/hoverの両方へ使う同色化を検出した。
  可視差のassertionを加えたpaint-02は438項目中12失敗。OS button対への切替を実装し、paint-03で全成功した。
- build-02の製品buildは終了0だったが、記録側が生成物を入力へ含めたため変更検出で終了125となった。
  buildでは生成物をartifactとする指定へ直し、build-03で入力不変・記録成功を確認した。
- 外部Pythonからの初期run-storage-01/02はhover状態を得られず黒画像となった。01の子終了0も描画成功とは扱わない。
  02で明示assertionの失敗を記録し、以後は同じUI thread上のnative-01へ置き換えて確認した。

## 静的検査・統合・限界

最終の関係静的検査は`final-check.ps1`の記録、最終head必須CI・main・通常exe反映・起動と終了整理は
同agentsの`integration-completion.json`で確定する。統合には`eng/check.ps1`の最終head CI成功を要求する。
成功済み対象試験は関係入力が不変なら再利用する。全件回帰をローカルで追加しない。

任意の利用者設定OS色のコントラスト・全役割が同色対の場合の可視差、物理pointer/実reader/DWM呈示、異なるmonitor間の実DPI遷移はこの試験の保証外。
描画に与えたDPIと実monitor移動を区別する。申請済みStore MSIXの変更は含めない。
