# #319 基本アクセシビリティの検証記録

2026-10-17。対象実装と検証を完了し、最終headの必須CIへ渡す記録。
Issueは[#319](https://github.com/hideyukiMORI/nene-folio/issues/319)、契約はADR0055 Aと[ADR0056](../adr/0056-ui-automation-provider-lifetime.md)。
最終PR/head/CI/main・通常exe反映・整理結果は関連PRとD:/NeNeFolio/agents/319-basic-accessibility/completion.jsonで確定する。

## 正典経路と変更

`accessibility_host.c`がUIスレッドのCOMと公開先の寿命を持ち、`accessibility_node.c`が自前要素のUIA ABIを担う。
`folio_window.c`/`drawer_window.c`から現在の状態・描画矩形・共通操作へ接続する。
applicationの既存cursorを文書選択から独立した単一の行参照とし、SetFocusでは保存/読込/本文投入を行わない。
Select/Invokeは既存の保存・無題確認へ合流し、成功時だけ選択とcursorを揃える。
標準EDIT/RichEditのText/Value/選択能力は維持し、名前だけを直接注釈する。

F6/Shift+F6は可視4区画、Tab/Shift+Tabは自前操作列を巡回する。既存の索引キーと本文Tab/IMEを保持する。
行識別とlayout改訂を分け、折畳で失効した子参照は再展開しても復活しない。
Invoke通知・選択・focus・展開・property/structure通知を共通状態から発行する。
操作メニューのExpandは非同期、Collapseはnative menu自身を閉じ、通知中の再入は拒否する。
閉じる操作は既存の保存/確認経路を通る。

保存schema・外部runtime・ゲートの判定基準変更なし。Windows同梱UIA/COM/直接注釈ライブラリをCMakeとeng/architecture.jsonへ宣言した。
ui_textに3言語の区画名/本文名/キー説明を追加し、FR-006/018、ARC-004、ADR0013/0015/0056を更新した。

## 実行した対象検証

環境: Windows 11 10.0.26300.0 / x64、固定clang-cl toolchain、Debug、既存ASan/UBSan/nullabilityとclang-tidy。
作業木D:/NeNeFolio/worktrees/319-basic-accessibility、証跡D:/NeNeFolio/agents/319-basic-accessibility/。
対象データはD側の隔離fixtureだけ。3カテゴリ（空を含む）、33ノート、複数言語名を用いた。

| コマンド / 証跡 | 結果 |
| --- | --- |
| `cmake --build D:/NeNeFolio/outputs/319-basic-accessibility/build --target nenefolio folio_tests` | build-06成功。文言fixture訂正後のbuild-07、対象試験も成功 |
| `D:/NeNeFolio/outputs/319-basic-accessibility/build/folio_tests.exe --accessibility` | `accessibility state and UI text tests passed`（state-tests-05.log）。関係実装不変のため再利用 |
| `cmake --build D:/NeNeFolio/outputs/319-basic-accessibility/build --target nenefolio` | 最終の寿命/通知/menu修正を含むbuild-13成功。clang-tidyも成功 |
| `pwsh -NoProfile -File D:/NeNeFolio/agents/319-basic-accessibility/compile-native.ps1` | native-build-09成功。製品sourceと正典buildのライブラリを使う隔離C fixture |
| `D:/NeNeFolio/outputs/319-basic-accessibility/native-runtime/native-contract.exe` | native-run-05: **106 checks / 0 failures**、provider reads 9319、wrong thread 0 |
| `native-contract.exe reader` + NVDA 2026.2 | reader-run-04成功。下記の実発話を取得 |
| `python eng/test-release.py --artifact D:/NeNeFolio/outputs/319-basic-accessibility/build/NeNeFolio.exe --metadata D:/NeNeFolio/outputs/319-basic-accessibility/build/product-version.json --workspace D:/NeNeFolio/outputs/319-basic-accessibility/release-proofs` | 配布検証器の異常系修正後、17項目成功（release-proofs-final.log） |
| `python eng/conformance.py` / `git diff --check` | 規約違反0件・空白エラーなし |

C fixtureは外部試験の中だけで保存失敗・名前確認取消・イベント観測を注入する。製品へ試験hookは追加しない。
別MTA threadの実UIA clientで58要素、9 buttons、36 tree items、5 radios、標準Value/Textと45 patternsを取得した。
Name/ControlType/IsEnabled/IsOffscreen/Boundsとpatternを照合し、UseComThreadingによるUI thread実行を確認した。

- 未保存AでSetFocus(B): cursor=B、選択=A、本文/Undo/変更印不変、本文・履歴write 0。
- Select保存失敗/無題取消: 文書と独立cursorを保持。成功時だけ選択とcursorを揃える。
- カテゴリSetFocus、Expand/Collapse、画面外ScrollIntoView: 別ノートを選ばない。
- 折畳→再展開、並べ替え、改名、削除、絞込0件→解除: 古い参照が別行へ化けず、失効参照は復活しない。
- 窓/状態破棄後の保持参照: 読取とInvokeがUIA_E_ELEMENTNOTAVAILABLE。イベント中の変更再入を56回拒否。
- menu開閉/通知中のCollapse拒否、Invokeの1回通知、Closeの無題取消/保存済み終了を確認。
- 3言語Name、3表示の不可視区画除外、幅620/960のBounds、空カテゴリ、固定帯hit test、F6/Tabの到達を確認。
- modal所有窓無効、IME変換flag、layout更新、終了照会中は背景操作を拒否。mode Select保存失敗でEDITを保持。

## 実readerの発話

NVDA **2026.2 AMD64**（NV Access署名検証成功）、Windows OneCore synth、reader/UI言語en。
カテゴリ名は日本語、ノート名はASCII。2026-10-17 **20:56:40〜20:57:17 JST**。
インストールせずD側portable/configを使用し、実行後に終了した。通常データ/自動起動設定は変更していない。

試験窓の前面化だけComputer Useで行い、その後はC fixtureが製品のkeyboard入口とUIA操作を呼んだ。
各段階で前面状態を確認した。UIAの取得値ではなく、実NVDAのInput/output logから次の発話を得た。

| 入力 | NVDA発話抜粋 |
| --- | --- |
| 両方表示へfocus | `Both, radio button, checked` |
| F6 | `New note, button` |
| Tab | `Search all notes, edit, blank` |
| 展開中カテゴリへSetFocus | `Categories, tree view` / `level 1, 仕事, expanded` |
| Collapse / Expand | `collapsed` / `expanded` |
| AlphaからBetaへSetFocus | `Alpha` / `Beta, not selected, level 2` |
| BetaをSelect | `selected` |
| native本文へfocus | `Beta — Note body, edit, read only, multi line` / 本文 `Beta` |
| 保存へfocus | `Note toolbar, tool bar` / `Save, button` |

証跡はreader-speech-04.txt、reader-result.json、reader-run-04.log。
入力fixtureのTab説明ラベルはnew categoryと誤記していたが、実際の次項目は仕様どおり検索欄であり、上表は実発話に基づく。
音声波形と人による聴取は採録していない。NVDAの実発話文字列を受理証跡とし、他reader/全UI対応は主張しない。
採録方法は[NVDAのlogging説明](https://download.nvaccess.org/documentation/en/userGuide.html#GeneralSettings)のInput/outputに従う。

## 初回失敗と独立レビュー

初期のSDK C定数/GUID/型/初期化/複雑度、対象試験のCRLF/非表示行契約/文言終端の不足を修正した。
最初のGUI fixtureはJSONキー順がparser契約と不一致で、versionを先頭へ訂正した。
C試験のmenu初回失敗はTPM_NONOTIFYによる通知抑止で、通知を受ける正典menu経路へ修正した。
native-run-03の空カテゴリ失敗はfixtureの添字1を実データの2へ訂正し、run-04/05で成功した。
ASan起動時に環境由来の`interception_win: unhandled instruction`診断は出たが、試験は終了し、sanitizerのERRORはなかった。

最初のreader試験は背景窓だったため受理に使わず、前面条件を追加した。
hideのEscによる停止後は試験とreaderを終了し、再開指示後のreader-run-04で取り直した。
寿命/所有境界の独立レビューは読み取り専用。Invoke通知不足と固定帯hit testを修正し、
20:44の差分再レビューでは追加blocker/highなし。lifetime-review-final.mdに結果を残した。

最初のCI（39030934078）はbuild/unit/coverage/ゲート負例証明を通過後、wrong-machine配布試験で失敗した。
UIA追加前のexeと追加後のexeのMachineだけをI386へ書き換えて比較し、前者はllvm-readobjが成功終了、
後者はMachineを報告した後にimport名の解析で終了1になることを再現した（pe-machine-probe.log）。
`eng/release.py`を、CPU種別の拒否を先に報告し、その後に外部解析器の成功終了を必須とする順へ修正した。
AMD64を報告していても解析に失敗した入力を拒否する回帰試験を追加し、従来のwrong-machine試験も維持した。
不正成果物の受理、除外、閾値変更はない。直接影響した配布検証器17項目だけをローカルで実行し、UI試験結果を再利用した。

## 限界と統合

IME/終了の拒否は実Win32窓と状態flagによる境界試験であり、OS終了や全IME製品の再測定ではない。
補助画面の全面公開は#321、OS高コントラストは#320。一般的なWCAG適合や全reader対応を宣言しない。
成功済み対象試験は入力不変なら再利用する（QLT-012）。最終headの`pwsh -NoProfile -File ./eng/check.ps1`成功をCIで必須とする。

規則: FR-012/013/014/018/020/022/024/032、ARC-001/002/003/004/008/010/011、C-002/004/007/012/018、QLT-007/008/010/012/013、GIT-001〜004。Waivers: none。
