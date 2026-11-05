# 同一ノート再選択・別ノートへの保存境界 — 2026-11-05 / #657

## 結論

ノート切替152checks/1.707116秒、二本目を残した既存画像fixture75checks/1.267154秒。両stderr空、子/補助/記録0。
同じ名前付きノートのSELECT/INVOKEは未保存本文・選択・Undoを保ち、別ノートへの切替だけが保存と履歴回転を一度行う。
無変更のノートを離れても保存世代を増やさず、文書を跨いで前のUndoを持ち越さないことを恒久native回帰へ追加した。
[ADR0013](../adr/0013-index-focus-vim-keys-and-persistent-mode.md)、[ADR0039](../adr/0039-note-trash-and-category-operations.md)、
[ADR0050](../adr/0050-unsaved-note-departure.md)の既存契約を固定するもので、新規製品欠陥の認定ではない。
製品ロジック/API/schema/依存/版/品質ゲートは不変。

## 正典経路と固定期待

既存tests/win32/image_status_tests.cの実folio_window/persistence、専用desktop/dataを使用する。
--note-switch selectorとCTest note_switch_nativeを同exe、30秒上限、image_status_data lockへ追加し、新targetや製品入口は作らない。
このselectorだけschedule/twoを合成して、既存oneの名前・順序・他selectorを維持する。
oneは短い通常本文「# Current 日本語 😀 **saved**」、twoは「> Other 別ノート 🐾」。先頭2世代は既存archive portで用意する。

1. window作成後に公開EDITを実行し、末尾に「 追記 🙂」をnative編集一回で追加。本文とCHANGEDを固定期待へ比較する。
2. 選択を10〜12 UTF16単位へ置き、現在世代の同一行をSELECT/INVOKEする。本文全units・選択・Undo可否を保持する。
3. store_revision、oneの現在fileと履歴1〜5の全bytes/不存在、twoの全bytesと全履歴不存在を比較する。
4. native Undo一回で元の本文/SAMEへ戻り、oneの全保存内容が変わらないことを確認する。
5. 再追記してtwoを公開SELECTするとEDITのままtwoを表示。oneは追記済み本文が保存され、履歴1は元の本文、2〜5は旧1〜4へ移る。store_revisionは正確に+1。
6. twoのUndoは空で、前の文書の編集を戻せない。twoは編集せず、oneへ公開INVOKEすると追記済み本文/SAMEへ復帰し、本文へfocusが戻る。
7. 復帰後もUndoは空、store_revisionは+1のまま、oneの保存/履歴とtwoの全bytes/履歴無しを再比較する。

全比較は固定文字列と事前snapshotで行い、製品が返す期待結果を再利用しない。画像要求0。
追加のtwoが同じ専用dataに存在する状態で既存の画像経路（引数なし）も一度確認した。shared fixtureへの直接影響が理由で、広いローカル全件反復は行わない。
実app/実ノート/Storeは使わずSwitchDesktopも呼ばない。初期data0file、各実行前copy/hashと終了時hashを原本へ保存する。

## 実行記録

| run | コマンドと結果 |
| --- | --- |
| configure-debug-01 | 固定toolchain、cmake -S <task> -B <debug> -G Ninja -DCMAKE_BUILD_TYPE=Debug、成功 |
| build-debug-01 | testのclang-format --dry-run --Werrorと対象cmake build、142actions成功 |
| build-debug-02 | 自己レビューで切替時Undo初期化/focusの2確認を追加後、対象TU/linkの2actions成功。まだnative未実行の候補更新 |
| contract-01 | image_status_tests.exe --note-switch、152checks/1.707116秒、stderr空 |
| image-01 | 同exe引数なし、75checks/1.267154秒、stderr空 |
| conformance-01 | 7docsの参照/diff/source/build graphと提出入力SHAを確認する |

tools/record-run.pyで全入力snapshot、raw stdout/stderr、終了値を記録。対象build/native失敗なし。
単体で実装・自己レビュー・対象検証、追加委任なし。最終head必須CI eng/check.ps1成功後、tools/merge-pr.ps1で正典統合する。
補助読取ではADR0013の推測名が不存在で、rg --filesから正しい所在へ訂正した。過大出力も範囲指定へ訂正し、製品試験の失敗とは分離する。

## 範囲と保全

短い正常な名前付き2文書、公開UI action/native messages、通常の保存成功が対象。
物理keyboard/IME、全OS/DPI、無題ダイアログ、保存失敗、電源断、巨大本文の新保証ではない。
store_revisionと保存全bytesを照合し、OS全I/Oを捕捉した回数保証ではない。先行fixtureの履歴数によりchecks数は変わる。
製品不変で通常版は再buildせず、#651反映後exe/data不変を照合する。追加waiver none、既存WVR-0001継続。#634保留。
規則ARC-001/004/008/012、C-008/012/014、QLT-008/010/012/013、GIT-001〜004。

原本はD:/NeNeFolio/agents/657-note-switch/。最終CI/統合/通常exe-data不変/全保全・専用領域整理は同integration-completion.jsonへ。
完了後は当該D専用worktree/outputのみ整理、branch/commit保持。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

## 最終受理・保全整理（15:58）

PR658 head5243c54df5615123aa6a268c2c2ab758c250fc7b、必須CI41983221237 full-gate/check成功、main5d776cbへ統合済み。
Issue657 CLOSED、通常exe/data32file不変。1377file/64616995bytes全保全ZIP/hash照合と取込tree/clean/ignored/link/稼働参照監査後、当該worktree/outputを削除。
branch/commit保持、全run成功。原本D:/NeNeFolio/agents/657-note-switch/integration-completion.json。
