# 本文倍率と未保存編集の保持 — 2026-11-05 / #659

## 結論と正典

倍率175checks/0.864898秒、共有fixture/追記helperのノート切替162checks/0.899475秒。両stderr空、子/補助/記録0。
[ADR0067](../adr/0067-note-display-zoom.md)の倍率所有・本文保持を恒久CTest note_zoom_nativeへ追加した。
#383の[既存専用試験](2026-10-25-note-zoom.md)の広い受理を置き換えるものではなく、通常操作の一連をCIで守る。新規製品欠陥の認定ではない。
既存image_status_testsの専用desktop/data、実folio_window/note_pane、公開EDIT/VIEW、通常WM_MOUSEWHEELを使用。
main windowへ本文内のscreen座標とMK_CONTROL/deltaを送り、既存の座標振分け→note_pane→EM_SETZOOM→配置通知を通す。
観測はEM_GETZOOM。直接EM_SETZOOMや新しい製品API/targetを作らず、同exe/30秒/image_status_data lockへ--zoomを追加する。

## 一連の固定期待

短い正常な日本語/絵文字/Markdown本文「# Current 日本語 😀 **saved**」を使用する。
初期data0file、既存archive portで2世代を用意し、各run前のcopy/hashと終了時hashを原本へ保持する。

1. VIEWの100%をnativeから読み、選択2〜4単位・本文全units・dirty・Undo・Redo・focusをsnapshot。
   focusをmain windowへ置いて本文内のCtrl+wheel5刻みを配送し、150%とsnapshot全項目の不変を確認する。
2. 公開EDITで本文を再投入しても150%を保ち、raw Markdown全unitsとSAMEが正しい。
3. 末尾に「 追記 🙂」をnative編集一回で追加しCHANGED。選択10〜12単位と本文/dirty/Undo/Redo/focusをsnapshot。
4. delta60→60→-120で150→160→150%。3600→120で300%上限、-3600→-120で50%下限、1200で150%へ戻る。
   各回のnative倍率とsnapshot全項目の不変を比較する。固定期待配列であり、製品の計算結果から期待値を作らない。
5. Undo一回で元の本文/SAMEへ戻る。Redoが可能なまま倍率を160%へ変え、再びsnapshot全項目を比較する。
   実際のRedo一回で追記だけが戻りCHANGED、Undo一回で元の本文/SAMEとなる。
6. 公開VIEWへ戻っても160%を保持し、最初と同じ表示本文/readonly/modeを確認する。

VIEW変更後・EDIT一巡後・Undo/Redo後・VIEW復帰後にstore_revisionと現在file/全履歴1〜5のbytes/不存在を照合する。
保存内容は不変、画像要求0。#657と共有する履歴seed/追記helper/selectorの直接影響を--note-switchで一度確認した。
実app/実ノート/Storeを使わず、SwitchDesktop無し。親単体で実装・自己レビュー・対象検証、追加委任無し。

## 実行記録

| run | コマンド・結果 |
| --- | --- |
| configure-debug-01 | 固定toolchain、cmake -S <task> -B <debug> -G Ninja -DCMAKE_BUILD_TYPE=Debug、成功 |
| build-debug-01 | clang-format --dry-run --Werror、cmake --build <debug> --target image_status_tests --parallel 2、142actions成功、tidy/警告維持 |
| contract-01 | image_status_tests.exe --zoom、175checks/0.864898秒、stderr空 |
| switch-01 | 同exe --note-switch、162checks/0.899475秒、stderr空 |
| conformance-01 | 7docsの参照/diff/source/build graphと提出入力SHAを確認する |

tools/record-run.pyで全入力snapshot、raw stdout/stderr、終了値を保存。対象build/native失敗なし。
成功済みの#383全条件や無関係なnativeは工程だけで繰り返さない。最終head必須CI eng/check.ps1成功後にtools/merge-pr.ps1で統合する。

## 範囲・保全

短い通常本文、合成native wheel、通常倍率変更と公開mode切替が対象。
物理ホイール/キー/IME、異なる物理DPI、全言語、行番号pixel/baseline、故障注入、全OSの新保証ではない。
全I/Oを捕捉せず、store_revisionと保存内容を比較する。先行fixtureの履歴数でchecks数は変わる。
製品ロジック/API/schema/版/依存/ゲート不変。通常版再build無しで#651反映後exe/data不変を確認する。
規則ARC-001/004/008/012、C-008/012/014、QLT-008/010/012/013、GIT-001〜004。追加waiver none、既存WVR-0001継続、#634保留。

原本D:/NeNeFolio/agents/659-zoom-native/。最終CI/統合/通常exe-data不変/全保全と専用領域整理は同integration-completion.jsonへ。
完了後は当該D専用worktree/outputだけ整理し、branch/commitを保持。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。
