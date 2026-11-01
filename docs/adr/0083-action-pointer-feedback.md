# ADR 0083 — 有効な操作対象の指先ポインタを既存のhitと可否から導く

Status: accepted / 2026-11-01 / Issue #498

## 背景

hideはGUI専門ペルソナとの読取レビューを受け、有効な操作部品を指先へ揃える方針を全体へ実装するよう指定した。
VS Codeのbutton/actionbar/listは有効項目にpointer、無効部品にdefaultを使う。一方Windowsの一般ガイドは
矢印を推奨している。本決定はNeNe Folio内の操作方針であり、Windows全体の標準だとは扱わない。

## 決定

- 主窓の有効ボタン・選択群・窓操作、drawerの追加/ノート/開閉可能なカテゴリ、補助面のボタン/タブ/
  操作・設定・テーマ・履歴の選択肢、検索EDITの消去×、自前native面のBUTTON/カテゴリ一覧を指先にする。
  選択済みと無効を混同せず、enabledの選択済み項目も指先とする。
- 説明/見出し/余白/暗幕/無効操作と、絞り込み中のカテゴリ見出しは矢印。入力・選択可能な本文はnativeの
  文字ポインタ、splitter/窓縁はresize、並び替えdragは既存の矢印と挿入線を保つ。OS標準picker/色選択はOSへ委ねる。
  閲覧WebリンクはADR0079のfield/既存起動可否と一致させ、本文選択・編集・IME・modalでは手形を強制しない。
- 指先の範囲は既存の描画/クリック/UIAの矩形と可否を使う。ポインタだけの候補列・業務状態・別のクリック処理を作らない。
  `ui_pointer`はOS標準cursorへの投影、WM_SETCURSORの所有HWND/client確認、通知位置からの再評価だけを共有する。
  各面のhit/可否は各所有者の既存経路に残す。parentが子EDIT/RichEdit/scrollbarのcursorを横取りしない。
- ADR0076の通知由来の座標方針を維持し、WM_MOUSEMOVEでは通知座標、WM_SETCURSOR/状態変更後はGetMessagePosの
  最後の取得messageの位置を使う。GetCursorPos/SetCursorPosで共有pointerを読取/移動せず、timerや別threadは追加しない。
  再評価は同じrootに属する実hit HWNDだけへ送り、nonclientのhit結果を保持する。別アプリやOSの面へ適用しない。
- leave/capture取消、enable、面の開閉、鍵操作、scroll/再描画で同じ通知位置の対象を引き直す。
  native BUTTONの既存subclassを選択LISTBOXにも共用し、blank領域とscrollbarを選択行に含めない。
  資源の寿命は子HWND、破棄はWM_NCDESTROY。dialog managerの既定釦/key/click/styleは変更しない。
- 既存hover色/枠、tooltip、keyboard focusを併用する。schema・版・配色表・依存・品質ゲートを変更しない。
  本決定はADR0060の操作部品の状態表示とADR0076のpointer表示を補正し、保存/選択/dragの意味は維持する。

## 検証

正典Debug/tidyと関係静的検査、隔離Win32 fixtureで各所有面のcursor、client/child/nonclient境界、
有効/無効/選択済み/filtered/IME/modal/capture/scroll/面切替を確認する。入力本文・選択・dirtyを操作表示だけで変えない。
物理pointer移動、実reader聴取、異なるmonitorのDPI遷移とは区別する。最終headの必須CIを要求する。Waivers none。

## 参照

- [VS Code button](https://github.com/microsoft/vscode/blob/main/src/vs/base/browser/ui/button/button.css)
- [VS Code actionbar](https://github.com/microsoft/vscode/blob/main/src/vs/base/browser/ui/actionbar/actionbar.css)
- [VS Code list](https://github.com/microsoft/vscode/blob/main/src/vs/base/browser/ui/list/list.css)
- [Windows mouse interactions](https://learn.microsoft.com/en-us/windows/apps/develop/input/mouse-interactions)
- [WM_SETCURSOR](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor)
- [GetMessagePos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagepos)
- [EN_LINK](https://learn.microsoft.com/en-us/windows/win32/controls/en-link)
