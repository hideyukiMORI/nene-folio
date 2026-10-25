# ADR 0066 — 無効な本文の背景も実効テーマで描く

- 状態: 受理（#377の実装前判断）
- 日付: 2026-10-25
- Issue: [#377](https://github.com/hideyukiMORI/nene-folio/issues/377)
- 規則: ARC-001/004/005/008/011/012、C-002/005/008/012/013/016/017、QLT-008/012/013
- 補正: ADR0064の内容無効化を維持し、disabled時の背景表示だけを明確化する。

## 根拠

設定/テーマ/nativeモーダルを開いた際、EDITはWM_CTLCOLORSTATIC、RichEditは内部のdisabled背景描画へ切り替わる。
前者の通知が未処理で、後者はEM_SETBKGNDCOLORの再指定でもシステム面色になる。
hideの実機画像と独立Win32診断で、テーマRGB45/53/59から240/240/240への変化を確認した。

親のみdisableは子へのSetFocusが成功し、native UIAもenabledとなる。WM_ENABLEのnative処理を省く案は
OSのWS_DISABLEDとMSAAを保つが、native UIAのIsEnabledがtrueとなる。両案は採らない。
SES_CUSTOMLOOK/SES_EX_NOTHEMING/SES_EXTENDBACKCOLORも背景変化を防がなかった。

native WM_ENABLEを通した後にWS_EX_TRANSPARENTをdisabledの間だけ設定すると、RichEditは文字を標準経路で描き、
背景は描かない。既存subclassがその地を塗ればテーマ色とnativeの無効状態が両立することを診断で確認した。
専用desktopの診断であり、製品全体の合格や物理IMEの証明ではない。
証跡: `D:/NeNeFolio/agents/modal-theme-fix-20261025`。

## 決定

1. EnableWindowとWM_ENABLE/WM_CANCELMODE/フォーカス通知は既存native経路のままにする。
   WM_ENABLEをoriginalへ渡した後、note_paneの既存subclassがWS_EX_TRANSPARENTだけを無効時に付け、有効時に外す。
   WS_DISABLEDを直接書き換えず、親子関係・UIA provider・IME・本文操作guardを変更しない。
2. note_paneが保持する背景色は実効paletteから渡された描画用の値であり、本文や業務状態ではない。
   作成と再着色の既存合流点でEM_SETBKGNDCOLORと一緒に更新する。再着色の成功/失敗・Undo復帰契約は維持する。
3. 無効な本文の背景は同じsubclassのWM_ERASEBKGNDで塗る。WM_PRINTCLIENTも同じ地を用意してからoriginalに文字を描かせる。
   WM_PAINT/文字/選択/スクロール/IMEはRichEditが所有し、独自の本文renderer、保存bitmap、TxDraw経路を足さない。
   表示切替と背景色更新ではerase付きで無効化する。無効中のWM_PAINTに既存更新領域がある場合だけ、本文client全体をerase付きに広げてnativeへ渡す。
   部分的な背景消去ではnativeが再使用する選択内の文字が欠けることを実測したためである。更新のないpaintで新たな描画を予約しない。
   無効中の部分再描画・resize・再着色・印刷通知を対象検証する。
   有効時は透明styleを外し、元のnative背景/編集描画へ戻す。
4. EDITのWM_CTLCOLOREDITとWM_CTLCOLORSTATICはfolio_windowの一つの色指定関数へ合流させる。
   対象外のcontrolは受信したmessageとHWNDのDefWindowProcへ戻し、異なるmessageへ読み替えない。
5. 資源失敗、モーダルの入れ子/取消、テーマpreview/取消、既存readonly/dirty/Undo/選択は変更しない。
   nativeのdisabled状態はプログラムによる文書書換えAPI全体の禁止を意味しない。独立probeでは通常のdisabled
   RichEditでもUIA Value.SetValueが成功する。今回この既存native挙動を新たな保証として扱わない。

## 検証

製品の専用desktop fixtureで通常/設定/テーマ/native名前面の画素と画像、disabled/復帰のnativeUIA・MSAA・focus、
閲覧/編集の本文・選択・dirty・Undo、テーマpreview/取消、resize/部分paintを確認する。
完成時に対象build/規約検査、最終PR headでeng/check.ps1の成功を必要とする。
保存schema・依存・ゲート変更なし。Waivers: none。

## 一次資料

- [WM_ENABLE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-enable): 通知時点でWS_DISABLEDは更新済み。
- [EnableWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enablewindow)
- [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
- [WM_ERASEBKGND](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-erasebkgnd)
- [WM_PRINTCLIENT](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-printclient)
- [WM_CTLCOLORSTATIC](https://learn.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorstatic)
