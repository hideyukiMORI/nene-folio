# #338 — 次ベータ0.2.0候補の確認

2026-10-18、hideの「やりやすい順番で進めて」に基づく配布準備。
Issue: [#338](https://github.com/hideyukiMORI/nene-folio/issues/338)。規則: FR-014 / ADR0045 / QLT-008・012・013 / GIT-001〜004。
基点main d63f655。唯一の版入力`CMakeLists.txt`を0.2.0へ更新し、[変更履歴と更新手順](../release/0.2.0.md)を用意する。
schema・依存・ゲート変更なし、Waivers: none。公開tag/GitHub Releaseはこの準備に含めない。
恒久証跡: D:/NeNeFolio/agents/beta-candidate-20261018/。

## 完成候補の対象検証

正典`eng/package-release.ps1`でReleaseを生成し、版・manifest・AMD64・import・font・同梱一覧・ZIPを検証した。
最初のbuildは成功したが、この品質記録への参照先が未作成だったためCNF-006で停止した。成果物の公開・成功扱いはしていない。
参照先を作成してから新しい出力先で正典コマンドを再実行し、exit 0で成功した。

実行コマンド:

`pwsh -NoProfile -File ./eng/package-release.ps1 -WorkDirectory D:/NeNeFolio/outputs/338-beta-candidate/candidate-work-2 -OutputDirectory D:/NeNeFolio/outputs/338-beta-candidate/candidate-package-2`

conformance 0違反、symbols 2 libraries / 0違反。版は文字列0.2.0、Windows固定版0.2.0.0、AMD64。
manifest、宣言内のOS DLL 14種、同梱11ファイル、fontのhash/license、ZIPと展開結果が一致した。

- 検証候補ZIP: 22,786,385 bytes、SHA256 `ef7eeb2b94c1b439d262994ecb2e936a404d0e572e1874c7b8da66acf5feb744`。
- 検証候補exe SHA256: `07305e280d34e1ba9ab39ce69c77dbb2e4c7ce175ae344731e263110f1504665`。
- 入力は作業中の候補なので`sourceDirty=true`を記録し、source-inputs.jsonの全入力hashを保存。後の文書変更/commit/統合で関係入力の一致を確認する。
- PR最終headの必須CI成功後、統合済みclean mainから正典コマンドで最終候補を生成する。既存の検索・DPI対象試験は入力一致を照合して再利用する。

最終候補の由来・hash・起動・通常exe反映・整理は同所`completion.json`に実結果を記録する。
これは公開操作ではなく、公開tag/GitHub Releaseは作成しない。

## 実機の対象

Windows 11 Pro 10.0.26300。現在4モニターに120 / 144 / 168 DPI（125 / 150 / 175%）が存在することをread-only APIで確認。
専用展開先の実候補exeをdataなしで起動し、設定の版表示と実モニター間移動後のDPI/最小560×360論理画素を確認した。
画面取得とキー送信は既存のPID指定toolsを使う。通常アプリ・ノートやOS設定を変更しない。

実行は外部`run-candidate-ui.ps1 ... -Label candidate2 -DpiTransitions`、16:34:09〜16:34:22、専用PID41716、exit 0。
実exeを専用desktopに起動し、既存`tools/post-keys.ps1`と`tools/capture-window.ps1`へPIDと主窓の完全な題名を渡した。
画面はPrintWindowで保存し、実際に画像を見て`Version 0.2.0`と設定面の外形を確認した。

| 移動順 | 実画面 | 実DPI | 最小窓の物理画素 | 観測 |
| --- | --- | ---: | --- | --- |
| 0 | DISPLAY4 | 120 | 700×450 | 版表示が読める |
| 1 | DISPLAY1（左側の負座標） | 144 | 840×540 | 版表示が読める |
| 2 | DISPLAY2 | 168 | 980×630 | 版表示が読める |
| 3 | DISPLAY3（上側の負座標） | 144 | 840×540 | 版表示が読める |
| 4 | DISPLAY4へ復帰 | 120 | 700×450 | 版表示を保って復帰 |

OSが報告するGetDpiForWindowと実WindowRectを各移動後に照合した。WM_DPICHANGEDの偽メッセージ注入ではない。
移動はSetWindowPosによるもので、物理マウスのドラッグ試験ではない。最小窓の設定項目には既存のスクロール領域があり、全項目同時表示を主張しない。
初回の観測ではpost-keysの題名を省略し、同PIDの補助窓を選んだため設定を開けず終了待ちも失敗した。
その候補だけを正しい主窓へ`:q`を送って通常終了し、入力先指定を直したcandidate2を採用した。初回の版表示画像は受理に使わない。
専用desktopの起動器にSwitchDesktopは無い。foreground HWNDの前後変化は記録したが、その原因や全操作中のforeground不変を証明していない。

物理IME候補操作、人の聴取、全OS/DPI/readerを受理したとは扱わない。
既存のUIA/native/reader成功結果と[#336のRelease検索応答](2026-10-18-release-response.md)は、関係する実装不変の範囲で再利用する。

## hideによる実機確認の項目

1. 候補の専用確認データで日本語を入力し、変換候補の移動・確定・取消を行う。本文、全ノート検索、ノート内検索を対象とし、未確定の文字で検索が先走らず、確定後に追随することを見る。
2. 変換中に表示切替キーを押し、変換先や入力文字が別の面へ飛ばないことを見る。
3. 普段使う読み上げ環境で設定・検索結果・保存失敗時の案内を聴き、順番と重複の有無を確認する。既存のreader生成発話ログと人が使いやすいと判断する受理を区別する。

実行可能な候補パス・版・hashと、機械側で確認できた条件は完成時の記録にまとめる。
