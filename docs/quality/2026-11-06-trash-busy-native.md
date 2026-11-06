# 使用中ノートのごみ箱送り拒否と編集履歴保持 — 2026-11-06（#745）

## 契約と正典経路

FR-035/ADR0040の正典はmdを移動できるか確認してから履歴に触れ、mdの共有違反なら何も移さず拒否する。
実窓の既存locked_save_nativeへ、VIEWとSAME本文・Redo保有EDITからの実Ex :trashnoteを追加する。
製品不具合は未確認。state_testsの状態遷移、B1の専用RecycleItem部品診断と区別する。
src/API/schema/CMake/依存/ゲートは不変。

- 既存専用one.mdの共有なしhandleを再利用する。操作前に正典recycle_prepareと同じDELETE|FILE_READ_ATTRIBUTES/共有0/OPEN_REPARSE_POINT|BACKUP_SEMANTICSのopenがERROR_SHARING_VIOLATIONになることを確認する。
- 公開tree focus→実Ex入力→:trashnote→Enterでexecute_trash_note_command/trash_note_at/run_trash_note/folio_state_trash_noteへ進む。SAME編集の先行saveは既存no-op。
- handleは拒否から戻った後に閉じ、元mdと全履歴の全bytes/有無・store_revisionを照合する。成功trashは再送しない。
- Exは入力語とfocusを保持し、公開surface statusが独立literal「ほかのプログラムが使用中のため、ノートを移していません。」を含むことを確認する。
- 拒否中とEscape後のnative全本文/非空選択/modified/UndoRedo/readonly・root/body/tree有効回復・VIEW/EDIT、NAMED/元名/元選択/SAME/public count、source全md集合、台帳の存在・全row order/public orderを確認する。台帳なしでもpublic orderを別snapshotで照合する。
- VIEWは描画済み本文をsnapshotと比較し、EDITは原文の全UTF-16とも照合する。Ex内focusと本文値の保持を区別する。
- EDITは実追記→UndoでSAMEかつRedo保有を作る。拒否後の実Redoで追記へ戻り、既存のCtrlS共有違反拒否・handle解放後の保存/UndoRedo/SAME保存を通す。
- source集合の件数と全体countを別に保持し、他カテゴリの既存ノートが無いことを仮定しない。追加snapshotのledger/name listsは同期試験終了時に破棄する。

主担当単体で自己レビュー。既存の排他handle・同期Ex・snapshotの範囲に閉じた試験追加のため独立レビューは追加しない。
FR-006/035、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0039/0040/0064。
追加waiver none、既存WVR-0001継続。

## 試行と結果

tools/record-run.pyで入力snapshot、raw stdout/stderr、child/wrapper終了値を保持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典toolchainのDebug configureとimage_status_tests build成功。事前の整形器path誤りは未実行として記録 |
| format-preparation | 正典clang-format --dry-runで6か所の整形差を検出、exit1。format-preparation.jsonとbuild-01入力snapshotへ記録。clang-format -iで訂正 |
| build-02 | 整形後の対象増分build成功。runtime前の自己レビューでroot/body/treeのenabled回復検査も追加 |
| build-03 | enabled回復検査を含む最終sourceで対象増分build成功 |
| test-01 / test-repeat-01 | locked_save_native 291checks/0.94秒、CTest0.95秒成功。台帳あり・source追加2file/target追加1fileのrepeatは350checks/0.73秒、CTest0.74秒成功。両runtime stderr空・child/wrapper0・入力不変。3既存mdと台帳1fileの全bytes/hash/作成更新日時不変。 |

repeatはnote_ledgerの既存version1/notes形式でone→zztrash-second→zztrash-firstの非辞書順台帳を作り、source2file/target1fileを追加した独立条件である。
追加物のみ外側の全bytes/日時比較へ含め、既存oneはfixtureのseed書込み対象として区別する。


静的検証はclang-format --dry-run --Werror、conformance実build graph、git diff --check。
変更はlocked_save_native専用helper/呼出しだけで、既存lock・他case・製品srcは不変。対象はimage_status_tests buildとCTest ^locked_save_native$。
成功した最終入力を照合して再利用し、統合時は最終headの必須CIでeng/check.ps1を実行する。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、専用desktop/verification-images/data、30秒timeout。
全原本D:/NeNeFolio/agents/745-trash-busy-native、試行同outputs、作業木同worktrees。
最終CI→正典統合→通常exe不変→全保全・安全整理の確定原本は同agents/integration-completion.jsonへ収載する。

準備の補助readで推測したADR名/docsの仕様path/coreのpane_mode等が不存在だったためrgで実pathを確認した。
初回整形器の推測path C:/Program Files/LLVMはCommandNotFoundで未実行。正典toolchainのVS LLVMを使った実行と区別して記録する。

## 限界

OSごみ箱移動成功/復元/設定変更、BUSY以外のOS移動失敗/台帳部分失敗/資源失敗、別ノート/カテゴリの削除、物理keyboard/全OS/DPI/外部UIAは対象外。
通常app/実ノート/clipboard/前面focus/Storeは試験に使わない。個別保留466/634/530と旧削除拒否領域を維持する。

## 統合・保全整理の確定

#745/PR746は最終head41e310ab6aa7830282d01f15051458a1969643ff、必須CI42176053296 full-gate/check成功。
main2dda66671c2af2017c7b05e5a1a39cf2f65477b8へ正典統合し、通常exeのhash/bytes/更新日時不変を確認した。src/eng/fonts/packaging/CMake不変で通常build/app起動なし。
4158filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0確認後、専用worktree/outputを整理。branch/commit保持。
Ready前merge01のPR本文field行不足拒否child/wrapper1も保持し、本文の形式だけを訂正したmerge02で成功した。
確定原本D:/NeNeFolio/agents/745-trash-busy-native/integration-completion.json。
