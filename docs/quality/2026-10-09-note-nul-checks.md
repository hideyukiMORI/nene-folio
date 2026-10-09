# NULによる本文欠落の拒否（#232）

Issue: [#232](https://github.com/hideyukiMORI/nene-folio/issues/232)。FR-005/006/033、ADR0006/0038の2026-10-09補正。
規則: ARC-001/003/004/009/010/011、C-002/005/007/014/018、CNF-009/010/011、QLT-009/013。Waivers: none。
基点はmain `41f9b4e`、作業枝は `fix/232-note-nul`。ログと実OS probeは `D:/NeNeFolio/agents/232-note-nul/`。

## 再現と変更

基点の正典 `note_pane` を使う独立Win32 probe（`D:/NeNeFolio/outputs/nul-roundtrip-repro-1009/`）で、
5 bytes `{'A', 0, 'B', '\n', 'C'}` が `note_text_create` に受理される一方、履歴の全文置換では
1文字のAに切れることを実測した。通常編集のstream往復でも入力と一致しない。利用者のdataは使っていない。

本文の共通生成時検証が、UTF-8を最後まで検証しながらNULの有無を記録する。
不正UTF-8を先に返し、妥当なUTF-8でもNULを含めば `NOTE_TEXT_EMBEDDED_NUL` とする。
adapterはノート/履歴の読取をMALFORMEDへ、applicationは保存入力をNOTE_MALFORMEDへ写す。
UIの本文差替経路は増やさず、3言語の既存理由を「壊れている、または扱えない文字」へ揃えた。

## 対象検証

| コマンド | 結果 | 証跡 |
| --- | --- | --- |
| `python eng/conformance.py` | 違反0 | 初回対象buildの標準出力 |
| `cmake --build build --target folio_tests` | clang-cl/clang-tidy成功 | target-build.log / target-build-2.log |
| `build/folio_tests.exe --note-nul` | 成功 | target-tests.log / target-tests-2.log |
| `build/folio_tests.exe --note-size-limit` | 共通本文検証が影響する既存のサイズ境界成功 | size-boundary-tests.log |
| `python eng/symbols.py --build-dir build --require core application` | 2 libraries、違反0 | symbols-2.log |
| `pwsh -NoProfile -File D:/NeNeFolio/agents/232-note-nul/run-os-probe.ps1` | 実OS/application 27 assertion、元データ9ファイルのSHA不変 | os-probe.log / os-results.json |

coreは先頭/途中/末尾/単独/BOM後のNUL、out不変、不正UTF-8優先、長さの外の終端NUL、
空本文・日本語・改行を確認した。汎用UTF-8/UTF-16 codecはNULを保って往復する。
applicationは保存/編集終了/EDIT別名保存/名前付きEDITの変更問い合わせ/初回保存を拒否し、
保存済みbody・mode・宛先・履歴を保持し、今回のarchive/create/writeを呼ばない。正常なsame-body保存は引き続き無操作。

実OSは4種類のNULファイルとNUL履歴をMALFORMEDで拒否し、出力ポインタを変更しない。
履歴は読めない行として残り、採用とNULノートへの選択で現在のbody/modeを変更しない。
正しい履歴の読取は成功。通常/別名/初回保存の拒否後も元の9ファイルは不変で、copyや一時ファイルは作られなかった。
OS probeの既存ASan interception warningはログに保持し、assertion成功/exit0と分けて記録した。

途中の依存検査はCMake File APIの照会未作成で1回失敗し、照会を作って再configureした。
続く検査で `memchr` が中核の許可リスト外と判明したため、許可リストを広げずUTF-8の既存走査内へNUL検出を収めた。
修正後の対象unitと依存検査は成功。抑制・除外・閾値変更は無い。

## 統合前の検証と残る範囲

本文型の共通生成契約が変わり、ノート・履歴・保存・検索写し・Markdown表示のconsumerへ影響する。
この具体的な共通基盤の影響に対して、統合前に正典 `pwsh -NoProfile -File ./eng/check.ps1` を実行し、
結果をこの記録とPRへ追記する。成功後に実行部が変わらなければ、その結果をpush/mergeで再利用する。

NULを含む既存ファイルは読めない理由を示す。内容を自動変換しないため、修正には外部エディタが必要。
物理IME・別DPI・長時間性能・外部編集監視は今回の対象外。新しいOS API・依存・保存schemaは無い。
