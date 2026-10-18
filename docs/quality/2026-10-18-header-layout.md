# #344 — 共通ヘッダーの新規ノートと本文操作列の整列

2026-10-18、hideの推奨案への反映指示に基づく。Issue: [#344](https://github.com/hideyukiMORI/nene-folio/issues/344)。
基点main 89d8a81（#340/#342統合済み）。設計相談だけ3ペルソナの独立レビューを使い、実装・自己レビュー・対象確認は単体で実施。
規則: FR-018/029/034/039、ARC-001/004/011、C-002/008/012/017/018、QLT-008/012/013、GIT-001〜004。
設計判断は実装前にADR0059へ固定。schema・依存・ゲート変更なし、Waivers: none。
恒久証跡: D:/NeNeFolio/agents/header-layout-20261018/。

## 変更境界

既存NEWコマンドへの入口を共通ヘッダーへ移し、ドロワーの40px帯と本文操作列の重複を撤去した。
狭幅は紙＋の図形へ縮め、同じ矩形を描画・click/capture取消・hit test・tooltip・UIAで共有する。
新規のUIA親/列挙/Tab/F6を移動し、名前・Button/Invokeと既存のmodal/IME抑止を維持する。
本文操作列は保存を先頭にしてパンくずと同じ固定36 logical pxへ揃え、行番号の桁には追随させない。
検索の保存・作成先・無題の離脱確認・作成成功後だけ一覧から広げる処理は既存経路を利用する。

## 完成時の対象検証

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` と `cmake --build build --target nenefolio`: exit 0。ASan/UBSan/nullability、clang-tidyを維持。
- `cmake --build build --target folio_tests` と `./build/folio_tests.exe --commands-ui`: exit 0。3言語の追加tooltip文言を含む既存対象unit。
- `python eng/conformance.py`、変更C/Hへのclang-formatと`git diff --check`: 成功。最終文書差分も確認する。
- 外部build-probe.pyは実compile_commandsの全flags・4製品lib・resourceを使う。製品へ試験専用入口を増やしていない。
- header2.log / header2-desktop.json: 専用desktopのPID2100、exit 0。前景HWNDは前後199600で不変。
- 実120/144/168 DPI × 日英中 × 3表示の27条件を最小幅で確認。全条件で重なりなし・24 logical px以上のdrag空白、HTCAPTION/HTCLIENT、tooltip登録矩形/名前/Ctrl+Nが一致。
- 320幅の一覧は図形へ縮め、560幅の両方/ノートは文字を表示。通常幅1200物理pxを含む9枚を実際に確認し、新規・表示3択の欠けなし、本文操作列とパンくずの整列を確認。
- UIA hit/Bounds/host/親・新規要素が一つだけ・列挙順と実COMのName/Button/Invoke取得を確認。F6/Shift+F6/Tab/Shift+TabとSpaceも成功。
- mouse押下/解放で作成、外へ解放・capture喪失では作成なし。実無題dialogのキャンセルで本文/選択/Undo/modified・表示・窓幅を保持。
- 実COM Invokeはqueueへ受け付け、pump後に作成成功して両方表示/本文focus。最終write/archive追加0・無題確認1。

初回native実行header1は外部probeのRelease呼出型がIUnknownとIInvokeProviderで不一致となりUBSanが停止した。
probeを実interface型へ修正し、製品コードとsanitizer設定は変更せずheader2で全対象を確認した。初回を成功扱いしない。
初期編集スクリプトの定義照合assert停止も証跡に保持。ASanのinterception_win起動診断は残るがheader2に対象違反はなくexit 0。
既存core/OOM/検索性能/RTF/reader試験は再実行しない。人の読み上げ聴取・物理IME確認の代替にはしない。

## 統合と反映

最終headの必須CI成功後に統合する。source-inputs.jsonとPR/mainのtree一致で対象結果を再利用する。
clean mainから通常exeと0.2.0更新候補を作り、別の合成fixtureで短い起動と画面を確認する。
新しい確認先はD:/NeNeFolio/check/2026-10-18/344-header-refresh/。稼働中の旧候補とユーザーデータは操作しない。
最終head/CI/main・exe/ZIP hash・起動・整理は同証跡フォルダのcompletion.jsonへ実結果を記録する。
公開tag/Releaseなし。過去の拒否済み削除を再試行しない。Waivers: none。
