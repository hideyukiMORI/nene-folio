# READMEと配布入口の現行化（#785）

## 公開状態と公式Store導線

hideのREADME最新性/Store URLの質問と、その後の継続指示に基づく文書更新。
READMEのGitHub公開版1.3.2は維持し、mainの後続CJK修正とは分けた。Store商品は9PCHS225SWBX。
`winget search --name "NeNe Folio" --source msstore --disable-interactivity`が同IDを返し、
`winget show --id 9PCHS225SWBX --exact --source msstore --disable-interactivity`で商品名NeNe Folio、
公開元Hideyuki Mori、発行元URLの本repo一致を確認した。契約同意・install・updateは行っていない。
公式[Storeページ](https://apps.microsoft.com/detail/9PCHS225SWBX?hl=en-US&gl=US)はHTTP200、titleは同製品。
web検索では見つからず、web openも取得不能だったため、wingetの公式msstore sourceとHTTPSの実応答で確認した。
Store版番号はUnknownで、1.3.2の認定/公開を推定しない。1.3.1更新成功は[hideの報告#706](https://github.com/hideyukiMORI/nene-folio/issues/706)、
1.3.2申請は[報告#781](https://github.com/hideyukiMORI/nene-folio/issues/781)として区別する。
raw出力runs/store-show-01、store-page.html/jsonを原本に保存した。

## 現行公開版の画像

公開ZIPのSHA256 e3922f11b227d089528f7880f27df4789b848a1b289871311faa36f48da36a34と全11payloadを照合。
exe SHA256 092810e436e7857013e5658bd1f41a36b133edea67519bbb723ccd489b503793、配布source ba1e697deccb71e2f248f4759349d51ddec99e29。
D専用展開先と独立desktopで、3カテゴリ7ノートの合成データだけを開いた。
既存post-keys.ps1のPID指定PostMessageとcapture-window.ps1のDPI対応PrintWindowを使う。
1200×800の閲覧/Lightとテーマ選択/Nordの2枚を目視し、日本語本文・Mermaid図・テーマ一覧・status barを確認した。
画像の編集/合成はせず、原本とdocs/images/1.3.2-main.png・1.3.2-themes.pngの全bytes/hashが一致する。
旧0.2.0画像は過去品質記録の参照先として保持した。
所有PID27968は正常終了0、worker26952も0。前面window handle332720は前後不変、SwitchDesktopなし。
通常app/実ノート/clipboardの操作は行わない。PrintWindowによる採取でありDWM画面や物理IMEの受理ではない。
原本readme-preparation.json、readme-ui-result.json、readme-desktop.json、screenshots.jsonとruns/capture-01。

## 通常反映と現在状態

#783/PR784最終CI42239417180 full-gate/check成功・main d343071統合後、hideの終了報告を受けた。
正典`pwsh -NoProfile -File tools/prepare-real-machine.ps1`を00:24に実行し成功。起動はしていない。
新exeは1.3.2/1.3.2.0、8,098,816bytes、SHA256 ab97b69dcab303d8a75c09ed879237c9477939a64c1ddb6d8faabc02180018f1。
実data33fileのbytes/hash/作成更新日時不変、旧exe退避と照合を完了した。
1,481fileを全hash照合したarchiveへ保管後、783専用worktree/outputを整理し、branch/commitを保持した。
確定原本D:/NeNeFolio/agents/editor-font-binding-20261106/integration-reflected.json。
00:12のintegration-completion.jsonは反映待ち時点の記録として保持し、確定結果へ読み替えない。

## 文書検証・範囲

変更はREADME・画像2枚・current・本品質記録・書体品質記録への追記・11月7日日報/引き継ぎ。
文書差分・相対参照・画像hash/寸法・実行結果・通常exe不変・conformanceを対象確認し、最終head必須CIを統合条件とする。
製品C・API・保存schema・版・ゲート・配布物に変更なし。成功済みの機能試験を反復しない。
FR-014、QLT-008/012/013、CNF-006、GIT-001〜004、ADR0043/0045/0080。追加waiver none、既存WVR-0001継続。
原本D:/NeNeFolio/agents/readme-current-20261107/。統合・保全整理は同integration-completion.json。
Storeの現在掲載版/1.3.2審査結果・実ノートでの見た目は未確認。Store送信や通常起動は行っていない。

文書検査check-01はPowerShellの配列/文字列連結優先順位により複数pathを一つの文字列へ連結したため、
記録ツールがinput snapshotで拒否し、検査子は未起動。元一覧をinputs-invalid-array.jsonとrun snapshotへ保持し、
pathごとの配列へ訂正したcheck-02で文書検査を行う。製品・画像取得の失敗とは区別する。
