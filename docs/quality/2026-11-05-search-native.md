# ノート内検索の選択・巡回・編集保持 — 2026-11-05 / #655

## 結論

公開検索入力から前後ボタン、表示件数、RichEdit選択へつなぐ恒久native回帰を追加した。
短い正常な日本語・絵文字・ASCII本文で、重なった3一致の前後巡回と、VIEW/未保存EDITの本文・Undo保持を確認する。
対象検索161checks/0.950290秒、共用したsurfaceボタンの置換153checks/1.060089秒。両方子/補助/記録0、stderr空。
[ADR0023](../adr/0023-in-note-search.md)と[ADR0052](../adr/0052-note-search-surface.md)の既存契約の恒久化で、新規製品欠陥の認定ではない。

## 正典経路

既存tests/win32/image_status_tests.cの専用desktop/data、実folio_window、公開FIND入力と世代付きsurfaceボタンを使用。
--search selectorとCTest search_nativeを同exe、30秒上限、image_status_data lockへ追加。
#653のreplace_buttonは処理を変えずsurface_buttonへ改名し、検索/置換で同じ公開INVOKEを共有する。
新target/製品入口/依存/ポート/API/schemaを作らない。

製品の変更は説明2か所だけ。folio_window.cのsearch_anchorが前方を「選択の終わり」と説明していた箇所を、
coreへ現選択を渡し、開始の次（空選択は同位置）から進む現契約へ揃える。
folio_state.hの方向の説明は、前後ボタンでは記憶方向が変わらない現契約へ訂正した。
check-comments.pyは指定した旧→新コメント置換だけでHEADと全文一致することを照合し、ロジック/宣言が不変であることを記録する。

## 一連の確認

専用ノートの本文は「猫 😀 aaaa 日本語」。期待値は固定のUTF-16位置と日本語の表示文字列であり、coreの結果から生成しない。

1. VIEWのwindowで本文先頭にcaretを置き、公開FINDを開く。native入力にAAを入れると、ASCII大小無視で5〜7単位を選択し「1 / 3 件」。
2. 公開NEXTで6〜8、7〜9、5〜7へ進み、件数は2/3、3/3、1/3。PREVIOUSで7〜9、6〜8、5〜7へ戻る。重なりを飛ばさない。
3. 絵文字😀を語にし、2〜4単位の1件を選択。NEXT/PREVIOUSのどちらでも同じ1件へ巡回し、サロゲート対を分割しない。
4. 正常な不存在語「不存在」は選択を保ち「見つかりません」、空語は選択を保ち空の結果となる。空語のNEXTも選択を変えない。
5. 公開closeで本文へfocusを戻し、本文のUndo可否と記憶方向を保持する。VIEW本文全unitsとreadonly/modeを確認。
6. 公開EDITへ切り替え、末尾に「 🐾」を1回のnative編集で未保存挿入する。同じ検索一連の後、本文とCHANGEDを保持。
7. native Undo一回で挿入前の保存済み本文へ戻り、SAMEとなる。検索が本文のUndo単位を追加していないことを確認する。

VIEW検索後、未保存EDIT検索後、Undo後でstore_revisionと現在ノート/全履歴1〜5のbytes・不存在を照合する。
保存は実行せず、画像要求は0。先頭2世代は既存archive portで用意し、先行nativeの履歴数に応じて実在する全世代を比較する。
通常app/実ノート/Storeを使わず、SwitchDesktopも呼ばない。最初の専用data0file、各実行前copy/hashと終了時hashを原本に保持。

## 実行記録

| run | コマンドと結果 |
| --- | --- |
| configure-debug-01 | 固定toolchain、cmake -S <task> -B <debug> -G Ninja -DCMAKE_BUILD_TYPE=Debug、成功 |
| build-debug-01 | 3fileのclang-format --dry-run --Werror、cmake --build <debug> --target image_status_tests --parallel 2。142actions成功、tidy/警告維持 |
| contract-01 | image_status_tests.exe --search、161checks/0.950290秒。子/補助/記録0、stderr空 |
| replace-01 | 同exe --replace、153checks/1.060089秒。共用ボタンの直接影響だけを確認、子/補助/記録0、stderr空 |
| check-comments.py | 製品2fileの差分が指定コメント置換だけであることを確認 |
| conformance-01 | 7docsの参照/diff/source/build graphと提出入力SHAを確認する |

tools/record-run.pyで入力snapshot、raw stdout/stderr、終了値を保存。対象ビルド/実行の失敗なし。
#306の過去の広範囲native確認やcore#448の4719query oracleを工程だけで繰り返さない。
最終head必須CIでeng/check.ps1を通し、tools/merge-pr.ps1で統合する。親が単体で実装/自己レビュー/検証、追加委任なし。

## 範囲と保全

正常な短い一行と、既定の前方記憶方向から公開前後を使う範囲。CR各型/大文書/全翻訳/物理IME/実打鍵/外部reader/全OS-DPIの新保証ではない。
全I/O捕捉ではないため同じbytesの書戻しが一度もない証明ではない。checks数は先行fixtureの実在履歴数で変わる。
製品コメント以外のロジック/API/schema/版/依存/ゲート不変。通常版は再buildせず#651反映後exe/dataの不変を照合する。
規則ARC-001/004/008/012、C-008/012/014/016、QLT-008/010/012/013、GIT-001〜004。追加waiver none、既存WVR-0001継続。#634保留。

原本はD:/NeNeFolio/agents/655-search-native/。最終CI/統合/通常exe-data不変/全保全・専用領域整理は同integration-completion.jsonへ。
完了後はD専用worktree/outputだけ整理、branch/commit保持。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。
