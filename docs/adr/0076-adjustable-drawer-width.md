# ADR 0076 — 一覧の再描画を保ち、実行中の幅を一つの境界操作で調整する

- 状態: 受理（#475の製品編集前に方針を固定。実装・対象検証・必須CIは別途記録する）
- 日付: 2026-10-31
- Issue: #475
- 規則: FR-013/039、ARC-001/002/004/005/008/011/012、C-002/008/012/013/016/017/018、QLT-008/012/013
- 補正対象: ADR0047/0056/0060。既存の表示切替・保存・本文・modalの契約を保つ。

## 根拠

hideは一覧のみの連続resizeで検索枠・追加/折畳みの古い描画が残ることを報告し、
幅変更を保って修復することと、同じ配置経路で両表示の境界ドラッグを加える方針を承認した。
main `8bde75c`の正典Debugから作った隔離fixtureでは、5回の幅/高さ変更すべてで自然更新と
強制全面更新の画素が異なった（361〜838画素）。各回の更新は部分領域だけで、旧検索枠・件数・＋/−が残る。
入力・exe・画像・終了値の原本は`D:/NeNeFolio/agents/drawer-resize-20261031/`と同taskのoutputsに保全する。

## 決定

1. 一覧のWM_SIZEで一覧client全体を無効化する。描画は既存の一枚のDIB→WM_PAINTだけとし、
   古い幅の絵を残して新しい部分だけ描く経路を廃する。自前の消去/resize描画、同期paint loopは足さない。
2. 選んだ一覧幅は`folio_window`がlogical pxで一つだけ所有する。初期240、一覧の最小200、本文領域の最小320。
   `regions_for`がclient/DPI/mode/選択幅から実幅を導く。窓縮小時は実幅だけを制限し、窓を広げると選択幅へ戻す。
   一覧のみ/ノートのみでは選択幅を使わず保持する。一覧のみの320への縮小と元の窓幅/最大化復元は既存のまま。
   settings schema、再起動をまたぐ保存、第二の本文/索引状態は加えない。
3. 両表示の境界の本文側8 logical pxを当たり領域にする。既存の本文側余白の内側なので、初期状態の本文位置は変えない。
   矩形は`folio_regions`の派生値として描画・hit・tooltip・UIAが共有する。表示は細線とhover/drag/focusの文字系色。
   フォーカスは同じkeyboard_target、押下は同じpressed_part、captureは主窓が所有する。mouse開始では本文focusを奪わない。
   カーソルも届いたmouse通知から更新した同じ押下/hoverを使う。共有pointer位置を読み直さず、子HWND/nonclientのcursorはOSへ渡す。
4. drag中は共通の幅適用→既存の`rearrange_keeping_line`へ渡す。既存RichEditの本文・選択・dirty・Undoと
   読んでいる論理行、applicationの索引cursor/scrollを保持する。releaseは窓外でも範囲内の最終幅を確定する。
   Escape/capture喪失/無効化/非active化/他操作/表示切替/親のresize/DPI変更では開始前の選択幅へ戻してcaptureを解放する。
   dragの開始幅とpointer offsetはその間だけ有効で、確定幅の別所有者にしない。
5. F6で一覧へ入り、Tabで境界へ届く。境界にfocusがあるとき、左右で4、PageUp/PageDownで32 logical px、
   Home/Endで現在の最小/最大へ動かす。IME・本文Tab・既存の表示キーを保ち、キー操作も同じ幅適用へ渡す。
   境界はボタンではないためEnter/Spaceで保存/表示切替等を実行しない。
6. UIAは既存host/node寿命へ幅調整要素を追加し、SliderとRangeValueを公開する。Name/HelpTextは3言語のui_text。
   Value/Minimum/Maximum/SmallChange/LargeChangeは同じ実幅と範囲から導く。単位はlogical pxで、非整数は最寄り整数へ丸める。
   非有限値と範囲外は拒否する。非表示では空bounds・offscreen・非focusable・無効とし、modal/IME/終了中は既存の可否へ従う。
   Value/範囲/可否/boundsの変化を既存snapshot通知から公開し、破棄後の保持参照は同じ失効契約で拒否する。

## 検証と限界

修正前と同じ画素比較、最大化/復元、dragの確定/取消/capture喪失/端、mode復帰と窓縮小時の選択幅保持を確認する。
未保存の長い本文の選択・実UndoRedo・focus・論理行と一覧位置、閲覧表のreflow、キー/UIAの範囲と保持参照を対象にする。
正典Debug/tidy・対象fixtureはrecord-runで記録し、成功済み不変入力は再利用する。フルゲートは最終headの必須CI。
専用desktopの合成入力/GDI画像と物理入力・実reader・DWM呈示を区別する。Waivers: none。

## 却下

- 幅を固定してresizeを禁止する: hideが承認した一覧の使い方を失う。
- resize中の別描画と別座標: 残像・入力・UIAの不一致を再び作る。
- 比率と画素幅を両方保存する: 窓縮小/復帰で正本が二つになる。
- 幅変更で本文を再投入する: 未保存本文とUndoの所有契約を壊す。
- 永続settingsを同時追加する: 要求された実行中の幅調整を越え、schema変更へ広がる。

## 参照

- [Microsoft RangeValue control pattern](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingrangevalue)
- [Microsoft Slider control type](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supportslidercontroltype)
- [Microsoft IRangeValueProvider::SetValue](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-irangevalueprovider-setvalue)
- [Microsoft WM_SETCURSOR](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor)
