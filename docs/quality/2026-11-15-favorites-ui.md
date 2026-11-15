# お気に入りUIの品質記録 — #817

2026-11-15。hideの継続実装指示に基づく。開始mainは667f9a7fd5726b1cdc47fa27504d26009a2b8368。
設計は[ADR0091](../adr/0091-favorites-scope-and-quiet-stars.md)、保存契約は[ADR0090](../adr/0090-note-favorites-and-display-order.md)。
作業木はD:/NeNeFolio/worktrees/817-favorites-ui、対象実行はD:/NeNeFolio/outputs/817-favorites-ui、
恒久原本とClaude相談はD:/NeNeFolio/agents/favorites-ui-20261115。

## 調査と試行

- 08:03〜08:06 JST: 現在欄、ロードマップ、必読規約、関連ADR、waiver、checkoutとIssue/PRを照合。
  mainとorigin/mainは同じ667f9a7、clean。openは#809/PR810と保留#466/#634。GitHubのopen milestoneは0件なのでdocs/ROADMAP.mdを順序の正本にした。
- GitHub連携のIssue作成は403 Resource not accessible by integrationで拒否。既存gh認証で同じIssueを作成して#817。
  自動承認審査の拒否ではなく連携権限の不足。既存削除拒否領域への操作はしていない。
- 調査中の存在しないwaivers/index.md・keyboard_target.h・drawer_header.hとWindowsで展開されないrg path globは読取エラー。
  正しいREADME・accessible_kind.h・drawer_header_button.hとrgのdirectory指定へ修正した。
  worktreeコピー進行中のsource読取も一部未存在となり、作成processのexit0を確認して再読した。製品実行なし。
- ClaudeはRead/Glob/Grep/Skillだけを許可し、design:design-critiqueでソースと前回案を相談。PID4612の背景PowerShellで起動、exit0。
  原本はclaude-brief.md、run-design.ps1、claude-stream.jsonl、claude-stderr.log、claude-advice.md。
  途中JSONL最終行の読取1回は未完行でparseエラー。既存processを維持し、完了後のresultを保存した。
- Microsoft公式のmouse message署名を確認し、touch/penの合成moveで未表示星を有効にしない設計へ採用。
  実画面を見た助言とはせず、16 DIP・狭幅の文字幅・表示状態は製品候補で確認する。

## 実装と検証の現在地

ADRを製品編集前に記録。検索下の範囲選択、静かな右端の星、共通UIA/鍵操作、0件案内を実装した。
表示範囲と星の保存経路は既存のfolio_stateのまま、追加queryは全カテゴリの星総数だけ。
入力は実ノートを使わない合成fixture。確認中の実装を完了とは扱わない。

| 試行 | コマンド・結果 |
| --- | --- |
| configure-01 | 固定toolchainで`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`。child/wrapperとも0、入力snapshot不変 |
| build-01 | `cmake --build build --target folio_tests nenefolio favorites_native_tests`。child/wrapperとも1。drawerのUIA読取の複雑度11、主窓の矩形関数62行、UIA起動の複雑度12を検出。閾値は変えず、空状態のHelpTextをtree情報の組立、command実行を共通実行関数、RangeValue領域の寸法を同じ問い合わせへ分けた |
| build-02 | 同じ3対象の構築。child/wrapperとも0、入力snapshot不変 |
| unit-01 | `build/folio_tests.exe --favorites`。child/wrapperとも1。既存ui_text期待表に追加4IDが未収載でnullability sanitizerが停止。製品の翻訳3列はあり、テスト期待表へ4IDを追加した |
| native-01 | `ctest --test-dir build -R '^favorites_native$' --output-on-failure`。4.25秒、child/wrapperとも8。まだ星を保存していない初期fixtureに、テストが存在する台帳を要求したため停止。未表示星への初期タッチは状態不変を確認し、台帳の保存確認は星変更後に行う形へ修正 |
| build-03 | 同じ3対象を再構築。child/wrapperとも0 |
| unit-02 | `build/folio_tests.exe --favorites`。child/wrapperとも0。追加総数queryと、既存台帳/順序/保存失敗/移動/検索/文言の対象を成功。後続はUIだけのため再利用する |
| native-02 | 同じCTest対象。17.06秒、child/wrapperとも8。2行の並び替えと連続クリック抑止まで通り、Tabで停止。本文専用native_key_inputは宛先をRichEditへ固定しkeyupを送らないため、実focusのHWNDへkeydown/keyupを送る専用試験入力へ修正。画像原本はnative-02-evidence |
| build-04 | favorites_native_testsだけ構築。child/wrapperとも0 |
| native-03 | 同じCTest対象。30.79秒でtimeout、child/wrapperとも8。追加操作と既存本文回帰を同processに含めた試行。全範囲の成功とは扱わない。30秒上限は変えず、既存favorites_native（本文/保存）と追加favorites_ui_native（星/範囲の操作）へ分け、どちらも必須CIで実行する |
| build-05 | 3対象の構築。child/wrapperとも1。試験mainが61行となり上限を超えた。selectorの初期化を専用関数へ分けた |
| build-06 | 3対象の構築。child/wrapperとも0 |
| conformance-01 | `python eng/conformance.py --build-dir build`。child/wrapperとも0、違反0 |
| native-04 | 2つのfavorites CTest。本文は30.21秒でtimeout、UIは18.17秒で並び順の期待に失敗。中断された前の試験が保存した手動順をfixtureが復元していなかったため、開始時に優先順と両ノートの未登録状態を明示する形へ修正 |
| build-07 | native対象を構築。child/wrapperとも0。以降の検証exeはSHA256 b913eab48184551fe142e0e664cb71954faa28f860e1fb0e3033450e010bcfe0 |
| native-05 | `ctest --test-dir build -R '^favorites_native$' -V`。31.71秒でtimeout、child/wrapperとも8。1周目完了、2周目の星操作までを確認。追加したpump上限診断の出力はなく、paint loopとは断定できない |
| baseline-01 | #813で成功した以前のexeを専用D領域へコピーし、合成dataで30秒の診断実行。timeout、child/wrapperとも1。旧exeのSHA256は保存済み原本と一致し、今回変更だけの時間増加とは認定できない |
| native-body-90-01 | 外部の90秒上限wrapperで`--favorites`を診断。child/wrapperとも0、記録全体45.47秒。ただしstdout転送を指定していなかったため項目数は認定しない |
| native-body-90-02 | stdout/stderr転送を明示した同wrapperで`--favorites`。248項目成功、child/wrapperとも0、記録全体55.53秒。本文/選択/dirty/Undo/Redo/履歴/保存と再起動を確認 |
| native-ui-90-01 | 同wrapperで`--favorites-ui`。121項目成功、child/wrapperとも0、記録全体31.78秒。非表示星、touch署名、並べ替え/同座標連続押下、外/scroll/resize取消、UIA/tooltip、Tab/矢印/Space、検索ANDと0件focusを確認 |
| native-visual-90-01 | 同wrapperで`--favorites-visual`。180項目成功、child/wrapperとも0。実120 DPIで幅200 DIPの3言語×明/暗/注入HC、hidden/hover/focusの題名画素不変、96/144/192 DPIの字形と文字幅を確認 |

90秒wrapperはrepo外の診断用で、CTest/CIの各30秒制限は変更していない。上の記録全体時間はsnapshotも含む。
08:57のDディスク計測は使用率約137%、queue 2、約576KB/s。09:02にはqueue 9・転送0のsnapshotも観測した。
I/O待ちが疑われるが根因は未確定。Cへの作業コピー移動や他のprocess停止はしていない。
全試行でrecordingErrorsは0。修正のないunit-02とconformance-01は再利用する。

描画matrixは同じfixture/exeの`--favorites-visual`へ分け、実行原本から画像を確認する。
新しい範囲のtooltipは既存drawer_toolへ追加し、UIAと同じ完全な名前・矩形の試験を追加した。

native-01で実drawerのhidden/hover/focusの3画像を取得。実DPIは120、行高38px、drawer幅300px。
題名画素hashは3状態で一致（13682254194775762415）、星領域はhiddenとhover/focusで変化した。
原画像とCTest logはnative-01-evidenceへ保全した。全native試験の成功を意味しない。
2ノートfixtureを加えて並び替え直後の別行への誤操作を固定する。配置cacheのcursor値は更新されないので、
押下判定では既存の現在cursor queryを重ね、描画の実focus条件と一致させた。

自己レビューでhoverからの毎回のlayout確保を避け、既存UIA配置cacheへ接続した。hover変化時は旧/新の星矩形だけを無効化する。
ドキュメント探索の誤ったファイル名・PowerShellのglob、整形後の古い空白を前提にしたapply_patchは失敗し、実在ファイル/正しいcontextへ修正した。
いずれも製品起動やデータ変更を伴う失敗ではない。

描画確認では最小幅200の実drawerを3言語×明/暗/注入した高コントラストsnapshotで記録する。
96/144/192 DPIの字形はproduction icon_paintへ明示した描画矩形を渡す部品の確認、文字幅は同梱書体のGDI実測。
実ウィンドウのDPI変更・物理タッチ・OS高コントラスト設定の変更とは区別する。

visual-evidenceにBMP15枚・PNG派生・撮影元更新日時/hashのmanifestと比較画像を保全した。
3言語×3配色、hidden/hover/focus、2種類の0件、3縮尺の星を目視し、範囲ラベルの欠け・星とcursorの重なりがないことを確認。
最小幅の既存英語カテゴリ見出しは末尾が切れる。新しい範囲ラベルは全言語で収まり、既存見出しの寸法は今回変更しない。
96/144/192 DPIで最長の日本語ラベルは60/90/120px、項目幅84/126/168pxに左右余白を含めて収まった。

規則IDごとの自己レビューで、状態はfolio_state、描画/当たりはdrawerの共通寸法、UIA/鍵/pointerは既存actionへ集約したことを確認。
閉じたenumの分岐、戻り値、本文非変更、改訂/capture/矩形照合、追加queryの所有者、3言語表と空状態の対応を点検した。
`git diff --check`は成功。最終headの`pwsh -NoProfile -File ./eng/check.ps1`は#817のPR必須CIを正本とし、
その結果・main SHA・通常exe反映・整理はD:/NeNeFolio/agents/favorites-ui-20261115/completion.jsonへ確定記録する。
製品の版上げ・配布/Store変更は範囲外。追加waiverなし（既存WVR-0001継続）。
