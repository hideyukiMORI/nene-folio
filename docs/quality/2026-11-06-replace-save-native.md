# 置換の2入力欄からの保存とnative編集保持 — 2026-11-06

Issue #719。FR-006、ARC-001/004/005/011/012、C-008/016/017、QLT-008/010/012/013、CNF-006、GIT-001〜004、ADR0016/0028。
追加waiver none、既存WVR-0001継続。主担当単体で設計・実装・自己レビュー・対象検証を行った。

## 固定した契約

ADR0016の補正2は入力面のCtrl+Sで本文だけ保存し、欄の語・選択・focusと面の種類を保つ。
既存image_status_testsへreplace_save_nativeを追加し、公開REPLACE操作と2つのnative EDITのWM_CHAR0x13から
既存store_from_surface/command_saveへ合流する。製品hook・別の保存経路・製品src変更はない。

- 変更済みEDITの本文は日本語・絵文字・Markdown原文を含む。置換パターン「日本語」、置換文字列「猫 🐾」を入力し、実ICUの下見1件を確認する。置換は実行しない。
- パターン欄へfocusを置きCtrl+Sを2回送る。両欄の全文・別々の非空選択・可視性と押した欄のfocusを保持する。
- 本文の全UTF-16・選択・native modified・Undo/Redo可用性・EDITモードを保持する。保存済み基準との比較はCHANGEDからSAMEへ変わる。
- md全bytes、全5履歴slot、store_revisionの増分1を各保存後に確認し、2回目の同内容保存が追加の世代を作らないことを固定する。
- CLOSE後に本文へfocusを戻し、実Undoで元本文/CHANGED・Redoありを確認する。置換文字列欄を押した欄として同じ保存確認を行う。
- 2回目の保存後も実Redoで追記本文/CHANGED、再Undoで元本文/SAMEになる。保存後の全履歴とrevisionは不変。

保存前の履歴は各保存helper内で所有し、成功後の履歴は呼出側で別途snapshotする。Undo/Redoが保存物を変更しない条件も確認する。
既存helperの意味は変更せず、selectorを追加して振り分け関数名をrun_extended_fixtureへ整理した。
既存の専用desktop/data、30秒timeout、image_status_data lockを維持し、新しいtargetや試験用APIは増やしていない。

## 全試行と結果

原本はD:/NeNeFolio/agents/719-replace-save-native、試行は同outputs、作業木は同worktrees。
tools/record-run.pyでinput snapshot/hash、raw出力、exe、child/wrapper終了値を保存した。

| 試行 | 結果と判断 |
| --- | --- |
| build-01 | 試験関数save_replace_inputが引数5個でC-012の上限4を超え、Debug/tidyが拒否。child1/wrapper125。artifact指定もverificationで誤っており、正典はverification-images。exe未生成・記録エラーを保持 |
| build-02 | 保存前historyをhelper内で取得する形へ整理し、引数4個へ。正しいartifact指定で正典Debug/tidy成功、child/wrapper0、入力不変 |
| test-01 | replace_save_nativeの221checks成功、native/CTest1.45秒、stderr空、child/wrapper0、入力不変・記録エラーなし |

ビルドは固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debugとcmake --build build --target image_status_tests。
対象はctest --test-dir build -R '^replace_save_native$' --output-on-failure --verbose。
ASan/UBSan/nullability/tidyと規約の閾値を維持した。成功した対象実行は関連input不変のままPR/統合へ再利用する。
format/conformance実build graph/diffの結果と最終head必須CI・正典統合・通常exe不変・全保全整理は確定原本integration-completion.jsonへ残す。

最初の自己静読で誤ったenum名accessible_item_kindを既存accessible_kindへ訂正してからbuildした。
補助readの誤ヘッダー名とstdout.txt/stderr.txtは、既存宣言とstdout.log/stderr.logへ訂正した。製品検証とは分離する。
build-01の途中にartifactだけの別収載helperを準備したが、build自体も失敗したため実行していない。
訂正後はbuild-02が正しいexeを直接保全しており、未実行helperを成功証跡に使わない。

## 限界

Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、ローカルNTFS・専用D領域で確認した。
通常app/実ノート/clipboard/前面focus/Storeを試験に使っていない。物理キー/IME、実置換の再検査、保存失敗、無題modal、
全ノート検索focus、自動保存、VIEWからの保存、全DPIは対象外。対象外の挙動を成功したとは主張しない。
製品src/API/保存schemaは不変。通常buildは反復せず、同一exeのhash/bytes/更新日時を照合する。

## 統合・保全・整理の確定

#719/PR720は最終head6ede846c16543608a049a238dcb500cc6e3bb255、必須CI42144117242 full-gate/check成功。
mainb85308e653eac90041c764accb882231350eb058へ正典統合し、通常exeのhash/bytes/更新日時不変を確認した。
3371filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0を確認後、専用worktree/outputを整理した。
branch/commit保持。確定原本D:/NeNeFolio/agents/719-replace-save-native/integration-completion.json。
