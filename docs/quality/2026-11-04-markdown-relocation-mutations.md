# 画像パス置換の変異入力 — #599

#597でinlineのcode/label/destinationを左から確定するよう変更したため、固定例の対象検証に加え、
元byte範囲を使う画像パス置換の不変条件を調べた。6seedから生成した11,703入力で、参照範囲・原文不変・
置換後の画像数/targetが一致し、Pythonで独立構築したコピー本文とも全bytesが一致した。
製品/test/依存/schema/gateは変更しない。

## 入力と実行経路

bare path内backtick、angle pathと日本語、外側linkとparenthesized title、blockquote/listとCRLF、
escape/percentを含む2画像、表内の画像/codeの6seedを使った。seedはそれぞれ1/1/1/1/2/1画像として実行確認した。
21token（括弧・引用符・backtick・escape・装飾・文字参照・空白・LF/CR/TAB・日本語）をcodepoint境界で
一文字挿入/置換/削除/途中切断し、重複bytesを除いた。乱数を使わず、UTF-8valid・NULなし、長さ0〜58bytes。

| 生成操作 | 件数 |
| --- | ---: |
| 元seed | 6 |
| 途中切断 | 249 |
| 挿入 | 5661 |
| 削除 | 252 |
| 置換 | 5535 |
| 合計 | 11703 |

corpusはlittle-endian長さと原文bytesの連続で601794bytes、SHA256は
`227aa97b25ff0138044b96802f7191d70a79772d57c86a5ad8dc174dc4836b36`。
全入力の生成操作/位置/token/原文/byte長/SHAをcorpus.jsonへ保存した。

#597最終候補`aae6706e0d808e1093712871f1f5c2a9a1571412`の正規Debug3libを、成功したlink-native-final-01の
inputhash・現filehash・copyhashで照合して再利用した。外部helperは正規toolchain/lld/明示ASan+UBSan runtimeでlinkした。
製品の`note_text`→`markdown_rtf`→`markdown_images`→`markdown_image_copy`→再`markdown_rtf`を呼ぶ。
helperだけがcorpusのfile I/Oを行い、製品へ試験用APIや依存を足していない。

## 観測と独立照合

各入力について次を実行した。

- 画像参照のspanが元原文の範囲内にあり、空/逆転/重複しないことを確認。
- すべての画像pathを`images/relocated-N.png`へ置換し、コピー本文を再変換。
- 画像数と各targetが置換前の順序/期待先に一致することを確認。
- 元note_textのbyte長/全bytesが入力と同じであることを確認。
- nativeの元span列からPythonがコピー本文を独立に構築し、native出力の全bytesと比較。

| run | 結果 |
| --- | --- |
| link-01 | 子/記録側0、stderr/記録エラー空 |
| check-01 | native/評価/記録側0、11,703行の全ID/byte長/元SHA一致、stderr/記録エラー空 |

画像0件2229入力、1件8148入力、2件1326入力。全11,703件でspan/原文/置換/再変換の不変条件が成立し、
Python独立copy bytesも一致した。180秒timeoutに到達せず、外部process終了まで3.949秒。
これはfile I/Oと出力も含む診断時間で、性能値として扱わない。

実行は既存tools/record-run.pyの新run-dirへ入力snapshot/raw/終了値/artifactを収載した。
失敗試行はない。成功済みの#597固定unit/native/性能計測は繰り返していない。
原本は`D:/NeNeFolio/agents/markdown-relocation-mutations-20261104`、生成scriptは同日の全体agents領域。

## 探索で除外した候補

ZIP path比較は既存ASCII同一視の規約で、Unicode大小文字の全同一性は#593でも対象外としている。
image_fileの最終正規path/identity、contained pathと外部画像の生成ASCII名を読み取り、新しい実到達欠陥を認定する根拠は得られなかった。
この候補には実行試験/製品修正を行わず、一般Unicode同一性を保証する結果へ数えない。
Markdown viewの行境界は長さの間に1byteの仮想改行を置くため、次行先頭と直前行末は同じoffsetにならないことも読取確認した。

推測file名/Windowsglob指定の読取失敗、GitHub読取APIの引数schema errorはjournalへ記録して訂正した。
これらを製品検証成功へ数えない。

## 制限と完了経路

構文受理/拒否や表示内容全体を正しいとするoracleは置いていない。画像0件のケースも本文全bytes不変は確認したが、
本来画像として解釈すべきかは判定しない。CommonMark完全互換、全Unicode、任意長/任意入れ子、全heap、
実画像読込/ZIP/実UI/全Windows環境/性能の保証ではない。確保失敗注入・heap leak検査は今回の対象外。
nativeに診断がなかったことと、任意入力で診断がないことを区別する。

ARC-001/004/008/012、QLT-008/012/013、GIT-001〜004、ADR0070/0085を確認した。
品質記録/gate-proofs/日報/引き継ぎ/currentの5文書だけを変更し、文書参照/差分/関係規約を確認する。
最終head必須CI・正典統合後、#597通常exeとの製品入力/実SHA不変を確認し、再build/data操作は行わない。
追加作業木とoutputの全file保全/hash照合、取込/未保存/ignored/稼働参照/link確認後に整理する。branch/commitは保持する。
確定原本は同agentsのintegration-completion.json。追加waiverなし、既存WVR-0001は変更なし。実app/実ノート/Store操作なし。
