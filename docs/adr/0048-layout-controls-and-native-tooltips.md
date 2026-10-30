# ADR 0048 — 表示3択は一群として描き、直接キーと標準tooltipを同じコマンドへ結ぶ

- 状態: 受理（2026-10-12、hideの反映指示に基づき設計・独立レビュー・対象実OS確認を受理）
- 日付: 2026-10-12
- Issue: #292
- 規則: ARC-001 / ARC-002 / ARC-004 / ARC-007 / ARC-011、C-002 / C-018、CNF-010 / CNF-011、QLT-008 / QLT-012 / QLT-013
- 関連: ADR0016（共通コマンド）、ADR0030/0032（文言・言語）、ADR0033（OS描画部品）、ADR0046（設定モーダル）、ADR0047（3表示）

## 文脈

hideはADR0047の一覧／両方／ノートを実機で受理し、直接キーと右上の視覚表現の改善を求めた。
GUI設計・視覚伝達の2つの専門家ペルソナによる独立レビューは、右上配置と短い文字を保ち、3択だけを一群にする案で一致した。
これは実在の専門家への取材や利用者テストではなく、既存画像・コードと公開一次資料に基づく設計判断である。
現行のUIにはtooltip実装が無く、独自の遅延・追跡・popup実装を増やす必要はない。

## 決定

1. 右上に一覧／両方／ノートの文字を残し、3つを共通の薄い枠で囲む。選択中は既存の塗りと下線で示す。
   表示群と設定の間は12 logical pxを基本とし、設定と閉じるを表示群へ含めない。閲覧／編集は本文側に保つ。
   各区画54、内側間隔0、枠の余白2を基本とする。320幅でも左に72程度を残し、resize帯を除いた64程度の移動領域を保つ。
   『表示』の補助見出しとブランドは実幅が収まる広い窓でだけ描き、必須3択を省略しない。描画/hit/tooltipは同じ矩形を引く。
2. Ctrl+Shift+1 / 2 / 3を一覧／両方／ノートへ割り当てる。既存HACCELから共通dispatcherの既存3コマンドへ合流する。
   Ctrl+1/2はRichEditの標準行間操作、Ctrl+AltはAltGrと重なり得るため採用しない。数字は主キーボード列の仮想キーを使う。
   新キーからも保存・本文再投入・幅復元の別実装を行わず、ADR0047の本文/dirty/Undo/選択/論理行保持を維持する。
3. IME変換中・名前入力・削除確認・設定モーダル中は新しい表示キーを実行しない。既存の他のキー契約は変更しない。
   索引の主窓も変換開始/終了を追跡し、入力欄と本文を含む各所有者の変換状態を実行前に確認する。主窓の通常IME処理は維持する。
   通常の索引・本文・Ex/パレット/検索/置換/絞込からは既存の表示コマンドと同じ一時入力面の終了、可視focus復帰を行う。
   3択自体への新しいTab順・スクリーンリーダー公開は本変更へ混ぜず、直接キーと既存パレットの操作入口を追加する。
4. ボタンの短い文字とコマンドの説明を区別する。コマンドは『一覧のみ表示』『一覧とノートを表示』『ノートのみ表示』とし、
   tooltipにはキーを添え、一覧だけは窓を細くすることも補う。操作メニュー・パレット・F1のキー説明にもキーを示す。
   文言とキー表記はui_textのja/en/zh-Hansが所有し、既存layout_buttonsに対応する表示metadataから各入口へ渡す。
   句の合成は既存ui_text_formatを使う。狭いパレットでは該当3行だけ既存32高を上下2行に分けて説明とキーを描き、別名欄の幅を予約しない。
   Ex別名・検索・実行・行のhit/scrollは既存catalogのままにする。単なる言語切替でキー割当を変えない。
5. tooltipの唯一の経路としてWin32標準Tooltip controlを採用する。ui_win32のplatformLibrariesと正典CMakeリンクへOS同梱comctl32を追加する。
   外部runtimeDependencies、manifestのCommon Controls v6指定、独自tooltip timer/window描画は追加しない。OS同梱DLLの扱いはADR0028/0033と同じ。
   TOOLINFOWのcbSizeはSDK V2形式の末尾lParamまでとする。v6を指定しない実OSのcomctl32 5.82はsizeofの72バイトを拒否し、互換形式の64バイトを受理した。
   SDKマクロのNULLメンバー式は実行時sanitizerが拒否し、C23 constexprにも使えないため、標準offsetof(TOOLINFOW,lParam)+sizeof(LPARAM)でコンパイル時に確定する。
   数値を直書きせず、登録と矩形更新は一つのconstexprを共有する。SDK V2実測値との一致も外部診断で確認する。
   主窓がtooltip HWNDと必要な固定UTF-16 bufferを所有する。矩形toolをTTF_SUBCLASSで登録し、TTN_GETDISPINFOWで最新言語を解決する。
   resize/DPIではTTM_NEWTOOLRECTを更新し、既存のテーマ/フォント更新に合わせる。hoverはfocusを奪わない。
   入力モーダルや設定を開くとpopupを閉じて無効化し、閉じた後は既存の可視UIに合わせて戻す。生成失敗と破棄は主窓の既存結果/寿命へ合流する。

## 強制と確認

既存の依存グラフ、文言の3言語網羅、閉じたコマンド、format/tidy/conformanceを維持する。除外・抑制・waiverは追加しない。
完成候補で対象unit、3キーのルーティングとIME/modal抑止、未保存本文とUndo保持、tooltipの文字/矩形/非活性化、
3言語の320幅・ライト/ダークの表示を確認する。変更の無いADR0047の最大化/幅復元は以前の検証を再利用する。
OSの個別言語切替設定・物理IME実入力・スクリーンリーダー等、未測定の条件は対象品質記録へ明示する。
最終headのフルゲートは必須CIに集約する。

## 結果

3表示の意味と直接操作を発見しやすくし、本文を扱う操作と画面配置を区別できる。新たにOS同梱comctl32を使用する。
保存schema版3、モードの実行中だけの寿命、既存の幅復元・本文保持・公開済みReleaseは変わらない。

## 参照

- [Microsoft Radio buttons](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/radio-button)
- [Microsoft Keyboard accelerators](https://learn.microsoft.com/en-us/windows/apps/develop/input/keyboard-accelerators)
- [Microsoft Rich Edit shortcut keys](https://learn.microsoft.com/en-us/windows/win32/controls/about-rich-edit-controls#rich-edit-shortcut-keys)
- [Microsoft About Tooltip Controls](https://learn.microsoft.com/en-us/windows/win32/controls/tooltip-controls)
- [Microsoft InitCommonControlsEx](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-initcommoncontrolsex)
- [W3C Non-text Contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html)

## 2026-10-18 の補正（#344）

[ADR0059](0059-common-new-note-and-content-alignment.md)で新規の常設入口を共通ヘッダー左へ集約する。
狭幅では同じ位置の図形へ縮め、24 logical px以上の移動領域を残す。本文操作列は固定36pxへ揃える。
UIAの新規の親は共通ヘッダー、F6のヘッダー入口は新規、索引入口は検索欄とする。
検索focusの保存は#342/ADR0024、作成・取消・成功後の表示変更はADR0050/0051の契約を維持する。

## 2026-10-18 の補正（#348）

[ADR0060](0060-ui-part-styles-and-state-rule.md)で決定1の区画54・余白2を区画56・余白4へ、表示群と設定の間12を群間16（一覧のみの320幅では8）へ補正する。
決定3の「3択自体への新しいTab順を混ぜない」は、Tabが群に1回だけ止まり矢印でfocusだけを動かす形で補正する。
tooltipは全操作部品に付け、字付きの釦はキーだけ、絵だけの部品は名前＋キーとする。tooltipの経路（OS同梱comctl32）は変えない。

## 2026-10-30の補正（#452）

[ADR0075](0075-window-shell-controls-and-layout-chooser.md)で次を補正する。
表示3択が移動24を保てない幅では、同じ表示slotを現在modeのiconとchevronを持つchooserへする。
表示command/shortcutは既存metadataを使い、tooltip/UIA Nameは現在modeを含める。特定modeのshortcutをchooser自身のキーとして表示しない。
