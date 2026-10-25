# ADR 0064 — モーダルは内容操作を止め、主窓の移動と終了要求を残す

- 状態: 受理（#373の実装前判断）
- 日付: 2026-10-25
- Issue: [#373](https://github.com/hideyukiMORI/nene-folio/issues/373)
- 規則: ARC-001/004/005/010/011/012、C-002/005/008/012/013/016/017、QLT-008/012/013
- 補正: ADR0035/0060の主窓全体の無効化と暗幕、面の高さ/位置。ADR0050/0053/0061の保存・終了取消・未確定テーマの契約は維持する。

## 根拠

hideは、モーダル中にも主窓のヘッダーから窓を動かし、閉じられることを求めた。
現行は設定/テーマの暗幕が全clientを覆い、hit_testもHTCLIENTへ落とす。全補助面中の主窓×はUIA共通可否で無効。
native DialogBoxは主窓ownerをOSが無効化し、別のnative depth guardも入力を拒否している。
本文の操作を止めることと、窓の管理を止めることを一つにした設計を変える。

独立Win32診断では、主窓が所有する非表示top-level popupをmodal ownerにすると、DialogBox/入れ子/ChooseColor/MessageBoxのOS同期loopを保ち、
主窓有効・内容欄無効・移動追従・最小化復元・取消後の主窓生存が成立した（109 checks/0 failures）。
これは製品保存・UIA・物理入力の検証ではない。runnerの期限により最終process exit codeは未捕捉。
根拠と失敗を含む診断は `D:/NeNeFolio/agents/gui-responsiveness-20261025/modal-input/`。
デザインリナの批評により、通常終了前にchild面やテーマpreviewを取消す案はADR0061に反するため採らない。

## 決定

### 1. 窓の操作と内容の操作

- 主ヘッダー空白の移動/ダブルクリック最大化、窓縁のresize、OSの移動/サイズ/最小化/最大化/復元、主窓×を補助面中も使えるようにする。
  shell操作の可否を一つのqueryから描画・pointer・UIA・tooltipへ渡す。内容コマンドの可否は既存面の契約を維持し、本文・一覧・新規・表示切替等を一緒に有効化しない。
- 暗幕は主ヘッダー下の作業領域に限定する。主窓×と空白は通常の見た目、無効な内容部品は既存disabled表示とし、無効なhover/tooltipを出さない。
- 主窓hit_testは面別にHTCLIENTへ落とさず、通常と同じgeometryでshellを判定する。Win32の開いたsystem commandを丸ごと許可せず、窓操作に必要なものだけを通す。
- modalのTab順へ主ヘッダーを混ぜない。面の入力focusとdialog managerを維持する。子面中のAlt+F4/Alt+Spaceは既存rootのOS経路、native面のAlt+F4/Esc/×はその面の取消。

### 2. native ownerの唯一の境界

- `ui/win32`の専用 `modal_owner` が、主窓にownedされた非表示top-level popupを所有する。主窓を公開する前に作り、主窓の資源と一緒に破棄する。
  作成失敗では主窓の構築を失敗させ、既存起動失敗へ戻す。正常稼働中だけrootを無効にする退避経路は足さない。
- 名前入力・失敗箱・OS色選択・MessageBox退避は、このowner境界を通る。DialogBox/ChooseColor/MessageBoxの同期loopとEndDialogを維持する。
  完全非同期化、別の自作modal loop、DialogBox中のroot再enableは採らない。
- 最下層のmodalだけをproxyにownedさせ、入れ子は現在のnative面をownerとする。OSが直前の面をdisable/restoreする関係を維持する。
  proxyは実行中に表示/activateせず、taskbar用の窓や独自UIA providerを作らない。
- 製品rootが必要なgeometry/appearance/focusの参照は、所有chainから同じ正典の関数で解決する。実際のmodal ownerと製品rootの役割を混同しない。
  同じUI threadでも無関係な窓は対象にせず、現在owner chainに属するdialogを都度確認する。owned proxyにGetLastActivePopupを使わない。
- native中は本文/一覧/検索欄/背後のCommandLayerをdisableし、業務の共通入口・queued input・UIAにも同じ遮断を適用する。
  HWNDのdisableだけを再入防止の代わりにせず、shellのために業務guardを外さない。
  具体的な対象はfilter_input、note_pane、drawer、command_layerとその入力子である。begin前にfocusを捕捉し、end後に現在面に従って有効化してから復帰する。

### 3. 主窓終了と同期呼出しの寿命

- 通常終了要求をUI所有の閉じたphase（待機/要求済み/終了処理中）で管理し、OS終了通知のsession phaseとは分ける。
- child面中の主窓×は面を隠さず、テーマpreviewも復元せず、既存 `close_window` → `depart_document` → `finish_quit` へ合流する。
  成功なら未確定テーマを書かず終了し、取消/保存失敗なら開いていた面・候補・本文を維持する（ADR0061）。
- 終了処理以外のnative面中の主窓×は、最内側の面へ既存の取消を要求する。未確定の名前を承認しない。名前入力のIME guardを迂回せず、拒否なら終了要求を解消する。
  面から戻った業務の続行を取消扱いにし、名前/色選択の成功戻りや失敗箱を挟む後続操作が古い宛先へ適用されないようにする。既に成功した保存を巻き戻さない。
- nativeがすべて戻り、最外の同期業務dispatchも終わるまで主窓を破棄しない。安全な地点で要求を一度だけ通常終了へ渡す。
  PostMessageだけを安全性の根拠にしない。内側loopに配送されても深さ/phaseで実行を拒み、要求を失わない。
  WndProcに加え、UIA hostの実行・通知・snapshot復元までを`folio_window_begin_dispatch/end_dispatch`で同じ寿命へ含める。
  UIA実行中の再入は主窓CloseのInvokeだけを共通可否で許し、外側の実行guardを元値へ戻す。root/header containerとCloseの可否を矛盾させない。
- 終了処理が開いた無題の3択や保存失敗面では、主窓×の反復で終了を積まず現在面へfocusを戻す。
  3択の取消や保存失敗でphaseを待機へ戻す。面自身の取消の意味を変えない。
- 色選択は主窓の同期modal境界へ合流し、保留終了時には色を適用しない。drawerに別のmodal状態を持たせない。
- WM_QUERYENDSESSIONはnative中を明示的に拒否し、ownerが有効になったことを保存終了可能と読み替えない。ADR0053の同意/取消を維持する。
  WM_QUITはOSの既存伝播を使い、nativeの終了/取消/エラー結果を保存・破棄の成功へ変換しない。製品のPostQuitMessageは既存WM_DESTROY後の経路を維持する。
  生存rootへ外部からraw WM_QUITを注入した場合の既存main loop終端動作は今回変更せず、通常の主窓終了要求の受理条件に数えない。
  GetMessage失敗/取消を成功としない。

### 4. 位置、寸法、focus、復元

- 浮き面の初期配置と高さは主ヘッダー下の作業領域を基準にする。内側余白16を保ち、対応最小窓でheaderが面に隠れないようにする。
  設定/テーマ/Help/履歴は同じ制限を使い、検索/置換帯は既存本文の配置契約を維持する。
- テーマ候補1行の高さが不足するときは、入力後と脚前の間隔を通常8から下限4logicalまで、不足分だけ連続的に縮める。
  題・入力・固定理由高・案内・件数・戻る操作・面高を保つ。結果や検索語で寸法を変えず、同じ候補矩形を描画・hit・UIA・scrollが使う。
  この縮退は隣接領域の間隔だけに適用し、文字と自分の枠の内側余白や、主ヘッダーと面の間の余白を削らない。
  実OS120DPIの英語320×360logicalでは1行40pxに対して33pxしか残らず、7px不足したことが根拠。通常の高さでは元の間隔を保ち、高くした際に行数を逆に減らさない。
- nativeは作業領域中央を初期位置とし、同じ高さ予算とmonitor workareaへ制限する。幅480/一覧320から開く契約は維持する。
  内容が不足する場合は既存の2行一覧/理由領域scrollへ縮退し、理由全文・入力・取消へ到達できるようにする。
- OS色選択とMessageBox退避は、OSが所有する入力/理由/取消と本来の寸法を保つ。初期位置を同じ作業領域から導き、header下へ置く。
  monitorの下端で押し戻すとheaderを覆う場合は、左右へ置ける余地も使う。monitorにも収まらない面では理由と取消の到達を優先する。
  自前面だけが高さ縮小と再flowを担い、OS面へ独自のcontrol配置や入力処理を足さない。
- native自身の手動移動を保つ。主窓移動では実HWND矩形から相対位置を導き同量移動し、SWP_NOACTIVATEでfocusを奪わずworkareaへ寄せる。
  最小化中に(-32000等の)退避位置へ面を再配置しない。最大化/復元/resize/DPIでは現在の領域を正典から再導出し、再入を防いで再flowする。
- 窓操作後は現在のnative面の有効な入力focusへ戻す。面終了後は既存 `restore_valid_focus` を使う。破棄済み/非表示/無効なHWNDへfocusを戻さない。
- native中の主窓pointerは、可能なshell操作ではWM_MOUSEACTIVATEの非activate応答を使い、名前EDITから不必要にfocusを外さない。
  OS移動/resize/最大化がactivationを伴う場合でも、名前の業務的承認は起こさず、操作後は同じmodalへ戻す。
  IMEの物理的な変換確定がOSのfocus移動で起きないことを疑似メッセージだけで保証しない。
- 暗幕は深さと現在面から再導出する。取消・ネスト・作成失敗でも対に復元し、失敗時に入力不能な覆いを残さない。
  資源失敗時に元理由を捨てず、終了要求のための通知の再帰/無限反復を作らない。
  native中のresize/DPI配置失敗は背後の面を閉じたり新たな失敗箱を再入させず、安全な復帰後に一度だけ扱う。
  OS退避面の追従/reflowに失敗した場合は、理由表示と取消を優先する。hidden ownerへactivationが残らないことを実visible条件で確認する。

## 検証と限界

対象はchild面のheader hit/pointer/close/UIA、native name/failure/ChooseColor/MessageBox退避、最内取消と最外unwind、保存成功/拒否/無題3択/IME guard、
通常終了中の再close、session拒否、最小窓/resize/最大化/復元/移動、focus/capture、UIA可否/owner chain/余計な可視windowを含む。
製品の候補で確認し、独立feasibilityだけで受理しない。実際の画像で暗幕と有効な主窓×、文字到達を確認する。
物理IME・実reader・実displayのDPI跨ぎ等は実施した範囲だけを報告する。
schema/実行時依存/ゲート閾値/除外は不変。最終head必須CI成功後に統合。Waivers: none。

## 一次資料

- [DialogBoxIndirectParamW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dialogboxindirectparamw)
- [About Dialog Boxes](https://learn.microsoft.com/en-us/windows/win32/dlgbox/about-dialog-boxes)
- [Window Features](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
- [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor)
- [GetLastActivePopup](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlastactivepopup)
- [EnumThreadWindows](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumthreadwindows)
