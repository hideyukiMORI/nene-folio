# ADR 0047 — 一覧・両方・ノートを一つの状態で切り替え、一覧では窓幅も縮める

- 状態: 受理（実装前に設計を固定し、対象検証と独立レビューを受理）
- 日付: 2026-10-12
- Issue: #290
- 規則: FR-013/016/018/024/039、ARC-001/002/004/011、C-002/012/018、QLT-008/012/013
- 関連: [ADR0011](0011-frameless-client-min-size-and-breadcrumb-ellipsis.md)、[ADR0013](0013-index-focus-vim-keys-and-persistent-mode.md)、[ADR0016](0016-command-catalog-and-shared-execution.md)、[ADR0044](0044-tests-at-completion-and-integration.md)、[ADR0046](0046-distinct-settings-dialog.md)

## 文脈

hideはドロワーだけ・ドロワーを隠してノートだけを表示する使い方を要望し、ドロワーだけでは窓も細くする意図を確認した。
中央の設定面とカテゴリ追加入口に続く第三の変更である。現行は240 logical pxのドロワーが常時左にあり、paneの座標・hit・最小幅がそれを前提にする。
編集本文はRichEditだけが所有するため、表示領域の変更を文書の読み直しや保存として実装すると、未保存入力やUndoを損なう。

## 決定

1. `folio_layout_mode`の閉じたBOTH / DRAWER / NOTEをapplicationの`folio_state`が一つだけ所有する。VIEW/EDITの`pane_mode`とは独立させる。
   起動はBOTH。実行中の状態で、settings schema版3にキーを加えない。窓の復元寸法はUIの一時状態として`folio_window`だけが持つ。
2. 全幅44 logical pxの共通上部帯に一覧／両方／ノート、設定、閉じるを常設する。3つのbuttonは同じ表・矩形で描画/hitする。
   command catalogと実行表へ登録し、Ex `:listonly` / `:both` / `:noteonly`、パレット、操作メニューから同じ実行関数へ合流する。
   文言は3言語のui_text。左のNENE FOLIOは空き幅に収まるときだけ描き、button以外の空白は移動領域とする。320幅でも復帰入口を隠さない。
3. client/DPI/modeからdrawer矩形とpane領域を一つのhelperで導出する。BOTHはdrawer240＋pane、DRAWERはdrawer全幅、NOTEはpane全幅。
   drawerの旧44pxブランド帯を除き共通上部帯の下へ配置するため、既存filter/category/rowの絶対yは変えない。件数はカテゴリ帯の右、＋の左へ移す。
   paneのcaption/actions/bodyは共通帯の下へ移す。gear/closeは共通帯に一度だけ描く。
   arrange・caption・gutter・paint damage・hit・visible判定は同じgeometry/modeを使い、非表示のpaneは幅0へresizeせずHWNDと前の矩形を保つ。
   filter矩形はdrawerのlocal計算から親座標へ変換して返す。隠れたdrawer/paneへwheelを渡さない。
4. DRAWERへ入ると幅320 logical pxへ縮め、最小320×360とする。BOTH/NOTEは最小560×360を維持する。
   入る直前の広い通常幅をlogical pxで一度保存し、出ると復元する。通常窓では現在位置と高さを保つ。
   最大化から入ると通常窓へ戻して縮め、出ると元の最大化状態と通常位置を復元する。同じmodeの再選択や狭い窓のresizeで復元情報を上書きしない。
   `WINDOWPLACEMENT`のworkspace座標をscreen座標として`SetWindowPos`へ渡さない。OSのplacement/monitor境界を使い、窓が画面外へ消えないようにする。
5. 単なる表示切替では保存・IO・文書選択・`render_pane`・RichEdit本文の再投入を行わない。本文・Undo・dirty・選択・読んでいる論理行を保持する。
   mode変更から同期WM_SIZEまでの一連のresize全体を論理行退避/復元で囲む。既存の左座標だけを比べる配置判定を、必要な幅/高さの変化へ拡張する。
6. 隠れたpane/filterへfocusを残さず、DRAWERは索引、NOTEは本文、BOTHはまだ可視の元focusへ戻す。
   NOTEで主窓へfocusが入るOS復帰も、入力面が閉じており実paneが可視/有効な場合だけ本文へ正規化し、隠れた索引が文字キーを受けることを防ぐ。
   DRAWERの単クリックとj/k/gg/Gは一覧のまま選択する。Enter・編集・閲覧・新規など本文へ明示的に入る操作はBOTHを表示して既存の操作を続ける。
   NOTEのCtrl+Shift+Fや本文Escなど索引へ移る操作はBOTHに戻す。Escの保存契約は維持し、表示buttonの非保存契約と混同しない。
   カテゴリ作成成功で索引へ移る場合もNOTEからBOTHへ戻し、取消はlayoutを変えない。
7. 検索・置換・履歴など本文を扱う面はDRAWERならBOTHを見せる。設定・Ex・パレットは現在のmodeのまま開ける。
   語を覚えているn/N・F3/Shift+F3もBOTHを見せてから検索結果へ動く。検索語が無い既存no-opではmodeを変えない。
   表示button/Ex/パレットからmodeを確定するときは既存close経路で一時入力面を閉じ、可視区画へ移る。設定中のbuttonは既存のmodal/backdropが遮断する。
   surfaceの戻り先は可視focusで解釈し、NOTE中のrootを隠れた索引として扱わない。文書なしでも見えているpaneは有効な戻り先にする。
   設定の中央配置・1列の選択・スクロール・閉じるはADR0046の同じ経路を使う。

## 強制と確認

- active: 既存のenum網羅、command catalog/文言表の網羅、source所有、compiler/静的解析を維持する。ゲートの弱体化・除外・waiverは追加しない。
- 不能: 見た目・実OSのfocus・最大化とDPIを静的検査だけで一般保証できない。完成候補に対する対象unitと実OS確認を品質記録へ残し、未測定条件を区別する。
- 対象はmodeの幾何と幅復元、狭幅の復帰/設定、索引と本文へのfocus、長い名前付き本文・無題・空本文の非保存とUndo/選択/可視論理行保持。
  commandの解析/列挙/引数拒否とapplicationの非IO保持契約を既存target testで確認する。最終headのフルゲートは必須CIに集約する。

## 結果

索引を小さな窓として使い、本文だけを広く読む操作が常設入口からできる。共通上部帯の44pxぶん本文の高さが減る。
実装・対象検証・実OS条件と限界は[品質記録](../quality/2026-10-12-layout-mode-checks.md)に残す。
起動ごとのmode/窓位置の永続化、公開Releaseの差替え、複数paneや浮動drawerは本Issueに含めない。
ADR0011の常に560幅という前提とADR0013の常時両区画という前提は、本ADRの可視領域/復元規則に従う。

## 却下した選択肢

- drawer/paneそれぞれに別の表示boolを置く: 両方が消える状態と保存先の重複が生まれる。
- 毎回文書を再表示して幅を変える: RichEditの未保存本文/Undoを失う。
- 一覧の単クリックですぐ広げる: 細い一覧として選び続ける用途を満たさない。
- 復帰buttonを隠す、既存Escを表示専用に変える: 可視の入口または既存の保存契約を壊す。
- modeと窓位置を設定へ保存する: 要望に必要な実行中の切替を越え、schema/複数画面の復元まで変更が広がる。

## 参照

- [Microsoft SetWindowPlacement](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowplacement)
- [Microsoft WINDOWPLACEMENT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement)
- [Microsoft ShowWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow)

## 2026-10-14 の補正（#302）

[ADR0051](0051-drawer-new-note.md)で決定6の新規を補正する。無題の離脱と次文書の準備が成功した後にだけ
一覧から両方へ広げる。キャンセル・失敗では一覧の表示と窓幅を保持する。

## 2026-10-18 の補正（#344）

[ADR0059](0059-common-new-note-and-content-alignment.md)で新規の常設入口を共通ヘッダー左へ集約する。
狭幅では同じ位置の図形へ縮め、24 logical px以上の移動領域を残す。本文操作列は固定36pxへ揃える。
UIAの新規の親は共通ヘッダー、F6のヘッダー入口は新規、索引入口は検索欄とする。
検索focusの保存は#342/ADR0024、作成・取消・成功後の表示変更はADR0050/0051の契約を維持する。

## 2026-10-18 の補正（#348）

[ADR0060](0060-ui-part-styles-and-state-rule.md)で決定2の共通上部帯の部品を3様式＋選択群へ揃える。
表示3択は区画56・群の余白4の選択群、設定・閉じるは当たり28の絵だけの部品、高さはすべて28。
押下は離した時に実行し、hover・押下・focusは1つの状態規則で描く。Tabは3択の群に1回だけ止まり、矢印はfocusだけを動かす。
表示切替の非保存・幅復元・本文保持の契約は変えない。

## 2026-10-30の補正（#452）

[ADR0075](0075-window-shell-controls-and-layout-chooser.md)で次を補正する。
共通headerのshell操作を常設し、狭幅の表示3択は同じMENUのchooserへまとめる。
normalから一覧へ入り、一覧中にOS最大化してから退出する場合は、OSをnormal化した後に現在の一覧normal位置/高さと保存wide幅を復元する。
最大化から一覧へ入った場合の保存wide_placement復元と、非保存の表示切替契約は維持する。
