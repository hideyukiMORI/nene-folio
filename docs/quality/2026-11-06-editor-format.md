# 編集本文の段落書式キー — 2026-11-06

Issue #709。FR-006、ARC-001/004/008/011/012、C-002/005/008/014/016/017、QLT-008/012/013、ADR0006/0019/0054。
追加waiver none、既存WVR-0001継続。主担当が調査・実装・検証し、paste_reviewが読み取り専用で独立静読した。

## 再現と修正

既存RichEditは編集時もRTFを受け取る。公式の[標準キー一覧](https://learn.microsoft.com/en-us/windows/win32/controls/about-rich-edit-controls#rich-edit-shortcut-keys)と製品の入力経路を照合し、
`test-route-02`でCtrl+Rが左揃え1から右揃え2へ、Ctrl+2が行間20から40へ、Ctrl+5が20から30へ変わることを確認した。
同じ診断でCtrl+H/J/K/Lは既存の移動アクセラレータとして処理され、本文と書式を変えない。
Ctrl+Jを段落揃えの不具合とした初期診断は、非対話WindowStationで本文が非表示になりHACCELを通らなかったため採用しない。

`folio_window_translate`の既存session・製品キー・本文移動処理の後で、EDIT本文宛てのCtrl+R/2/5だけを消費する。
Shift/Altを含む組合せ、別の入力欄、閲覧modeへは適用しない。[TranslateAcceleratorの契約](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translateacceleratorw)と同様に、
処理済みキーをTranslateMessage/DispatchMessageへ再投入しない。WM_CHARの値だけで拒否するとCtrl+JとCtrl+Enterを区別できないため、その案は不採用とした。
本文・選択・Undo/Redo・変更印を設定し直さず、保存形式・公開API・RTF投入・依存・ゲートは変更しない。

## 確認した範囲

CTestの`editor_format_native`を既存実製品fixtureへ追加。専用dataと非アクティブの専用desktopを使い、通常のdesktopへ切り替えない。
clipboardを操作する`paste_native`だけは従来どおり別WindowStationを使う。format試験はclipboard操作をしない。
`folio_window_translate` → TranslateMessage/DispatchMessageの順とthread keyboard stateで入力する。物理キー・物理IMEの試験ではない。

- Ctrl+J/R/L/1/2/5/B/U、Ctrl+Shift+A/L/period/comma/左右の14条件で本文・文字書式・段落書式を照合。
- Ctrl+R/2/5で未保存本文・選択・変更印・Undo/Redoの可用性を保持し、前後の入力をUndo/Redoできる。
- Ctrl+H/J/K/Lの実移動、Ctrl+Enter、Ctrl+A、単語選択、日本語文字、Tab/改行、Ctrl+Shift+2/3、Ctrl+Eの閲覧装飾と編集復帰を確認。
- 専用ノートの保存履歴が変わらない。30秒timeoutと既存のdata resource lockを維持する。

`test-candidate-03`は339checks成功、native1.87秒/CTest1.88秒、child/wrapper0、記録エラーなし。
共用入力helperを変更した直接境界の`paste_native`は`test-candidate-01`で414checks成功。そのhelperと製品実装は以降不変なので再実行しない。
後続のformat試験訂正はCtrl+Aの最終段落マークを含む選択範囲だけで、貼り付け契約を変えていない。
Windows11 Pro 10.0.26300、Msftedit10.0.26100.9949。固定toolchain/入力snapshotは各runに収載した。

## 全試行と限界

原本は`D:/NeNeFolio/agents/709-editor-format`と同名outputs、全体journalは`D:/NeNeFolio/agents/quality-20261106/journal.md`。
各runのsource/exe/設定snapshot、raw stdout/stderr、child/wrapper終了値を保持する。

| 試行 | 結果と判断 |
| --- | --- |
| build-baseline-01/02、build-diagnostic-01 | 正典Debug/tidy成功 |
| test-baseline-01/02、test-diagnostic-01 | 30秒枠を超えて停止。保全I/Oとの重なりを観測したが原因を断定しない。キーの良否は未判定 |
| test-diagnostic-02 | 保全終了後にキーまで到達。window再生成によるmode不整合を検出し、同じwindowと正典discardで初期化へ訂正 |
| build-baseline-03/04 | 成功 |
| test-baseline-03 | 非対話stationでのfocus期待に失敗。既存discard回帰は変更せず、新診断の前提を見直した |
| test-baseline-04 | 書式変化を検出。ただしCtrl+JはHACCEL未処理であり最終根拠にはしない |
| build-candidate-01 | 新試験のsignednessでcompile失敗。候補未実行。CHARRANGEへ訂正 |
| build-route-01/02 | 成功。独立静読の指摘を受け製品候補を外して入力経路を診断 |
| test-route-01 | body非表示/無focusを記録。通常のHACCEL経路の根拠にはしない |
| test-route-02 | 専用desktopで既存移動キーとCtrl+Enterを確認。Ctrl+R/2/5だけ段落書式を変える修正前red |
| build-candidate-02/03 | 成功。Ctrl+R/2/5だけTranslateMessage前に消費する候補 |
| test-candidate-01 | paste414checks成功、formatはCtrl+A選択範囲の期待に失敗 |
| build-selection-01 / test-selection-01 | build成功。Ctrl+Aが本文26単位に対して最終段落マーク込み0..27を返すことを観測し、期待を訂正 |
| build-candidate-04 | 成功 |
| test-candidate-02 | build完了前に記録を始めた誤りをsnapshot検査が拒否。child未起動、wrapper125。変更中font/旧exe/未生成build結果を成功扱いにしない |
| test-candidate-03 | build完了と入力固定後の対象試験339checks成功 |

ASan起動時の`interception_win: unhandled instruction`一行が一部のraw出力にある。計装を無効化せず、隠さず収載した。
この警告と初期timeoutを同一原因とは断定しない。物理IME/全配列/全OS/viewport、外部からの直接WM_CHAR/TOM操作は未検証。
最終head必須CI・main統合・通常版反映・全保全整理の確定結果はagents内`integration-completion.json`へ収載する。

## 統合・通常反映・全保全の確定

PR710の最終head `357c2ac980b1f572d0c680ab4ae8f6b137518964`で、必須CI42129740643のfull-gate/checkが成功した。
main `8ccbf29a232bd25d03102be3ce8f0fe4f9e05851`へ正典統合し、通常版をprepare-real-machineで反映。
通常exe SHA256は`caee9596c1f73f40dc959737cb2309097e860cacf8ee2ee03c9789bd2c593174`。実data32fileのbytes/hash/作成更新日時は不変、通常appは起動していない。

作業木と全outputsの10611filesをarchive内の全hashで照合した。archive SHA256は`4e922e3f843aa58ac34914ca8c5f90a9347123f604433d6548a8e468886761d7`。
絶対path、同一squash tree、clean/ignored、全file不変、links/稼働参照を確認して2領域を整理し、branch/commitを保持した。
完了収載complete01はmainの改行変換によるtest sourceのbytes差で停止した。原本hashは保存済みのreview値と一致し、
mainと保全sourceのCRLFだけをLFへ正規化したbytesが完全一致することを確認した。complete02は成功し、失敗runも記録へ含めた。
確定原本は`D:/NeNeFolio/agents/709-editor-format/integration-completion.json`。
