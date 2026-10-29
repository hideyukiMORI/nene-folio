# #422 閉じた未対応画像の文字保持

記録日: 2026-10-29。変更範囲は既存inline token owner内の閉じた画像範囲、共用Markdown link unit、利用者向けMarkdown範囲の説明。画像表示・取得・URL起動は追加しない。Waivers: none。

## 変更

- `MARKDOWN_INLINE_IMAGE_LITERAL` を内部専用kindとして追加し、既存mate情報から閉じた画像候補を判断する。
- 画像印のescape条件と画像/通常link共通の閉じ終端queryを一か所へ集約した。
- top-level画像は画像全体を消費する。通常link label内ではlabelに完全に収まる画像だけを保護する。
- delimiter pairing/coherenceは画像範囲をskipし、emitは既存`append_range`でliteral出力する。外側の強調/link装飾と既存RTF変換を使う。
- unitに旧不具合のred、画像内の強調/strike/escape/code/nested link、外側delimiter、label内画像、親label跨ぎ、destination内画像印、閉じない画像、Unicode/RTF特殊文字、softline、表cell pipeのfixtureを追加した。

## 検証記録

限定redは既存期待との差を含む実RTFで記録した。11:26:56 JSTの一度だけ実行し、旧sourceでalt/destination内の強調が装飾として扱われる失敗を確認した。再実行していない。原本、実行ファイル、hash、selector logは`D:/NeNeFolio/agents/422-image-literal/phase-red-result.json`、`selector-red.log`、`build-red.log`および`inputs-red/`。

Greenの段階実行記録。段階確認を重ねたため、受け入れ条件の全fixtureが揃う前にも実行が複数回発生した。中間成功は最終受理に使わず、下記final実行で受理する。

- 11:31:10頃: build/tidy失敗。`pair_delimiters` 13、`coherent_pairs` 12、`emit_token` 11。`build-green-attempt1-failed.log`。
- 11:31:50: partial selector exit 0。`phase-green-partial-result.json`、`selector-green-partial.log`。
- 11:32:38: 親label跨ぎfixtureのoracleが設計と不一致。静的traceで訂正し、新source実測と一致。`phase-green-attempt2-failed-result.json`、`selector-green-attempt2-failed.log`。
- 11:33:21: destination内画像印fixtureのoracleが設計と不一致。destination非表示の契約に訂正し、新source実測と一致。`phase-green-attempt3-failed-result.json`、`selector-green-attempt3-failed.log`。
- 11:33:44: pre-unclosed selector exit 0。`phase-green-pre-unclosed-result.json`、`selector-green-pre-unclosed.log`。
- 11:34:17: 中間build/tidy・selector exit 0。原本は`pre-final/`に保全。
- 11:38:18: 全fixtureを含むfinal build/tidy・`--markdown-links` exit 0。`phase-green-result.json`、`selector-green.log`、`inputs-green.json`。exe SHA-256 `2DA6D155A6E4141E4B92FA959390042D8FB7CF9ACC2DAA893E2084CB3BC8C831`。

green exeと入力snapshotは試行ごとに同名で上書きされた。過去の個別exe/input snapshotは保全されていない。今回最終前の11:34:17 exe・selector・result・inputs・compile commands・build/configure logを`pre-final/`へ保全した。red exeは別名で保全されている。

Final green後、toolchain読込後の`clang-format --dry-run --Werror`は対象3ファイルでexit 0。`git diff --check`もexit 0。FileAPIを使った`python eng/conformance.py --build-dir D:/NeNeFolio/outputs/422-image-literal/build`は初回に品質記録のRTF例をMarkdown linkと誤認してCNF-006を1件報告した。記述修正後の再実行はexit 0、0 violations。これらのコマンド出力は個別logファイルへ保存していないため、実行記録は`D:/NeNeFolio/agents/422-image-literal/commands.json`に結果と保存有無を記す。

実行証跡一式は`D:/NeNeFolio/agents/422-image-literal/`。最終対象実行ファイルは`D:/NeNeFolio/outputs/422-image-literal/folio_tests-green.exe`。統合・CI・通常exe反映は親担当。
