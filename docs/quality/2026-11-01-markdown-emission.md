# Markdown文字出力の連続部分をまとめる — #514

hideの「対応できそうなところは端から対応して」に従い、#512に続いて本文変換を切り分けた。
通常2,000段落の観測ではbuffer追加152,070回、RTF文字追加142,000回で、通常文字も1byteずつ呼び出していた。
構文解釈・生成RTF・保存内容を維持し、この細かな呼び出しを削減した。

## 変更と正典経路

- `rtf_text.c`はCRの除去とbackslash/braceのescape境界までの連続bytesを、同じ`text_buffer`へまとめて渡す。
- `markdown_rtf.c`の編集用RTFは既存のCR/LF/CRLF境界までを、同じ`rtf_text_append`へ渡す。末尾段落も維持する。
- `markdown_inline_tokens.c`は行内文字を最大256byteのローカルscratchへまとめる。文字参照、表のescaped pipe、
  改行の空白化の順序を維持し、参照出力前・満杯・末尾で同じescapeへ渡す。別parser、全文複製、永続cacheを加えない。
- `markdown_tests.c`は254〜257/511〜513byte付近のUnicode・参照・RTF記号・改行と表のコードを確認する。
  固定scratchにheap確保はなく、OOMは既存のexhausted/生成物破棄/out不変へ合流する。

規則: ARC-001/002/003/004/007/008/010/011/012、C-002/008/012/013/016/017、QLT-008/009/010/012/013、GIT-001〜004。
[ADR0078](../adr/0078-lightweight-markdown-inline-rendering.md)を補正した。
公開API・schema・版・依存・申請済みMSIXは不変。Waivers: none。

## 検証

原本は`D:/NeNeFolio/agents/markdown-emission-20261101/`。各実行を`tools/record-run.py`で新規directoryへ記録し、
入力/成果物のsnapshot・SHA256・raw出力・終了値を保存した。`pre-integration-audit.json`は最終sourceとbuild、
test exe、計測library、保存snapshotの対応を確認する。`findings.json`が計測とbytes比較の原本。
最終head必須CI、main、#512と合わせた通常版反映、全hash保全と追加作業物整理は`integration-completion.json`で確定する。

| コマンド/実行 | 結果・範囲 |
| --- | --- |
| `pwsh -NoProfile -File …/build.ps1` / debug-03 | 正典Debug・clang-cl/tidyで製品と`folio_tests`のbuild成功 |
| `build/folio_tests.exe --markdown-rtf` / unit-01 | 変更境界と既存Markdown対象単体試験成功 |
| `measurement.ps1 build` / allocation-build-01、`measurement.py run` / allocation-run-01 | 正典sanitizer vectorと既存allocation probeの測定build。同じ`--markdown-rtf`だけを実行して成功。追加閲覧入力9確保点、編集入力5確保点で失敗を注入しout不変を確認 |
| final-before/afterの`profile.ps1` compile/run | 5種類×閲覧/編集×3回の比較。正典compile/link引数と最終library hashを照合。観測用sourceとの差分は計測だけ |
| before/afterのprobe `--corpus` | prefix 0〜520byte×7種×閲覧/編集の7,294件。長さを含む4,787,367bytesが完全一致 |
| `final-check.ps1` | conformance/実依存graph、core/application symbol、変更Cの整形と差分を確認 |

Debugの初回は`append_range`の関数長指摘で停止した。既存の1文字変換を小関数へ分離した。
2回目は追加試験の`strcpy`が安全性検査で停止し、長さ付きの`snprintf`へ修正した。
失敗時はexe未生成によるartifact snapshot失敗も記録されている。3回目と以後の対象試験は成功した。
抑制・除外・閾値変更はしていない。

速度比較final-01は、修正後driverのlibrary rootが親作業木を指していたため採用していない。
参照先を修正してfinal-02を再compileし、library hashと実行exeを照合した。
親の同一exeは別directoryへコピーして再実行し、その対応を`clone.json`へ残した。
観測版の文字出力も5種類×2モードすべてで非観測版と全bytes一致する。失敗/不採用記録も保存する。

## 実測と限界

Windows x64、正典DebugのASan/UBSan/nullability付き、合成文書、各3回の中央値ms。
全体時間は観測フックなしの生成処理だけを測り、呼出し回数は同じコードへ観測を追加した別実行で数えた。
比較用processは順番に実行した。Release、実ノートの切替全体、RichEdit投入やDWM呈示の値ではない。

| 合成入力 | 閲覧: 前→後ms | 編集: 前→後ms |
| --- | ---: | ---: |
| 通常2,000段落 | 55.598 → 47.134 | 6.842 → 1.808 |
| コード2,000行 | 7.079 → 3.219 | 3.932 → 1.015 |
| 表3,000行 | 83.365 → 81.891 | 6.013 → 1.445 |
| 連続2,000行 | 71.946 → 67.310 | 6.926 → 1.633 |
| 日本語・参照・装飾2,000段落 | 80.210 → 74.434 | 9.164 → 2.866 |

通常2,000段落の閲覧ではbuffer追加152,070→12,070回、RTF文字追加142,000→2,000回。
編集ではbuffer追加146,072→6,072回。生成bytesは同一で、再配置や構文の省略で速めてはいない。
OS負荷で時間は変動し、特に表の閲覧は小差のため一般的な改善率を主張しない。
本文変換の構文処理とRichEdit内部の投入/表配置には負担が残る。これらを避ける新経路は追加していない。
GUI経路の変更は#512で確認し、今回は同一RTFを渡すcore出力境界に絞って確認した。
