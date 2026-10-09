# #236 — 閲覧本文をUTF-8 RTFで保持する

2026-10-10 JST。規則: ARC-001 / ARC-003 / ARC-009 / C-005 / C-014 / QLT-009 / QLT-013。
Waivers: none。NeNeFolioサナが単体で調査・実装・検証。通常データや起動中アプリは使用していない。

## 再現と決定

base `1bb1616` の `markdown_rtf` はU+FFFDをASCII RTFの `\u-3?` にしていた。
RichEditはその文字を入力時に捨て、`A` / U+FFFD / VT / `B` の例ではVTも消えた。
VT単独は保持されたため、VT全般がRTFで失われるという意味ではない。
符号なしescape、uc0、fallbackの変更でもU+FFFDは保持できなかった。

[MicrosoftのUTF-8 RTF入力](https://learn.microsoft.com/en-us/windows/win32/controls/em-streamin)を
実測し、本文をUTF-8のまま生成して唯一の入力境界でcodepageを明示する方式をADR 0002へ先に記録した。
旧UTF-16単位への変換と `\uN?` の生成を削除。構文記号のescape、CR扱い、Markdown記法・palette・faceは維持する。
WPARAMへ広げてからCP_UTF8をshiftする。初期probeのsigned int shiftはUBSanが拒否し、製品へは入れていない。

## 対象検証

- `build/folio_tests.exe --markdown-rtf`: exit 0。
  既存のMarkdown記法・3言語のface・paletteに加え、UTF-8の保持、隣接VT、RTF構文文字を検証。
  既存のallocation scenarioにもU+FFFD/VTを加えた。確保失敗は正典ゲートの測定buildで確認する。
- `python eng/conformance.py`: 0 violation。製品と対象testのbuild、`git diff --check`: 成功。
- `pwsh -NoProfile -File D:/NeNeFolio/agents/236-utf8-rtf/run-probe.ps1`: 実Win32 **93ケース・896 assertion、exit 0**。
  3言語×2テーマ×15本文の90ケースと、先頭byte位置を変えた長文3ケース。
- 同じprobeを `-TaskTree C:/Users/info/WORKS/NeNeFolio -Label red` でbaseへ向けると、
  case 2 / assertion 11で失敗。`A` / U+FFFD / VT / `B` の期待4単位に対し、実体は `A` / `B` の2単位だった。

実Win32では製品の `note_text → markdown_rtf → note_pane_render → note_pane_text` を接続した。
空本文、U+FFFDと隣接VT、日本語/中国語/アクセント/絵文字、見出し、太字、斜体、inline/fenced code、
リンク、引用、箇条書き、番号、Markdown/RTF特殊文字、CRLFを確認した。
本文型の元byte列は変わらず、閲覧はread-only、U+FFFDの検索・選択位置が表示本文と一致する。
強調/下線と文字サイズを確認し、ASCIIコードのConsolasも実測した。
非ASCIIの字形をOSが別fontへbindingする場合を、fonttblの指定そのものと混同しない。
font指定の補助probeではbase/修正後ともConsolasと200 twipsを確認した。

長文は `é日😀�` を16,385回、196,620 byte（UTF-16では81,925単位）並べ、先頭にXを0/1/2個置いた。
文字列全体と検索位置が一致した。幅600×高さ400の非表示部品で93ケース全体に約5分10秒を要し、
これは速度改善の証明ではない。旧版との時間比較やOS内部の遅延原因は測定していない。
最初の幅0のprobeは長文中に自分の測定processを停止し、通常幅へ直してから上記成功を得た。

現物は `D:/NeNeFolio/agents/236-utf8-rtf/` の `probe.c` / `run-probe.ps1`、
`green-probe.log` / `green-result.json`、`red-probe.log` / `red-result.json`、`baseline/`。
ASan起動時の `interception_win: unhandled instruction` は残っており、警告の無い実行とは報告しない。
正典ゲートの結果は `gate-result.json` / `fullgate.log`、統合・通常exe・整理は `completion.json` に保存する。

00:59:39 JST、source `bc28220` の `pwsh -NoProfile -File ./eng/check.ps1` はexit 0。
CTest 2/2、分岐3233/3448（93.7645011600928%）、低網羅の拒否、実道具proof19件が成功した。
4 JSONを作業木外の `gate-evidence/` へSHA256照合して保存。以後の変更は結果を記録する文書だけである。

## 範囲

今回の共通rendererとUI入力境界に関係する測定だけを行った。物理IME、高DPI、他画面の操作は繰り返さない。
成功済みのソースをpush/mergeだけのために再測定しない。保存schema、依存、ゲート閾値・除外は変更なし。
U+0007/LS/PSの平文編集入力は別の境界で、今回修正済みとは扱わない。
