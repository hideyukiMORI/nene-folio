# #377 — モーダル中の本文と検索欄のテーマ背景

日付: 2026-10-25。Issue: [#377](https://github.com/hideyukiMORI/nene-folio/issues/377)。
実装前判断は[ADR0066](../adr/0066-disabled-pane-theme-background.md)。
規則: ARC-001/004/005/008/011/012、C-002/005/008/012/013/016/017、QLT-008/012/013、GIT-001〜004。
Waivers: none。保存schema・依存・ゲートの変更なし。

## 問題と変更

設定を開いた際に本文/絞込欄が灰色になることをhideの実機画像で確認した。
EDITはdisabled時にWM_CTLCOLORSTATICへ切り替わるが、現行はWM_CTLCOLOREDITだけを処理していた。
RichEditはnativeの無効化処理でシステム色へ切り替わり、EM_SETBKGNDCOLORだけでは変更できない。
前回#375の画像確認では、この表示不良を拾えていなかった。

- `folio_window.c`: EDITの二つの色通知を一つの既存配色関数へ集約。対象外は実際の受信HWND/messageへ戻す。
- `note_pane.c`: native WM_ENABLEはそのまま通し、disabled時だけWS_EX_TRANSPARENTを設定する。
  本文の地は既存subclassが実効paletteから塗り、WM_PAINT/文字/選択/IMEはRichEditが所有する。
  disabled中の既存paintは本文client全体をerase付きに広げ、nativeの部分的な選択文字再使用で文字が欠けないようにする。
  有効時は通常のnative描画へ戻す。背景色は作成/再着色の同じ合流で更新し、Undo再開失敗時の条件も維持する。
- ADR0066、ADR索引、モジュール責務、現在状態、日報/引き継ぎを更新する。

## 診断で却下した案

親だけをdisableするとRichEditへSetFocusでき、native UIAもenabledだった。
WM_ENABLEをnativeへ渡さない案は色とOS/MSAAの無効状態を保つが、native UIA IsEnabledがtrueになった。
SES_CUSTOMLOOK/SES_EX_NOTHEMING/SES_EXTENDBACKCOLORも灰色を防がなかった。
これらは独立Win32 probeだけで確認し、製品へは入れていない。

## 対象検証

恒久証跡: `D:/NeNeFolio/agents/modal-theme-fix-20261025/`。
作業checkout: `D:/NeNeFolio/worktrees/377-modal-theme-background`。
生成fixture/出力: `D:/NeNeFolio/outputs/modal-theme-fix-20261025/`。
全fixtureはCreateDesktop/SetThreadDesktopで専用desktopを作り、入力desktopへ切り替えず、合成データだけを使用した。
実機の稼働アプリ・実入力・利用者データは操作していない。外部fixtureのUIA client用threadは製品コードに追加していない。

実行したコマンド:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target nenefolio
python eng/conformance.py
git diff --check
pwsh -NoProfile -File D:/NeNeFolio/agents/modal-theme-fix-20261025/build-fixture.ps1 -Baseline -Case smoke
pwsh -NoProfile -File D:/NeNeFolio/agents/modal-theme-fix-20261025/build-fixture.ps1 -Case paint
pwsh -NoProfile -File D:/NeNeFolio/agents/modal-theme-fix-20261025/build-fixture.ps1 -Case full
```

- Debug build/clang-tidy成功、conformance 0 violations、空白検査成功。
- 修正前main `17648c5` の製品fixtureでは、通常色は一致し、設定時は本文/絞込ともRGB240/240/240となり2件の期待どおりの不合格。
  monalisaの正しい本文色はCOLORREF `0d0b12`、絞込色は`09070c`。`baseline-smoke-result.log`に保存した。
- 閲覧/編集、設定、dark/light preview、取消、resize、名前入力＋入れ子の失敗面、復帰を製品fixtureで確認する。
  本文/選択/dirty/Undoと実Undo、native HWND/UIA/MSAAの無効状態、SetFocus拒否、配色画素と画像を対象にした。
  最終full fixtureは142 checks / 0 failures、process exit 0。`candidate-full-result.log`と`candidate-full-exit.json`に記録した。
- native画面の部分更新と同じWM_PAINTによる全面更新をDIBで比較。暗/明色・2サイズの選択/解除で9比較の差分はすべて0。
  対象のpaint fixtureは11 checks / 0 failures、exit 0。
- 最初の画面とWM_PRINTCLIENTの直接比較には文字描画差が含まれていた。その比較を同じWM_PAINT同士へ修正した後、
  部分更新の残像680px、更新領域だけを消した場合の選択文字欠け369pxを実際に検出し、製品側の描画を修正した。
  初期ログ/画像も保持し、合格へ読み替えていない。
- fixture準備でSDK CヘッダのCOM宣言不足とUIA定数の重複定義を修正した。製品の定数やゲートは変更していない。
  初期runnerの55秒期限はGetPixelを大量に呼ぶ診断とnative UIAを含むfull実行で超過したが、fixtureはその後完了した。
  比較をDIBへ変更し、runnerは20秒単位の待機で終了コードまで記録するよう修正した。最終再実行ではassertionとexit 0の両方を確認した。
  診断/runnerの不備を製品テストの失敗や合格として数えない。

最終full fixtureの結果、最終PR headの必須CI、main統合、通常exe反映、整理は同directoryの
`verification-final.json` / `completion.json` と実際のPR/CI状態を照合する。
必須CIの正典コマンドは `pwsh -NoProfile -File ./eng/check.ps1`。
独立レビューは設計と完成差分に限定し、実装と検証は単体で実施した。

## 限界

GDI/専用desktopの画像とnative APIの検証であり、物理IME・実reader操作・DWM呈示・物理DPI跨ぎは未測定。
native RichEditのdisabledはプログラムによる全書換えAPIの禁止ではなく、独立probeでは元のdisabled状態でもUIA Value.SetValueが成功した。
今回この既存native挙動を変更せず、UIAのIsEnabledと実フォーカス、既存業務guardを維持した。
無効時の既存更新を本文全体へ広げるため、巨大本文のモーダル中再描画コストは別の規模境界として残る。
通常編集時の背景/描画経路は変更していない。#369の初回全文I/Oは別件。
