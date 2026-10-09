# 保存サイズ上限の確認記録（#227）

Issue: [#227](https://github.com/hideyukiMORI/nene-folio/issues/227)。FR-005/006、ADR0006/0020/0021/0022/0041の2026-10-09補正。
規則: ARC-001/004/008/009/010/011、C-002/003/005/008/011/012/014/017、QLT-009/013。Waivers: none。
source checkpoint `488f0f5bfbe87f4547dd5c853c6af07efa40d201`、最新main `c68269c57e10fad589a37ea1d97becf95c414dcf` へ未公開commitだけを置き直した最終source `f84444066e88101553f22fbd56d7f71157471cf2`。
専用branch `fix/227-note-size-limit`、全文ログ/試験source/独立buildは `D:/NeNeFolio/agents/227-note-size-limit/`。

## 問題と修正

旧実装は16 MiBを超える正しい本文を保存できるが、同じファイルを読込上限で拒否した。
親の隔離実adapter測定 `D:/NeNeFolio/agents/2026-10-09-autonomous/large-note-repro.log` は16777217 bytesのeditor入力をACCEPTED→create_note STORED→read_note UNREADABLEと記録している。この成功済み修正前再現は反復しない。

coreの単一目的 `persisted_size_limit.h` に16*1024*1024 bytesのconstexprを一つだけ置いた。
note_textはUTF-8検証、BOM除去、改行正規化後の長さ確認を行い、正しいが超過した本文を専用NOTE_TEXT_TOO_LARGEで拒否する。
正規化前のraw長だけでは拒否しない。事前計数は既存foldと同じ改行幅判定を使い、減算でoverflowを避け、超過なら新規確保へ進まない。失敗時outは不変。
applicationは専用FOLIO_STATE_NOTE_TOO_LARGEを既存の3言語失敗表示へ写す。通常/初回/EDIT別名保存と名前付きEDITの変更問い合わせは、当該本文のarchive/render/create/write前に拒否し、本文/対象/modeを維持する。
既存の先行synchronizeは順序を変えず、過去のpending修復が起き得る。無題note_changedはCHANGED、VIEWはSAMEの従来早期returnを維持する。追加確認箱は作らない。

file_bytesの共通store入口はtemporary file作成前にraw serialized length超過をUNWRITABLEで拒否する。readのraw上限は従来どおりで、BOMも長さに含める。
JSON/台帳/journalもwriterの実出力長が同じ上限を通る。保存schema、generic persistence enum、依存方向、gateは不変。
FR-006と関連ADRにcanonical本文と外部BOM付きrawの違い、先行同期、typed failure、公開前のjournal拒否を記録した。

## 修正後の対象検証

| 実行 | 結果 | 全文ログ（同agents領域） |
| --- | --- | --- |
| 対象incremental build（コマンド全文は未保存） | clang-cl/clang-tidy成功。folio_tests.exeとNeNeFolio.exeを再linkした5工程をログで確認 | build-target-2.log |
| `D:/NeNeFolio/worktrees/227-note-size-limit/build/folio_tests.exe --note-size-limit` | exit0 | unit-target.log |
| `python D:/NeNeFolio/agents/227-note-size-limit/allocation-target.py` | core/applicationへの確保失敗注入、専用selectorでexit0 | allocation-target.log |
| 最新main後のOS測定用build | 223書込lock guard/226 IME guardを含め成功 | build-os-base.log |

対象unitはASCII limit−1/limit/limit+1、多byte UTF-8のbyte境界、BOM除去、CRLF→LF収縮、CR→CRLF膨張、末尾改行、不正UTF-8優先、失敗out保持を確認した。
確保失敗入口は超過本文が確保枠を消費せず、次の小さい本文で注入失敗が残っていることを確認した。
applicationは通常/初回/EDIT別名/名前付きEDIT問い合わせのtyped failureと当該本文のarchive/write/create0、正常same-save、VIEW原文保存、無題/VIEW問い合わせ、既存index先行修復、3言語専用理由を確認した。

最初の対象buildはfixture malloc後のnullability推論をclang-tidyが拒否した。明示null分岐→require(false)+return/exitを置いて解消した（build-target-1.logを保持）。抑制/閾値変更はない。
allocation測定は既存の非致命ASan `interception_win: unhandled instruction ...` を出した。ログに保持し、試験PASS/exit0とは別の観測として扱う。
親によるsourceと全対象testsの静読で受理された。488f0f5→f844440のsrc/tests差分は先行223のadapter 31行と226のUI 4行のみで、227自身のcore/application/file_bytes/新結果/testsは不変。対象unit/OOM成功を再利用した。

## 実OS境界

独立 `run-size-os-probe.ps1` / `size-os-probe.c` はD専用exe直下のdataだけを使う。
2026-10-09 22:44:32 JSTまでに初回実行がexit0で成功した（os-build.log / os-probe.log / os-probe-exit.txt）。raw store/create limit+1はUNWRITABLE、既存4 bytes seedは不変、新規dest/tmp不在。canonical 16777216 bytesはcreate/read/write成功、外部BOM raw 16777219 bytesはfile_bytesとadapterの両方でUNREADABLE。旧台帳とprepared台帳の実writer長は16639786 bytesで上限16777216以内、journalの実writer長は16896060 bytesで超過。STARTはJOURNAL_FAILED、old.mdは1 byte M、履歴1.mdは1 byte H、旧indexは長さ/全bytes一致、新名md/history・journal・tmpは不在。OS測定にも既存非致命ASan interception warningを観測し保持した。巨大64000名fixtureは既存O(n²)重複検査を通るため、専用probeのみで測り通常unitへ載せていない。

## 最終単一ゲート

共有note_text生成契約とclosed結果enumが変わり、保存だけでなく既存読込/検索写し/Markdown表示/履歴復元の全consumerの型写像と正常入力境界へ影響するため、対象サイズtestだけでは検証が閉じない。この具体的な共通基盤の影響を理由に、対象unit/OOM/OSと親による静読を経たsource f844440で正典 `pwsh -NoProfile -File ./eng/check.ps1` を1回実行した（run-final-gate.ps1 / gate.log / gate-exit.txt）。2026-10-09 22:48:25 JSTまでにexit0で成功。conformanceとself-test、全79工程のclang-cl/clang-tidy、symbols core/application 2 libraries・違反0、CTest 2/2、branch 3238/3454 = 93.7463810075275%（93.75%）、negative 7.61%の90%未満拒否、実ツールproof 19件を確認した。成功後のsrc/tests/CMake差分0を確認し、工程だけのpush/review/mergeではこの結果を再利用する。

## 残る範囲

母語話者による文言評価、物理入力、巨大文書性能、外部編集検知、電源断/OSクラッシュ耐久性を保証しない。
利用者app/data、旧C作業木、過去の削除拒否対象は未操作。Waivers: none。

最終証跡4本（coverage complete/negative/resultsとproof results）をagents専用領域へSHA256一致で保存した。archives.csvに絶対source/destination/bytes/SHAを記録。品質記録の追記と親所有4状態文書の最終収載は文書のみで、成功sourceの実行部を変えない。
日時: 2026-10-09 22:48:25 JST
