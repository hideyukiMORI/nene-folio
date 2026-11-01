# 有効な操作対象の指先ポインタ — #498

- 日付: 2026-11-01
- 規則: ARC-001/002/004/007/011/012、C-002/008/012/016/017、QLT-008/012/013、GIT-001〜004
- 設計: [ADR0083](../adr/0083-action-pointer-feedback.md)。ADR0060/0076を補正。
- Waivers: none

## 変更と自己レビュー

主窓の有効なボタン・表示/モード選択・窓操作、drawerの追加/ノート/開閉可能なカテゴリ、
補助面のボタン/タブ/選択肢、検索EDITの消去×、自前native面のBUTTON/カテゴリLISTBOXを指先に揃えた。
選択済み項目も有効なら指先。見出し・説明・余白・暗幕・無効項目・filteredカテゴリは矢印。
文字入力/選択、splitter/窓縁、並び替えdragは元の操作に対応する形を保つ。

`ui_pointer`がOS cursorの表示・WM_SETCURSORの所有者/client境界・通知位置からの再評価だけを担う。
各HWNDの既存hit/描画/UIA矩形と可否を利用し、業務状態やクリック/保存/選択を複製しない。
WM_MOUSEMOVEは通知座標、その他はGetMessagePosを使い、共有pointerを移動/読取するAPIやtimerを追加しない。
状態更新後は同じrootに属する実hit HWNDだけを引き直す。nonclientと子EDIT/RichEditを親で上書きしない。

nativeの既存BUTTON subclassをLISTBOXにも共用し、寿命は子HWND、解放はWM_NCDESTROY。
名前/保存先入力のIME可否は各dialogの既存composingから答える。Webリンクは既存の起動可否を共有し、
cursor通知だけはRichEdit capture中にI字を返す。クリックのgesture/URI検証/portは不変。
検索EDITは自分の既存入力門を使い、IME中も入力欄のI字と実際に使える消去×の指先を保つ。
schema・版入力・配色・依存・品質ゲート・申請済みStore MSIXは変更しない。

## 対象検証

script/fixtureと完了記録は`D:/NeNeFolio/agents/hand-pointer-20261101/`、
実行原本は`D:/NeNeFolio/outputs/498-hand-pointer/`。`tools/record-run.py`で入力snapshot、hash、raw出力、終了値を記録した。

- `build.ps1`（build-06）: 正典Debug/tidy成功。抑制・除外・警告の緩和なし。
- `probe.ps1 compile ui-06` / `probe.ps1 run ui-06`: **111 checks / 0 failures**。
  主窓14操作、選択済み項目、splitter capture/取消、子/nonclient境界、modal/session、drawerの行/blank/drag、
  SEARCH帯を開いたままの行hover/実click、filteredカテゴリ/ノート、検索EDITの実選択captureと消去を確認。
  検索/置換/設定/テーマ/パレット/操作menuのボタン・見出し・有効/無効・scrollを確認した。
  dirty/Undo/本文選択/保存済みMarkdownを保持し、cursor通知だけではbrowserを起動しない。
- `probe.ps1 compile native-09 -Native` / `probe.ps1 run native-09 -Native`: **92 checks / 0 failures**。
  実`name_prompt`/`storage_prompt`/`failure_box`のBUTTONとLISTBOX、client外/blank/有効化/leave、
  実WM_SETCURSORを確認。名前/保存先のIME開始/終了では、pointerを動かさず矢印/指先へ切り替わった。
  各面を取消で閉じ、ノートや保存先を確定しない。リンクmapに`folio_window.c.obj`は入っていない。
- `probe.ps1 compile ui-11` / `probe.ps1 run ui-11 -Extras`: **12 checks / 0 failures**。
  RichEdit実HWNDのEN_LINK cursor通知、実captureの有無、IME、SEARCH帯、主窓の静止pointer更新を確認。
  文字選択capture時の1 redをcursor通知の条件で修正し、同じ境界がgreenになった。
- `probe.ps1 compile ui-12` / `probe.ps1 run ui-12 -Remaining`: **12 checks / 0 failures**。
  実保存履歴の選択行、履歴操作、パレット候補/群見出し/別tab/IME、設定暗幕を補完した。
- `probe.ps1 compile ui-14` / `probe.ps1 run ui-14 -FilterIme`: **13 checks / 0 failures**。
  検索EDITのIME中の入力/消去と実click、確定後の反映、modal/session/無効化の境界を確認。
  ui-13で文字入力と消去×の2 redを記録し、同じ入力門を使う表示へ修正した。
- `final-check.ps1`（final-check-02）: conformance/File API、core/applicationのsymbol境界、変更C/Hのclang-format、git diffを確認する。
  最終結果は同agentsの`pre-integration-audit.json`へ固定する。

fixtureは正典のcompile/link vectorとASan/UBSan/nullabilityを保持し、製品libへ結ぶ。
主窓fixtureだけは製品`folio_window.c`の無改変bytesへ試験コードを連結し、元objectの重複linkを拒否する。
後段の製品補正はEN_LINK cursor通知のcapture条件と検索EDITの入力可否だけ。
ui-06の非capture/非IME時、ui-11/12の検索EDIT外の結果と、folio_window objectを使わないnative-09は
影響を受けないため再利用した。最終差分は入力snapshotとの機械照合でこの2箇所に限定した。合計240項目。

## 途中の失敗と範囲

独立静読で検索EDITの選択captureと、SEARCH帯が開いたときのdrawer可否を指摘され、修正後に実入力で確認した。
build-01/02は複雑度・引数数・既存関数名との衝突で失敗し、役割を分けて解消。
build-03は製品build成功でも、記録対象のfixture driverを実行中に更新して記録側が失敗した。成功扱いせずbuild-04/05で確定した。
初期fixtureの不足include/古いport初期化/引数渡しは検証側の構築失敗として保存した。

ui-05の2失敗は、破棄されるWM_MOUSEMOVEだけでsession cursorを検査したことと、行だけのmenuにbuttonを期待したことによる。
実WM_SETCURSORとmenu行の有効/無効に直してui-06で確認した。
native初期実行はCOM初期化不足、およびmodal ownerの範囲/DPIによる再配置で狙った座標に入らず失敗した。
COMを初期化し、ownerとdialogを同じ通知位置へ整列してnative-09で確認。製品の配置処理は変更していない。
背景desktopでRichEditへWM_SETCURSORだけを送る試験はネイティブのEN_LINK発行を再現できず、
実HWNDの通知/capture境界を明示するui-10/11へ置き換えた。そのui-10のcapture時1失敗は製品側で修正した。
最終静読で検索EDITのIME入力とポインタの不一致を見つけ、PRをDraftへ戻して旧headのCIを取消した。
ui-13/14のred/green後に最終headを確定し、そのheadの必須CIを改めて要求する。

専用desktop上の同一UI threadに合成通知を送り、通常ノート・input desktop切替・SendInput・OS標準pickerには触れていない。
物理pointerによるRichEditの通知発行、実reader聴取、DWM実画面呈示、全monitor間のDPI遷移を保証する試験とは区別する。
最終headの必須`eng/check.ps1` CI、main、通常exe反映/起動、全保全と終了整理は
同agentsの`integration-completion.json`で確定する。
