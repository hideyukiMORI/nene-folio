# #342 — 検索focus時の名前付き編集本文保存

2026-10-18、hideの実機フィードバックとUNTITLEDの扱いの明示合意に基づく。
Issue: [#342](https://github.com/hideyukiMORI/nene-folio/issues/342)。基点main a31e428（#340統合済み）。
規則: FR-015/032、ADR0006/0024、ARC-001/004/007、C-012/017、QLT-008/012/013、GIT-001〜004。
単体実行、schema・公開port・依存・ゲート変更なし、Waivers: none。
恒久証跡: D:/NeNeFolio/agents/filter-focus-save-20261018/。

## 正典経路と振る舞い

filter_input_procedureの既定WM_SETFOCUS処理後、NAMED/EDITだけstore_bodyへ渡す。
共通の履歴→md→検索cache同期を利用し、本文やdirtyの第2所有者を増やさない。
変更なしは既存no-op。UNTITLED/VIEW/NONEは保存しない。UNTITLEDは保存先を尋ねず検索へ進み、対象外を維持。
保存失敗/キャンセルは本文へfocusを戻す。失敗は既存failure_box、キャンセルは箱を出さない。
filter_savingはmodalのfocus再入だけを抑止し、保存中のEN_CHANGEも処理しない。完了後は解除する。
×を押す場合もfocus時の保存を先に行い、失敗時は空語へ変更しない。
IME未確定中は保存せず本文へ戻す。WM_KILLFOCUSで保存やmodal表示は行わない。

## 完成時の対象検証

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build --target nenefolio`: exit 0。ASan/UBSan/nullabilityとclang-tidyを維持。
- 後続のbuild入力確認は`ninja: no work to do.`。成功した機能試験は工程だけで繰り返さない。
- 外部build-probe.pyが実compile_commandsの全flagsと4製品lib/resourceを使用し、専用desktopの実Win32で確認。
- focus.log/focus-desktop.json: PID34664、exit 0、前景HWNDは前後16255834で不変。hideのPID48252やデータに触れていない。
- マウス、Ctrl+Shift+FのWM_COMMAND、検索外側余白、公開UIA actionの4入口で、各1回の履歴/本文writeと即時検索一致を確認。
- 各入口で同本文の再focusはwrite/archive追加0。保存前後の本文・選択・modified・Undo可能状態・EDIT modeが一致。
- 実persistence portへの転送を観測し、履歴失敗/本文write失敗を各1回だけ注入。失敗表示は各1個、保存前mdは不変、本文focus復帰、内容/選択/Undo保持、再試行成功。
- 失敗dialogの中からfocus保存関数を再入し、追加I/Oなし。×起点の保存失敗では元の検索語も保持。
- 合成WM_IME_START/ENDCOMPOSITIONで未確定時の保存拒否と終了後の保存を確認。物理IME候補操作の代替ではない。
- UNTITLEDはwrite/archive/dialog追加0、本文を保って検索欄へfocusし、専用語は0件。
- 最終カウンタはwrite試行8（失敗1含む）/archive試行9（失敗1含む）/dialog2。focus.bmpの完全な画面を実際に確認。

準備中はPython既定cp932の読込と、外部probeのappend_text名の衝突を修正した。実装投入スクリプトは前方宣言を数えたassertで停止し、対象を定義へ限定してから投入した。
いずれも成功とは扱わず、失敗ログを保存。ASanのinterception_win起動診断は残るが対象workerのASan違反はなくexit 0。
製品core/application/persistenceの実装は変更せず、既存の単位/OOM/性能試験は再実行していない。

## 統合と候補反映

最終head必須CIの成功を統合条件とする。成功時source-inputs.jsonとPR/mainのtree一致で再利用を確認する。
統合済みclean mainからeng/package-release.ps1で0.2.0更新候補を生成し、別の展開先で短い起動を確認する。
通常exeはtools/prepare-real-machine.ps1で反映する。hideが編集中の#338候補は終了・上書きしない。
新しい手動確認先はD:/NeNeFolio/check/2026-10-18/342-beta-refresh/。合成データだけを新しく作る。
最終head/CI/merge・候補hash・通常exe・起動・整理は同所completion.jsonに実結果を記録する。
公開tag/GitHub Releaseは作成しない。#340の表示対象結果を再利用し、DPI一式を再実行しない。
物理IME 1はhideの暫定良好所感、3は未確定中切替なし。人の読み上げ聴取は未受理。
