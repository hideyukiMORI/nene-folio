# ADR 0056 — UI AutomationはUIスレッドのCOM公開と失効可能な参照で構成する

- 状態: 受理（#319の実装契約。受理試験が揃うまでは製品対応はplanned）
- 日付: 2026-10-17
- Issue: [#319](https://github.com/hideyukiMORI/nene-folio/issues/319)
- 規則: ARC-001/002/003/004/008/010/011、C-004/006/007/011/012/013/016/018、QLT-008/012/013
- 上位判断: [ADR0055](0055-accessibility-and-system-contrast-scope.md)のA

## 接続前の実測

Windows 11 build 26300 / x64でC23の独立probeを実行した。UIスレッドをSTAとして初期化し、
ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreadingを返す自前fragmentと、標準EDIT/RichEditを同居させた。
別プロセスのMTA clientから要素・Invoke・標準Value/Textを取得でき、104回のprovider呼出しで別スレッド実行は0。
IAccPropServicesの直接注釈によって両入力欄の名前を補い、元の値/本文を取得できた。
providerを切断後、保持していたInvoke参照はUIA_E_ELEMENTNOTAVAILABLE（0x80040201）になった。
最初の非表示窓は標準子窓がtreeに出なかったため、非アクティブ表示の試験窓へ修正して確認した。
このprobeは製品の操作/発話試験を代替しない。入力とログはD:/NeNeFolio/agents/319-basic-accessibility/。

## 決定

1. UIの所有者がCOM STAの初期化/終了を対にして持つ。providerはUseComThreadingを返し、applicationの操作はUIスレッドへ集約する。
   ProviderOwnsSetFocusも指定し、modal/IME/終了中の可否を確認してから既存のfocus経路へ渡す。
   製品へworker thread、独自marshaling、free-threaded marshalerを追加しない。
   [ProviderOptions](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/ne-uiautomationcore-provideroptions)の契約を使う。
2. 自前要素はIRawElementProviderSimple/Fragment、HWNDに結び付く根はFragmentRootを実装する。
   主窓のヘッダ/本文操作列と、drawer窓の固定操作/Treeを同じprovider実装で公開し、各根のhostはそのHWNDだけとする。
   標準EDIT/RichEditは既存のHWND子のまま残し、IAccPropServicesの直接注釈だけで名前を補う。
   独自MSAA providerや標準Text/Valueを再実装しない。
   [server-side provider](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-serversideprovider)と
   [直接注釈](https://learn.microsoft.com/en-us/windows/win32/winauto/using-direct-annotation)に従う。
3. ui/win32のproviderは参照計数と公開先の寿命だけを所有する。画面状態はfolio_window/drawer_window/applicationの現在値から導く。
   窓の破棄では公開先を先に切り離し、UiaDisconnectProviderを通して所有参照を解放する。
   clientが保持した参照から破棄済みのfolio_window/stateへ戻れない。参照計数の0だけでproviderを解放する。
4. 行の識別はapplicationの行識別改訂番号と行参照で結ぶ。並べ替え/追加/改名/削除/絞込で改訂を更新する。
   描画から導くlayoutの改訂は別に持ち、展開/折畳ではlayoutだけを更新する。カテゴリ自身の参照を保って展開変更を通知し、
   非表示になった子行の参照は現在のlayoutに存在しないものとして拒否する。
   過去の行識別改訂に結び付いた参照は別の行へ転用しない。改訂番号の上限では行公開を失効させ、番号を再利用しない。
   閲覧/編集、cursor、選択、スクロールの変化だけでは行の同一性を更新しない。
5. applicationは既存cursorを選択とは独立した単一の行参照へ拡張する。フォーカス意図は可視の行だけを受け付け、保存/読込を行わない。
   選択や並べ替え/削除に伴うcursorの更新もapplicationが行う。UIA用の第2のcursorは持たない。
6. 実Win32 focusが主窓にある場合だけ、ヘッダ/索引/本文操作列内の操作位置をui/win32の一時状態で区別する。
   native入力欄のfocusはHWNDから導く。F6/Shift+F6とTab/Shift+Tabは同じ可視操作/矩形を使い、本文のTab/IMEは保持する。
   モーダル・本文投入・終了処理中は背後へのUIA操作も拒否する。単なる列挙/focusで本文や履歴を書かない。
7. UIAに必要なWindows同梱ライブラリだけをeng/architecture.jsonとnenefolio_system_linkで宣言する。
   COMのQueryInterface/SAFEARRAY等に必要なvoid pointerはSDK境界だけで受け、状態/コールバックの自前文脈は具体型にする（C-006）。
   SDKのvtable宣言に合わせた関数は公開ABIの境界であり、製品内の汎用データバッグにはしない。
   Cで定数の外部定義を含むUIAutomation.hはaccessibility_node.cだけで読み、他のUIファイルは具体型の公開境界を使う。

## 受理

ADR0055 Aと#319の条件を維持する。UIA property/pattern/eventとkeyboardの対象試験、参照保持/破棄/再入、
未保存文書からSetFocusとSelectを分けた試験、実readerの発話、最終head必須CIを揃える。
OS高コントラストと補助面の全面公開は#320/#321。Waivers: none。

2026-10-17時点の途中実装・実施済み検証・残件は[品質記録](../quality/2026-10-17-basic-accessibility-checkpoint.md)を参照。
実画面/reader/寿命境界の受理試験は未完了であり、この時点のcommit/pushは製品対応完了を意味しない。
