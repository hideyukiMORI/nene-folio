# ADR 0055 — アクセシビリティ公開とOS高コントラストを別の受理単位にする

- 状態: 受理（#312の範囲設計。後続の製品実装は未着手）
- 日付: 2026-10-17
- Issue: [#312](https://github.com/hideyukiMORI/nene-folio/issues/312)
- 規則: FR-012/013/016/018/020/022/024/032、ARC-001/002/003/004/007/008/010/011、C-002/004/007/012/018、QLT-008/010/012/013
- 関連: ADR0013/0015（索引のfocus/cursor）、ADR0016（共通コマンド）、ADR0030/0032（文言）、ADR0046〜0049（設定・表示・配色）、ADR0050/0053/0054（本文と終了）

## 文脈

main `8ffb9bcb9e64e1f131642cd38eebfad917a0d019` の既存exeを隔離したfixtureで操作した。
ノートのクリック、jによる次ノート/折り畳みカテゴリへの移動、lによる展開、表示キー、設定の開閉、絞り込みは動作した。
取得されたアクセシビリティ情報には本文RichEditと検索EDITが現れた一方、ヘッダの操作・カテゴリ/ノート行・設定の選択肢は現れなかった。
カテゴリへcursorが移っても、取得文面には直前のノート本文が残った。検索EDITは入力値を公開するが、取得されたNameは空だった。
これは実スクリーンリーダーの発話確認ではない。取得手段の整形済みtreeとnative GetFocusも同一視しない。
実測・入力・限界は[品質記録](../quality/2026-10-17-accessibility-observations.md)に残す。

現在のappearance_portはlight/darkのみを返し、主窓はImmersiveColorSetという名前のWM_SETTINGCHANGEだけを再着色入口にする。
固定テーマにhigh-contrastという名前があっても、OSの高コントラスト状態・利用者のシステム色とは結び付いていない。
読取専用probeではWindows 11 Pro build 26300、OS高コントラストOFFを確認した。OS設定は変更していない。

## 決定

**基本操作の意味公開を先に進め、OS高コントラスト追従と補助画面の意味公開を別Issue/PRで受理する。**
このADRと#312の完了は範囲・正典経路・受け入れ条件の確定であり、アクセシビリティ対応済みという製品宣言ではない。
後続はA [#319](https://github.com/hideyukiMORI/nene-folio/issues/319)、B [#320](https://github.com/hideyukiMORI/nene-folio/issues/320)、C [#321](https://github.com/hideyukiMORI/nene-folio/issues/321)。

### A — ヘッダ・索引・本文への到達とUI Automation公開（最初の実装）

対象は共通ヘッダ、本文の操作列、索引の新規入口・検索欄・カテゴリ/ノート行、および既存RichEditへの到達。
新規の自前描画要素の公開はWin32 UI Automationのserver-side providerを正典にする。
独自MSAA providerとの二重実装、画面文字のOCRによる代用、全UIのnative controlへの置換は行わない。
これは新規の自前controlにUIAを勧める[Microsoftの方針](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-msaa)とも整合する。

1. providerの寿命・COM参照・HWNDとの結合はui/win32が所有する。文書選択・カテゴリ展開・索引cursorは既存applicationが所有する。
   providerは状態を引き、既存の共通コマンド/行操作へ意図を渡す。別の文書一覧・採用状態・保存経路を作らない。
   描画・hit test・UIA Bounds/ElementFromPointは同じレイアウト値を使う。文言はui_textの3言語、ノート/カテゴリ名は実データから取る。
2. 公開する意味は次の対応とする。実際にできないpatternや常時成功する形だけの実装を提供しない。

   | 対象 | 公開する意味と操作 |
   | --- | --- |
   | 共通ヘッダ・本文操作列 | ToolBarと区別できる名前。通常操作はButton/Invoke、表示3択と閲覧/編集は選択群と現在値、操作メニューは開閉とnative menuへの遷移 |
   | 新規ノート・カテゴリ追加 | 名前付きButton/Invoke。有効条件・未保存時の確認は既存操作と同じ |
   | 索引 | Tree。単一の選択済みノート、スクロール範囲、結果集合を公開 |
   | カテゴリ | 名前付きTreeItem、ExpandCollapse、focus。カテゴリcursorは本文の選択を変更しない |
   | ノート | 名前付きTreeItem、LeafNode、SelectionItem、focusとScrollItem。Selectは既存の保存/切替契約、本文への実行は既存Enter経路 |
   | 検索EDIT・本文RichEdit | native providerを維持。検索の名前/説明と本文の閲覧/編集状態を確認し、既存Value/Text/選択の能力を壊さない |

   Tree/TreeItemのpattern、折り畳み配下と画面外の区別は[Tree](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supporttreecontroltype)・[TreeItem](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supporttreeitemcontroltype)の契約に合わせる。
   選択済みノートと索引cursorは同一ではない。HasKeyboardFocusは実際に索引を操作しているcursor、IsSelectedは選択済みノートから導く。
   カテゴリをfocusしただけでノートを選択解除したり読み直したりしない。
   現行ADR0015の「ノートcursorは選択済みノートそのもの」では、未選択ノートへのUIA SetFocusを表せない。
   Aではapplicationの既存cursorを、選択とは独立したカテゴリ/ノートの1つの行参照へ拡張する。UIA専用cursorをUI側に足さない。
   SetFocusはこのcursorと表示上のfocusだけを変え、本文読込・保存・選択変更を起こさない。展開中カテゴリにも明示的なfocusを置ける。
   Select/Invoke/クリックでノートの採用に成功したときだけ、選択とcursorをそのノートへ揃える。失敗/取消なら両者を変更しない。
   既存j/kの停止行と「ノートへ移ると選択も変わる」操作は維持する。Enterはcursorのノートを既存の保存/確認経路で採用してから本文へ進む。
   UIA Expand/Collapseは展開意図だけを渡し、別ノートを暗黙に選ばない。従来のlが展開後にノートを採用する動作は、共通の展開→選択の組合せで保つ。
   絞込/折畳/削除でcursorの行が消えた場合の可視行への復帰もapplicationが決め、UI/providerに隠れた別cursorを残さない。
   AのPRでADR0015の状態/Enter契約、FR-018とARC-004の所有者表を同時に更新する。
   [UIA SetFocusの契約](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-irawelementproviderfragment-setfocus)と、選択を伴う明示操作を区別する。
3. マウスなしの操作経路としてF6/Shift+F6で可視の操作区画を巡回できるようにする。
   順序は共通ヘッダ→索引→本文操作列→本文、逆方向はその逆。非表示/無効の区画は飛ばし、見えるfocus印を描く。
   ヘッダ/操作列内のTab/Shift+Tabで可視の操作を順に辿り、Enter/Spaceは共通の実行へ渡す。
   索引の固定操作・検索欄・行の間もTabで往復できるようにする。行は1つのTab停止点とし、全ノートをTab列へ並べない。
   既存j/k/h/l、Enter/Esc、矢印/PgUp/PgDnのスクロール、表示直接キーは維持し、キー説明をui_textへ追加する。
   RichEdit内のTab入力やIME変換を奪わない。本文から操作区画へ出る入口はF6を使う。
   名前入力・確認・設定などのモーダル中は、その面の操作範囲を越えて背後へfocus/Invokeできない。
4. 名前、選択、focus、展開、結果集合、境界/可視性の変更を既存の状態更新に伴って通知する。
   行番号を永続的な識別子とみなさない。並べ替え・絞り込み・改名・削除後の古いprovider参照は、別のノートへ化けず失効する。
   スクロールで画面外になった行と、折り畳み/絞り込みで対象外になった行を区別する。
   窓破棄後の呼出し、参照保持中の終了、列挙中の状態更新、UI threadへの再入を対象検証に含める。
5. native EDIT/RichEditを重複する独自要素で包んで能力を消さない。欠けたnative入力名の補い方は、標準providerのText/Value/選択が残る小さな実測で確定する。
   UIA/COMのthreading、古い参照の失効、native子窓とのtree結合を実装Issueの着手時に固定し、対応するAPI/OSライブラリだけを正典ビルドへ宣言する。
   外部runtimeや製品テスト窓口は追加しない。[providerの公開・通知・切断](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-serversideprovider)を含む小さい実測なしに一括実装へ進まない。

Aの受理条件は、UIAの名前/型/pattern/状態/通知とキーボード操作の両方が確認できること。
さらにWindows Narratorなど実スクリーンリーダーを1つ以上使い、名前・役割・選択・展開・focus移動の発話を確認する。
reader名/版/言語/OS/入力方法と発話または人の確認記録を残す。UIA treeだけでこの条件を代替しない。
3言語のName、3表示、狭幅/通常幅、絞込0件・複数カテゴリ・画面外の行、無題の取消、保存失敗、IME/modal/終了中の抑止を変更境界として扱う。
未保存のAノートを開いたままUIA SetFocus(B)を行い、cursor=B・選択=A、保存/履歴write 0を確認する。続くSelect(B)の保存失敗/取消では両者を保持し、成功時だけBへ揃える。
既存本文・Undo・変更印を、単なるfocus移動や列挙で変えない。実行による保存は従来契約どおりに限る。

### B — OS高コントラストを一時的な実効配色として反映（Aから独立）

1. OS状態を読む唯一の場所はappearance_adapter。起動とOSの色/高コントラスト変更通知から、既存appearance_portを通じて更新する。
   SPI_GETHIGHCONTRASTの状態と必要なGetSysColor値を、platform非依存の明示値としてapplicationへ渡す。
   UI各所でOS色を直接取り直す経路は増やさない。WM_SYSCOLORCHANGEと該当するWM_SETTINGCHANGEを扱い、ImmersiveColorSetだけに限定しない。
2. OS高コントラストONは、保存済みのsystem/light/dark/固定テーマのどれよりも表示上優先する。
   theme_choiceとカテゴリ色の保存値は維持し、設定ファイル・本文・履歴は書き換えない。OFFに戻ると元の選択から通常色を解決する。
   高コントラスト中にテーマを選び直す場合も保存する選択と実効表示を区別し、「OS高コントラストを適用中」と既存設定面に明示する。
   初期段階で独自の「OS高コントラストを無視」設定は追加しない。
3. applicationが持つ実効配色をUIとRTFが共有する。固定127配色の正本表は維持し、OSから渡された色を同じ役割へ写す1つの解決経路を加える。
   UIのCOLORREF変換と本文RTFで別々にOS色を決めない。HC中の配色更新はenumのlight/darkが同じでも検出する。
   原則はOSのwindow/windowText・buttonFace/buttonText、選択はhighlight/highlightTextの対。
   本文・見出し・リンク・コード・行番号・各ボタン・カテゴリ帯・設定面・名前入力・確認/失敗面を含めて読む文字と背景を対応させる。
   カテゴリ固有色や透明な暗幕、フェード、複数色の装飾をHC中の可読性に持ち込まない。focus・選択・無効は線/印/状態でも区別する。
   [Microsoftの高コントラスト指針](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter)を基に、利用者指定のシステム色を尊重する。
4. 外観の取得失敗とHC_OFFを同一視しない。既存の有効snapshotを壊さず、初回取得失敗時の退避と通知を着手時の型・失敗契約に含める。
   RTF準備失敗は既存値を保持する。RichEdit投入失敗/OS終了中の再入はADR0053/0054の保護を通す。
   編集中の本文・Undo・変更印・選択・表示論理行を保持し、設定モーダルが開いたままでも可視面の色を揃える。

Bの受理条件は、通常/HCの切替・HC中のシステム色変更・OFF復帰、systemと固定テーマ、設定面を開いた状態、VIEW/EDITと未保存本文を確認すること。
注入した明暗2組の任意システム色によるunit/Win32検証に加え、実OSの高コントラストON/OFFを少なくとも1環境で確認する。
実OS設定の試験は復元可能な検証環境を用い、環境・前後設定・画像・復元を記録する。模擬色だけでOS追従完了を主張しない。
固定配色全127種類の無関係な再測定はしない。実装時にFR-016・ARC-004の状態表・ADR0049のOS値範囲を同じ変更で更新する。

### C — 設定・候補一覧・検索/置換・通知の意味公開（Aの公開基盤を再利用）

設定/テーマ選択、コマンド/ヘルプ/履歴の候補、ノート内検索/置換、未保存/失敗/完了の通知が対象。
各面のName・値・選択・現在のモーダル範囲と復帰focusを公開し、背後の要素は操作不可にする。
名前入力や標準確認ダイアログはnative公開を維持し、ラベル不足だけを補う。
読み上げ通知を文字列の定期再送で作らず、既存状態変更に対応する通知へ結ぶ。内容を自動保存する別経路は追加しない。
Aの基本操作だけを終えた時点では、これらの面を読み上げ対応済みと扱わない。
Cでも対象のキーボード経路・UIA状態/通知・実readerでの発話確認を受理条件にする。

## 強制と完了の境界

- #312の設計受理時は製品動作をすべて **planned** とした。#312ではproduction・API・保存schema・依存・ゲートを変更していない。
  2026-10-17追記: Aは#319/ADR0056、Bは#320/ADR0057で実装・対象検証を進めた。最終CI/統合の確定先は各品質記録と現在タスク。
  Cは#321の後続範囲であり、補助面全面公開をA/B完了へ含めない。
- A/B/Cは後続Issueごとの小さいPRで実装し、関係する仕様と所有者表をそのPRで更新する。
- 既存の依存・文言・enum・format/tidy/conformanceの強制は維持する。UIAの意味/発話は自動で全保証できず、記録付きの対象検証を残す。
- 各実装では完成時の対象検証と最終headの必須CIを要求する（QLT-008/012）。未実施のreader/OS試験を省略して完了にはしない。
- #312の完了条件は現状測定、上記の範囲・優先順・正典経路・受理条件、後続Issueへの収載、文書CIと統合。
  設計文書だけの変更に、ローカルで無関係な製品テストを繰り返さない。
- 公開Releaseの差替え、全UI刷新、一般的なWCAG適合/全reader対応の宣言は今回の範囲に含めない。Waivers: none。

## 採らない案

| 案 | 理由 |
| --- | --- |
| 固定high-contrastテーマをOS追従の代用にする | OSの任意色とON/OFF変更を反映できない |
| UIAと独自MSAAを同時に作る | 同じ意味と寿命の実装を二重に持つ |
| tooltipや本文の読み出しだけで全体対応済みにする | 行/ボタンの役割・状態・操作・通知が欠ける |
| native controlへ全置換する | 現状の不足に対して変更範囲が大きく、既存UIと本文安全性を広く動かす |
| UIA用の選択モデルと直接保存処理を追加する | applicationの所有と共通操作を迂回する |
