# アクセシビリティの現状測定 — 2026-10-17

Issue: [#312](https://github.com/hideyukiMORI/nene-folio/issues/312)。設計: [ADR0055](../adr/0055-accessibility-and-system-contrast-scope.md)。
測定基点: main `8ffb9bcb9e64e1f131642cd38eebfad917a0d019`。
exe SHA256: `80f5b540f25400a31fa65116b6973a0ca7d1eeb2bb0993b9c2710e03e9479eac`（#311反映済みの既存Debug exeと一致）。
恒久証跡: `D:/NeNeFolio/agents/312-a11y-scope/`。

## 条件と手段

Windows 11 Pro / 10.0.26300 / x64。読取probeのPer-Monitor v2文脈でGetDpiForSystemは120。
これは試験窓のGetDpiForWindowを取得した値ではない。画像と操作はcomputer-useのsky APIで対象の試験窓を名指しした。
通常作業データは使わず、D:/NeNeFolio/outputs/312-a11y-scope/runtimeにコピーした既存exe・同梱fontsと、2カテゴリ/3ノートの小さいfixtureを使った。
取得した画像は基準の960×640と一覧320×640としてツールが返した範囲。別画面・別DPI・全3言語での実操作試験ではない。

公開情報はcomputer-useのUI Automationベースの整形済みtree/document_text/focused_element。
raw UIA treeの全property・pattern・eventを列挙した独立clientではない。とくにfocused_elementが関連本文を返す場合があり、native GetFocusの測定とは扱わない。
利用したWindows操作スキル: `C:/Users/info/.codex/plugins/cache/openai-bundled/computer-use/26.1013.31256/skills/computer-use/SKILL.md`。
物理キーボードの人手入力ではなく、同スキルのキー/ポインタ入力である。従来のPostMessage harnessの結果とも区別する。

## 観測

| 操作/画面 | 画面で確認した状態 | 取得された情報と判断 |
| --- | --- | --- |
| 初期表示 | 共通ヘッダ、索引、空の本文 | window、名前のないEDIT ID 2、名前のない子window、RichEdit Document ID 3。ヘッダ/行を示す要素はない |
| 最初のノートをクリック | Accessibility fixtureを選択し、本文を描画 | RichEditの本文テキストは取得できる。ノート行・カテゴリ行の要素はない |
| 索引でj | Second noteへ切り替わる | 本文はSecond noteに変わるが、ノート選択を示す行要素はない |
| もう一度j | 折り畳まれたArchiveにcursor印、選択済みノートと本文はSecond noteのまま | 取得情報にはSecond note本文だけが残り、Archiveの名前/展開状態/cursorが現れない |
| l | Archiveを展開しArchived fixtureを開く | 新本文を取得できるが、カテゴリ展開を示す要素はない |
| 索引でTabを1回 | cursor/画面は変化せず | ヘッダへ移動した観測はない。この1回だけで全Tab経路を検証したとはしない |
| Ctrl+Shift+1 | 一覧320幅へ切り替わる | RichEditは取得treeから外れる。表示3択/現在値、行要素はない |
| 設定アイコンをクリック | 中央設定面が開く | 名前のないwindowが増える。テーマ/言語/行番号の項目は取得できない |
| 設定でEsc | 一覧へ戻る | 設定の子windowは取得treeから外れる |
| Ctrl+Shift+F | 索引の検索EDITにcaret | focused_elementはEDIT ID 2。検索欄の名前は得られない |
| Secondを入力 | 絞り込み結果が1/3、Second noteだけが見える | EDITのValueはSecond。結果件数/行を示す要素はない |
| Alt+F4 | 試験窓が閉じる | 試験exeのwindow一覧は空。本文編集/保存はしていない |

最初の文書未選択状態で送ったEsc/Ctrl+Shift+Fからは、取得情報のfocus変化を確認できなかった。
その結果もui-observations.jsonへ残し、選択後に検索欄へ移る操作を別に確認した。入力が失われた理由や製品不具合はこの観測だけで断定しない。

ソースではsrcに独自のWM_GETOBJECT/UIA/MSAA provider/NotifyWinEventがなく、描画した行/ヘッダを公開する実装も見当たらない。
実際に本文と検索値が取得できたので、アプリ全体が読み上げ不能だとは結論しない。
画面のcursorと選択済みノートが別であることはdrawer_windowのrow.cursor/row.selectedとも一致する。

## OS高コントラスト

読取専用のread-appearance.cを、固定eng/toolchain.ps1のclang-clでコンパイルして実行した。
正しい引数は `/clang:-std=c23 /W4 /WX /utf-8 /MT`、linkはuser32.lib。compile/実行は最終exit 0。
SystemParametersInfoW(SPI_GETHIGHCONTRAST)は成功し、flags=126、HCF_HIGHCONTRASTON=false。
GetSysColorのwindow/windowText/buttonFace/buttonText/highlight/highlightText/grayText/hotlightをappearance-observation.jsonに記録した。

初回2回は、外部probeの標準指定を`/std:c23`、`-std=c23`と誤って渡してコンパイラが拒否した。
eng/targets.cmakeの正典を照合して`/clang:-std=c23`へ訂正した。抑制追加・production変更はない。
DPI非対応の初回実行が返した96も別記録として残し、probe自身をPer-Monitor v2にして120を取得した。

appearance_adapterはAppsUseLightThemeを読むだけ。folio_state_refresh_themeは解決された固定theme enumを比較し、
主窓のsystem_colors_changedはImmersiveColorSet以外を受け付けない。WM_SYSCOLORCHANGEの専用処理もない。
OS HCがONの実表示は未測定。今回の結論は「明示追従する経路がない」であり、ONにして不具合を再現した結果ではない。
OS高コントラスト・システム色・通常環境の設定は変更していない。

## 証跡と後続の判定

- ui-observations.json: 操作ごとの取得情報と測定上の限界。
- baseline-main/selected-note/category-cursor/list-only/settings/filtered.png: 対象画面の保存画像。
- read-appearance.c、appearance-build*.log、appearance-observation.json、os-observation.json: 外観読取の入力/結果。
- 試験exe・fonts・fixture、read-appearance.exeは完了時にarchiveとhashへ収載する。

現状測定からADR0055のA（基本操作）、B（OS配色）、C（補助画面）を分けた。
実スクリーンリーダーの発話、raw UIA pattern/event、OS HCのON/OFF、別OS/DPI/言語、実利用者受理は未実施。
これらを#312の設計だけでPASSへ変えず、後続Issueの受け入れ条件へ明記する。

後続は基本操作/UIA [#319](https://github.com/hideyukiMORI/nene-folio/issues/319)、OS配色 [#320](https://github.com/hideyukiMORI/nene-folio/issues/320)、補助画面 [#321](https://github.com/hideyukiMORI/nene-folio/issues/321)。
読み取り専用の独立設計レビューで、現行ノートcursorが選択と一体である点をP2として検出した。
ADR0055へapplication所有の独立cursor、SetFocus/Selectの分離、保存失敗/取消の保持を明記し、再確認後の未解決P1/P2はない。
レビュー記録は恒久証跡のreview.md。未実装の動作を確認した結果ではない。

production・保存schema・依存・ゲート変更なし。規則: FR-012/013/016/018/020/022/024/032、ARC-001/002/003/004/007/008/010/011、C-018、QLT-008/010/012/013。Waivers: none。

初回の文書規約検査は記録ファイル名に含めたbaselineがCNF-005の禁止語へ該当し、1 violationで失敗した。
記録をaccessibility-observationsへ改名し、参照を更新した。検査規則・除外・抑制は変更していない。

訂正後の`python eng/conformance.py --root .`は0 violations、`git diff --check`はexit 0。
変更6文書の相対参照70件はすべて存在し、production差分がないことも確認した（document-checks.json）。
最終headの`pwsh -NoProfile -File ./eng/check.ps1`は必須CIで受理し、結果は関連PRとcompletion.jsonへ記録する。
