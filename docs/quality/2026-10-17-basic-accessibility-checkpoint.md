# #319 基本アクセシビリティの途中記録

2026-10-17。状態は実装中。hideの指示でcommit/pushする時点の記録であり、製品の受理記録ではない。
Issueは[#319](https://github.com/hideyukiMORI/nene-folio/issues/319)、契約はADR0055 Aと[ADR0056](../adr/0056-ui-automation-provider-lifetime.md)。

## 実装と検証範囲

`accessibility_host.c`がUIスレッドのCOMと公開先の寿命を持ち、`accessibility_node.c`が自前要素のUIA ABIを担う。
`folio_window.c`/`drawer_window.c`から既存の状態・描画矩形・共通操作へ接続する。
applicationのcursorは文書選択と独立した単一の行参照とし、focusだけでは保存/読込を起こさない。
ui_textへ3言語の操作区画名/本文名/キー説明を追加した。保存schema・外部runtime・ゲートの変更はない。
Windows同梱のUIA/COM/直接注釈ライブラリをCMakeとeng/architecture.jsonの同じ許可経路へ追加した。

単位試験はfocusと選択の分離、保存/履歴writeを伴わないfocus、失敗/非表示行の拒否、展開の冪等性、
選択を伴わないreveal、移動/削除/絞込時のcursor整合を扱う。既存の直接影響する状態試験と3言語文言検査も実行した。

## 実行済み

作業木はD:/NeNeFolio/worktrees/319-basic-accessibility。証跡はD:/NeNeFolio/agents/319-basic-accessibility/。
固定toolchainは`eng/toolchain.ps1`で読み込んだ。最終の製品buildはbuild-08.log、対象試験はstate-tests-05.log。

| コマンド | 結果 |
| --- | --- |
| `cmake --build D:/NeNeFolio/outputs/319-basic-accessibility/build --target nenefolio folio_tests` | build-06で成功。続く対象試験は文言テストの列挙終端の更新不足で失敗 |
| `cmake --build D:/NeNeFolio/outputs/319-basic-accessibility/build --target folio_tests` | 文言テスト訂正後、build-07で成功 |
| `D:/NeNeFolio/outputs/319-basic-accessibility/build/folio_tests.exe --accessibility` | `accessibility state and UI text tests passed` |
| `cmake --build D:/NeNeFolio/outputs/319-basic-accessibility/build --target nenefolio` | focus失敗時の状態保持、既選択項目のSelect、未列挙要素の選択通知を整えた後、build-08で成功 |
| `python eng/conformance.py` / `git diff --check` | commit準備時に実行し、規約違反0件・空白エラーなし |

ビルドには既存のclang-tidyを含む。compile/linkの初回失敗はSDKのC定数定義、GUID、型、初期化、関数複雑度を修正して解消した。
単位試験の初回失敗はfixtureのCRLF、非表示カテゴリの結果契約、文言列挙終端を照合して修正した。
抑制・除外・閾値緩和は追加していない。pushを理由に成功済み対象試験を繰り返さない（QLT-012）。

接続前の独立C probeではSTA providerを別プロセスMTA clientから104回呼び、別スレッド実行0、
native EDITのValue/RichEditのText保持、切断後のUIA_E_ELEMENTNOTAVAILABLEを確認した。
このprobeは製品providerの寿命・操作試験を代替しない。

## 未完了と次の確認

- 製品の実画面確認は、隔離fixtureのJSONキー順が既存parser契約と合わず起動エラーになった。
  fixtureを訂正した後の再起動操作はhideのEscで停止。通常データは使用していない。
- Narratorなど実readerの起動/発話確認は未実施。UIA treeの取得で代替しない。
- 読み取り専用の独立レビューは途中。Invokeを提供する操作の`UIA_Invoke_InvokedEventId`通知不足が未修正。
  寿命/所有境界の最終レビュー結果はまだない。
- 製品providerの参照保持/破棄/再入、property/pattern/event、失敗/取消、3言語/3表示/画面外/0件、
  native Text/Value/Undo、IME/modal/終了境界の試験は未完了。
- ADR0015/関連するADR0013、FR-018、ARC-004の契約更新、最終head必須CI、main/通常exe反映、専用作業物整理が残る。

規則: FR-012/013/018/020/022/024/032、ARC-001/002/003/004/008/010/011、C-002/004/007/012/018、QLT-008/010/012/013、GIT-001〜004。
Waivers: none。
