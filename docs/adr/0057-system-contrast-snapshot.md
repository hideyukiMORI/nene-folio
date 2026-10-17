# ADR 0057 — OS外観を失敗と区別したsnapshotで受け、実効paletteを一度だけ解決する

- 状態: 受理（実装・対象検証済み。最終CI/統合の確定先は品質記録）
- 日付: 2026-10-17
- Issue: #320
- 規則: ARC-001/002/003/004/007/008/010/011、C-002/003/012/018、QLT-008/010/012/013
- 上位判断: [ADR0055](0055-accessibility-and-system-contrast-scope.md)のB

## 決定

1. 既存appearance_portのread_themeを、結果enumとappearance_snapshotを返すreadへ置換する。
   snapshotはOSの明暗、通常/高コントラスト、window/windowText・buttonFace/buttonText・
   highlight/highlightTextのRGB対を持つ。色は独立に有効な値で、通常時は使わない。
   SPI_GETHIGHCONTRASTとGetSysColorを呼ぶのはappearance_adapterだけ。
   既存のAppsUseLightTheme欠落時のLIGHT既定は維持する。SPI取得失敗はUNAVAILABLEで、outを変更しない。
2. folio_stateが最後に採用できたsnapshotと実効theme_paletteを所有する。
   coreのappearance_snapshotが固定表またはOS色から同じ役割へ一度だけ解決し、UIはCOLORREF変換、
   RTFはそのpalette内のRTF色を使う。127固定配色と保存theme_choiceは維持する。
   paletteの比較はRGB値と高コントラスト状態による。enumのテーマ名だけで変更を判定しない。
3. 更新ではRTFを先に準備し、成功時だけsnapshotと実効paletteを採用する。失敗時は直前の有効値を保つ。
   初回SPI取得失敗時は通常LIGHTを明示的な退避値とし、保存済み選択を通常規則で解決する。
   起動は続け、三言語の取得失敗通知を一度表示する。以後の取得失敗は設定面の状態に残し、成功で解除する。
   OS変更そのものは設定・本文・履歴を書かない。設定の明示変更だけは既存の書込み成功後採用を維持する。
4. 高コントラストでは本文・見出し・引用・リンク・コード・番号・カテゴリ装飾をwindow対へ、
   操作の札をbutton対へ、選択をhighlight対へ写す。リンク等は既存の下線/字形でも区別する。
   固有カテゴリ色とフェードは描画に使わず、設定の暗幕は不透明にする。設定には適用中と表示する。
   保存色とテーマの識別子は変えない。テーマ候補の色見本も適用中のOS色で描き、候補名で選べる。
5. 主窓でWM_SYSCOLORCHANGEとWM_SETTINGCHANGEを受け、同じ更新へ合流する。
   後者はlParamが無い通知も含めて再取得する（Windowsの推奨）。同じpaletteではRTF再生成/再投入を行わない。
   RichEdit投入中とOS終了処理中は既存の本文保護を優先し、更新要求を一つにまとめて安全な時点で再取得する。
   編集の再着色は既存TOM Undo停止経路を使い、本文・Undo・変更印・選択・表示論理行を保持する。
   再着色はtyped結果を返し、TOM取得/停止の失敗は背景も保持する。書式失敗は旧色を復元し、
   本文readinessを変えず未適用として残す。同じsnapshot通知/テーマ再選択でも再試行する。
   Undo再開失敗はnote_paneが未再開状態を所有し、本文を保持して編集だけ止める。
   次の再着色/本文投入では再開を先に回復し、成功前に再停止/再投入しない。
   次の対象への投入要求を回復不能で拒否した場合は、applicationが採用済みの対象と旧本文が違い得る。
   その時点は既存の投入失敗/UNAVAILABLEへ移し、旧本文を新対象へ保存させない。
   tomSuspend/tomResumeの特殊Countは実MsfteditでS_FALSEも正常復帰するため、FAILEDで判定する。
   通常Undoの「指定数の取消が足りない」判定を停止/再開へ持ち込まない。実Undoによる保持を検証する。
   先行した本文投入失敗の復旧時も、未適用配色を一度通知して背景と新RTFを揃える。
6. 名前入力/失敗面は既存dialog_themeを共有し、開くときに実効paletteを受ける。
   OS色通知で開いている面も所有者の更新後の値を取り直す。標準確認と共通色選択はOSの描画を維持する。
   user32はappearance_adapterの宣言済みOS依存へ追加する。外部runtime・保存schema・ゲート基準は変えない。

## 受理条件

#320とADR0055 Bを維持する。任意の明暗2組の注入、通常→HC→HC内変更→OFF、固定/system、
未保存EDIT/VIEW、設定と名前/確認/失敗面、取得/準備/投入失敗と終了再入を対象検証する。
実OS ON/OFFは復元を記録できる環境で行い、前後値・画像・復元結果を残す。
対象検証だけで完了にせず、最終PR headの必須CI・main・通常exe反映・作業物整理まで確認する。
Waivers: none。

## 検証記録

[2026-10-17の品質記録](../quality/2026-10-17-system-contrast-checkpoint.md)に対象コマンド、
実RichEditの347項目、実OS ON/OFF、初回失敗と復元の差分、独立レビューの修正を残す。
Windowsのバッファと特殊Countの戻り値は実APIで確認し、未使用のOS既定配色名を製品で所有しない。
システム色の役割は[Microsoftの指針](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter)、
通知の再取得は[WM_SETTINGCHANGE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-settingchange)に従う。
