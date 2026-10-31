# ADR 0081 — モード切り替えと操作メニューのキーを同じ記述子から案内する

- 状態: 受理（2026-11-01、hideのショートカット追加とホバー説明の依頼）
- 日付: 2026-11-01
- Issue: #490
- 規則: ARC-001/004/011、C-002/008/012/017/018、QLT-008/012/013
- 関連: ADR0013/0016/0018/0048/0060

## 決定

1. Ctrl+Eを閲覧/編集の往復にする。現在modeはapplicationだけが所有し、UIは閉じたmodeから
   既存EDIT/VIEW commandを選んで同じdispatcherへ渡す。閲覧へ戻る前の保存、保存失敗や無題の
   名前入力取消で本文と編集を保持する契約は既存のまま。i、Esc、Ctrl+Sの意味は変更しない。
   ノート未選択では実行しない。一覧のみの表示からは既存mode commandと同じく本文を表示する。
2. Ctrl+Mを「操作」ボタンと同じメニューへ結ぶ。ボタンの共通矩形と既存menu surfaceを使い、
   キーで開いた場合は先頭の有効項目を選び、Escで元のfocusへ戻る。同じキーの再打鍵はボタンと
   同じくメニューを閉じる。未選択や一覧のみで操作ボタンが非表示なら何もしない。
   Ctrl+Pは従来のヘルプ/コマンド一覧であり、操作メニューのキーとは表示しない。
3. 両キーをcoreの既存folio_shortcut記述子にHACCEL経路のactionとして加える。TOGGLE_MODEと
   OPERATIONSは既存commandの別名ではなく、現在modeの選択/面を開くUI意図である。
   actionのキー表記を得る既存の内部検索をfolio_shortcut_action_hintとして公開し、tooltip、UIA、
   helpから参照する。modeや操作menuのための別command catalog/保存経路/キー文字列を作らない。
4. IME変換、native modal、設定/テーマ、終了処理、本文投入、除去処理中は新キーを実行しない。
   既存のlayout用可否判定を共有し、新意図の既存dispatch側の抑止も維持する。
   新しい2キーだけはWM_KEYDOWNのprevious-state bitを実行前に見て、自動リピートを消費する。
   key/modifierは同じ記述子から照合し、押下状態を別に所有しない。
5. 閲覧/編集・保存・検索・操作のtooltipを、字の省略有無にかかわらず「説明 (キー)」にする。
   両modeボタンは「閲覧・編集を切り替える (Ctrl+E)」とする。Ctrl+Eは現在modeから反転するため、
   「保存して閲覧」だけの直接キーと偽らない。個別ボタンのクリックは引き続き指定modeを選ぶ。
   保存は既存Ctrl+S、検索は既存Ctrl+F、操作はCtrl+Mを用い、ja/en/zh-Hansの文言をui_textで持つ。
   標準Tooltip control、矩形、寿命、UIA Name、既存の他のボタンの説明は維持する。

ADR0013の「閲覧への出口は札と終了だけ」、ADR0018の「保存して閲覧はGUI専用」、
ADR0048/0060の「字付きtooltipはキーだけ」「この変更でglobal shortcutを足さない」を上記範囲で補正する。
VIEW単独commandのキーは依然空とし、menu内のV表示は維持する。新キーはヘルプの同じキー説明へ追加する。

## 確認と制約

Debug/tidy、既存shortcut/文言unit、実Win32のアクセラレータ・両方向・保存/取消/失敗・IME/modal・
auto-repeat・focus・tooltip/UIAを対象に確認する。物理IMEやreaderの聴取とは区別する。
最終PR headの必須CIを統合条件とし、成功済み対象検証を工程ごとに繰り返さない。
保存schema・版・依存・Store提出済みMSIXは変更しない。Waivers none。

Ctrl+EはRichEditの既定では中央揃えに使われるため、既存HACCEL優先経路でアプリのmode操作として
受け、通常入力へ落ちないことを確認する。新たな文字装飾機能は設けない。
[MicrosoftのRich Editキー仕様](https://learn.microsoft.com/en-us/windows/win32/controls/about-rich-edit-controls#rich-edit-shortcut-keys)。
