# 操作パレットの絞込み・取消と編集保持 — #704

2026-11-06。hideの11月5日23:17頃の「止めるまで進めて」と11月6日の「頑張って！」を受け継続。
主担当単体で進め、追加委任なし。基点main `82204cb8d6c079e583ac6c8fc2b797f649976064`。
規則: ARC-001/004/005/011/012、C-008/016/017、QLT-008/010/012/013、CNF-006、GIT-001〜004、ADR0016/0043/0044。
追加waiver none、既存WVR-0001継続。

## 根拠・変更・確認する契約

[ADR0016決定4](../adr/0016-command-catalog-and-shared-execution.md)は一時入力面の開閉で保存・モード変更・本文再投入をせず、元のfocusへ戻す。
検索/置換/設定のnative回帰とは別に、操作パレットでEN_CHANGEによる候補再構築を経て取消する経路を固定する。
候補を読むだけで未保存本文やRedoを失う誤動作を検出する。新しい製品機能や入力面実装ではない。

既存tests/win32/image_status_tests.cのfixture・公開UI action・native本文snapshot・保存履歴snapshotを再利用する。
HELPの公開アクションからパレットを開き、別名writeで候補があり、無関係な語で候補が無いことを公開UIの選択結果で照合する。
入力欄のfocusと全文を確認してから、候補ありは閉じるボタン、候補なしは入力欄Escで閉じる。
変更状態とUndo後のRedo保有状態の両方で、本文全UTF-16単位・非空選択範囲・native dirty・Undo/Redo可否・元focusを前後比較する。
最後に実Redo/Undoを行い、各全文とapplicationのSAME/CHANGEDも確認する。
全境界で保存本文・5履歴slotの存在/全bytes・store_revision不変を照合し、画像要求0も確認する。

CMakeLists.txtは既存targetへpalette_native/--palette、同image_status_data lock/30秒を2行追加するだけ。
製品C/API/schema/依存/gate/閾値/抑制不変。既存test本文とselectorの意味は保持する。

## 実行結果と環境

正規eng/toolchain.ps1→CMake Debug→cmake --build <専用debug> --target image_status_tests --parallel 2が成功。
整形・警告error・静的解析を維持。対象image_status_tests --paletteは **141checks/0.727768秒**、終了0、stderr空。
CTest登録はctest --show-only=json-v1 -R '^palette_native$'で同exe/selector/lock/30秒を読取り照合する。
差分/相対参照/source/build graphのconformanceと最終hashの自己レビューを行い、最終head必須CIのeng/check.ps1を通して正典統合する。

環境はローカルWindows、専用desktop NeNeFolioImageStatus520。D:/NeNeFolio/outputs/704-palette-native/debug/verification-images/配下だけのportable dataは初回0file。
公開UI action、native入力欄へのSetWindowText、SendMessage、公開UI選択を使う。通常app/実ノート/Store/Ledgerは操作しない。
物理Ctrl+P/IME・全DPI・無題・パレットからの操作実行・性能は今回の主張に含めない。
既存の成功済み対象検証と通常exeを再実行/再buildせず、ローカルの無関係な全回帰も追加しない。[gate-proofs](gate-proofs.md)に環境を収載する。

全input/raw/artifact/終了値はD:/NeNeFolio/agents/704-palette-native/runs/、対象値はcontract-01.json。
最終source/CMake/docsのhash・既存test保持・全試行はself-review.json、最終CI/統合/通常不変/全保全整理はintegration-completion.json。
未完の原本を統合済みと扱わない。完了後に専用worktree/outputの全保全と取込/未保存/untracked/ignored/link/稼働参照を監査し、専用rootだけ整理する。
branch/commit、恒久証跡と公開配布1.3.1は保持する。

## 直前の完了と未確認事項

#702/PR703は最終CI42045853037 full-gate/check成功、main82204cbへ統合済み。127checks/0.691320秒の編集破棄契約を収載した。
11月6日00:16までに1396files/64864787bytesを全保全・照合し、取込/clean-untracked/ignored/link/稼働参照0確認後に702専用rootだけ整理した。
archive SHA256は74e81e0031b1ee72518ff93844fee7ec05af247cc37819a4204d6f06779746f3、branch/commit保持。通常exeのSHA不変。
原本はD:/NeNeFolio/agents/702-discard-native/integration-completion.json。[編集破棄の品質記録](2026-11-05-discard-native.md)。
Store1.3.1は11月5日23:17頃のhideによるPre-processing報告が最新。portal/提出hash/申請番号/認定/公開は独立未確認。
保留#466/#479/#634/#530と旧削除拒否領域を維持し、この継続指示で個別保留を解除しない。
