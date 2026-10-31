# ADR 0079: 閲覧リンクの起動を検証済みWeb URIと明示portへ渡す

Status: accepted / 2026-10-31 / Issue #482

## 背景

hideは#480の表示サンプルを確認後、閲覧モードでWebリンクを開く機能を依頼した。
既存のリンクは色/下線だけで、表示名とリンク先の対応をnative本文に保持していない。
表示名付きリンクと山括弧URLを同じ経路で扱い、文字選択・編集・保存と衝突させない必要がある。

専用window station/desktopの診断で、RichEditのfriendly-name fieldはURLをhidden instructionとして
保持し、TOM GetURLで取得できることを確認した。例ではraw111単位に対し表示38単位で、
通常のコピーは表示38単位だけだった。GetURLは両端のASCII引用符を含むURLを返した。
nativeの位置を表示文字の位置とみなすと、既存検索/選択を壊す。

## 決定

- Markdownの解釈は既存coreで一度だけ行う。許可されたWebリンクの表示に標準RTF HYPERLINK fieldを使い、
  表示名の装飾とリンク先を保持する。UIにMarkdown parserやリンク先の別解釈を作らない。
  自動URL検出は有効にせず、コード・画像原文・裸URL・非対応schemeの既存表示を保つ。
  URLそのものを表示するnative autoURLはWindows標準のリンク色/下線となる。
  Windows版でAURL_DISABLEAUTOFORMATを指定するとRTF色も失い暗色で読めなくなるため採用しない。
- `web_uri`が終端付きUTF-8の検証済みWeb URIを所有する。absolute HTTP/HTTPS、非空authority、
  有限長を要求し、NUL/制御文字/空白/生のbackslash/引用符/山括弧とauthorityのuserinfoを拒む。
  schemeはASCII大小を区別しない。DNS解決や到達性はこの型の責務にしない。
  Markdown destinationのescape/文字参照を既存の字句処理で展開してから同じfactoryへ渡す。
  不対応destinationは従来のリンク装飾を保ち、fieldを作らない。確保失敗はRTF全体の失敗へ戻す。
- UIの`note_pane_links`がnative書式から導くhidden区間の対応を所有する。RTF投入の準備中に作り、
  同じnative本文にだけ結び付ける。文書差替えで失効し、編集では保持しない。
  検索へ渡す本文はhidden instructionを除き、検索/選択の範囲をnative位置との間で変換する。
  表の配置やnative書式検査は従来のraw位置を使う。表示用の別Markdown本文やASTは作らない。
  対応の構築失敗は既存note_pane_loadの非READY契約へ戻し、部分的なリンクを操作可能にしない。
- nativeリンク通知は既定の外部起動へ流さず、対象pane・閲覧・READY・IME/modal/streamingを確認する。
  通常の左クリックは同じリンクでの押下/解放と非dragを要求する。文字選択や修飾付きdragでは開かない。
  閲覧のCtrl+Enterでcaret/選択内の単一リンクを開く操作も同じapplicationの意図へ渡す。
  起動直前にTOMから取得した対象をUTF-8へ変換し、同じURI factoryで受理する。
  表示名とURLが一致するとRichEditがfieldをnative autoURLへ変換するため、GetURLがS_FALSEなら
  GetLinkTypeでtomClientLink/tomAutoLinkURLを確認し、tomLinkの全範囲を同じURI factoryへ渡す。
  実Msfteditでは自動走査無効時にtomClientLinkだった。nativeの型/範囲を正本とし、
  URL自動走査を有効にしたり、画面の文字列からリンクを推測したりしない。
  autoURLは隣接すると範囲が結合するため、受理fieldの直後へhiddenの空白を一つ出力して境界を保つ。
  これはnative書式の区切りで、表示/検索/コピーからは既存のhidden対応で除く。
- applicationが意図を受け、`web_link_port`だけへ検証済みURIを渡す。portは合成rootが明示して
  `folio_ports`からstateへ渡す。文脈を必要としない起動関数に汎用void pointerやglobal登録を持たせない。
- adapters/win32だけがShellExecuteExWのopenを使い、URIをlpFileへ直接渡す。コマンドshell・
  引数文字列・探索する実行ファイル名を組み立てない。native COM寿命はadapter内で対にし、
  OSの失敗を型付き結果で返す。失敗表示は既存ui_textの全言語列と本文下の通知へ収める。
- 保存本文、Undo/dirty、ノートschema、版、公開betaは変えない。mailto、file、UNC、内部/相対リンク、
  裸URL認識は追加しない。OS同梱shell32等の既存宣言を使い、実行時依存やゲートを緩めない。

## 検証

URIの受理/拒否・上限・原文保持・確保失敗、RTF fieldのescapeと表示label、applicationのmode/port結果を単体で確認する。
専用native fixtureでhidden/visible対応、非ASCII/補助面文字、隣接リンク、表、検索/選択/copy、
クリック/drag/鍵/IME/失敗、テーマ/resize/dirty/Undoを直接影響範囲として確認する。
試験の外部起動は偽portで捕捉し、hideのclipboard/focus/実ノートを使わない。
実adapterは固定の公開example.comを必要な一回だけ開く境界確認とし、合成試験や実reader聴取と区別する。
最終headの必須CIで正典eng/check.ps1を要求する。Waivers none。

原本: `D:/NeNeFolio/agents/web-links-20261031/`、`D:/NeNeFolio/outputs/482-web-links/`。
途中診断はnative-compile-01（C23指定の誤りで失敗）、native-compile/run-02（成功）。

参考: [RichEdit Hyperlinks](https://devblogs.microsoft.com/math-in-office/richedit-hyperlinks/)、
[ITextRange2::GetURL](https://learn.microsoft.com/en-us/windows/win32/api/tom/nf-tom-itextrange2-geturl)、
[EN_LINK](https://learn.microsoft.com/en-us/windows/win32/controls/en-link)。
