# 同名書き出し拒否からの復帰 — 2026-11-06（#771）

FR-041、C-008、QLT-008/010/012/013、GIT-001〜004、[ADR0064](../adr/0064-content-modal-and-window-shell.md)と[ADR0082](../adr/0082-markdown-transfer.md)。
追加waiver none、既存WVR-0001継続。

## 根拠と対象

既存名の拒否自体は[#494実native](2026-11-01-markdown-transfer.md)、実file_bytesの非置換/bytes/日時保持は
[#601](2026-11-04-atomic-publication.md)の成功結果を再利用する。今回は[export_native](2026-11-06-export-native.md)へ、
Redoを持つEDITで実同名拒否→実Redo/Undo→別名成功→実Redo/Undoの連続境界を加える。製品不具合の再現とは扱わない。

tests/win32/image_status_tests.cの既存picker入力/有限observer/noticeを共用する。製品C/API/schema/依存/版/CMake/ゲートは変更しない。
同名拒否では公開ACCESSIBLE_NOTICEの全文と可視性を照合し、成功時のmodal noticeは以前と同じ独立literalを維持する。新selector/runner/試験用製品窓口は無い。

## 有限fixtureと判定

1. 専用desktopで変更EDITを書き出し、実Undoして原文/非空selection/Redoを持つEDITへ戻す。
2. 同じ日本語保存先を実IFileSaveDialogで選び、既存の通知欄「その名前のファイルがあります。別の名前を指定してください。」を全文照合する。
3. 拒否直後の本文全文/selection/modified/UndoRedo/focus/mode/選択note、元md/全履歴/revision/両category全実file集合・台帳を保持する。
4. 実Redo→Undoで期待する変更/原文へ戻り、別の日本語pathへ成功する。その後の実Redo→Undoも確認する。
5. 保護対象の全bytesを独立literal、サイズ/作成更新日時を実OS値で拒否直後と別名復帰後に比較する。
6. 外部全file集合は拒否直後1件、別名成功後2件、VIEW出力後3件。予定外の一時file/directoryを拒否する。

初期状態から元categoryに追加2mdと非辞書順台帳、別categoryに追加1md、以前のexportdirectoryに保持物1fileを置いた。
初回実行の途中出力1fileも保全し、最終実行では既存6fileを上書きせず、新exportdirectoryを選ぶ。通常app/data/foreground/clipboard/Storeは使わない。
同thread/root所有と50ms/100callback上限、root有効/内容無効/readyfalseと終了後復帰は既存のまま検査する。

## 全試行と結果

| 試行 | コマンド/結果 |
| --- | --- |
| build-01 | 固定toolchain、Debug/tidyの`cmake --build build --target image_status_tests`成功。child/wrapper0、記録errorなし、入力不変 |
| test-01 | 3.19秒でpicker/notice終了条件に失敗。child/wrapper8、記録errorなし、入力不変。テストが同名拒否をmodalと誤認しており、製品不具合のredではない |
| build-02 | 既存inline_outcomes/report_outcomeの通知欄を確認し、public ACCESSIBLE_NOTICEの全文/可視性へ訂正。変更testのみ2actions、Debug/tidy成功、child/wrapper0、入力不変 |
| test-02 | `ctest --test-dir build -R '^export_native$' --output-on-failure --verbose`、438checks/3.52秒成功。child/wrapper0、stderr空、記録errorなし、入力不変 |
| audit-01 | 既存6fileの全bytes/hash/作成更新日時不変、新3出力の全UTF-8 bytesと全file集合が独立literalと一致。child/wrapper0、入力不変 |

初回前提の誤りと根拠は同agents/diagnosis.json。製品のinline方針・成功文・本文/metadata/全file集合判定は変更していない。
補助apply_patch初回は整形後の行に一致せず適用前に拒否、現行行を読んで適用した。ADR0060の推測filename不存在も実pathへ訂正した。

原本D:/NeNeFolio/agents/771-export-recovery、試行D:/NeNeFolio/outputs/771-export-recovery。record-runで入力/実argv/raw/終了値を収載する。
単体自己レビュー後に対象検証をまとめ、成功入力不変ならpush/レビュー/統合で再利用する。無関係なnative/ローカルfullgateを反復しない。

## 限界と統合

このホストとCIの有限native条件であり、物理IME、全DPI/全Windows版、第三者の同時書込、ディスク障害全般は保証しない。
作成更新日時はnative処理中に比較し、外部Pythonも既存fixture/途中出力6fileを比較する。新しい出力の途中metadataを外部Pythonが観測したという主張へ広げない。
最終headの必須CI/正典統合/通常exe不変/全保全/専用整理は、同agentsのintegration-completion.jsonへ確定する。

## 前作業#769の確定

#769 / PR770は最終head`f7a7ebb315c1a0afb14dcd710626b2269459dfd7`、必須CI42215039675のfull-gate/check成功。
main`fed522a684966b68d326a3506a3691f6df49d4b5`へ正典merge02で統合し、Issue769 CLOSED。通常exe hash/bytes/更新日時不変、製品src/eng/fonts/packaging/CMake不変。
3870file全hash保全、ZIP83769164bytes/SHA256 `7fcda0819b4e5f9664e14a637992df4db1119d9a4bf24134ea020fdf1888d4f2`。
絶対2roots/同squashtree/clean・ignored・untracked/link/稼働参照0を確認して専用worktree/output整理、branch/commit保持。
原本D:/NeNeFolio/agents/769-merge-ready-failure/integration-completion.json。初期CI成功後の時計境界反例と補助行slice失敗も分離収載した。
