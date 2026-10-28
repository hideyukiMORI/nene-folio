# Markdown containerの親復帰と所属 — 2026-10-29

Issue: [#404](https://github.com/hideyukiMORI/nene-folio/issues/404)。基点main `5c1240e`。
設計は[ADR0071](../adr/0071-ordered-markdown-containers.md)、利用者向け範囲は[Markdownの書き方](../MARKDOWN.md)。
規則: ARC-001/003/004/008/010/012、C-002/003/005/008/011/012/014/016/017、QLT-008/009/012/013。
Waivers: none。

## 変更と対象境界

quoteとlistの順序、親item、連番、未表示markerを短命の`markdown_containers`へ集め、独立quote数と旧`markdown_list`を置き換える。
raw行/次行/非空paragraph状態を受理し、所属が変わる前にFLUSH_REQUIREDを返して旧段落の出力を済ませる。
同じownerへの純粋な厳密照合をfence/tableの本文と先読みに使い、scopeを抜けた行を通常解釈へ戻す。
CR/LF/CRLFの共有行境界、部分tabの残余padding、原文spanの借用を維持する。

印はitemごとに一度表示する。quote/headingの最初の段落へ最後の印を付け、外側の複数印、code/table/rule/空itemは専用段落へ出す。
63列超の表は全原文を表示し、その行の原文markerを印として扱って合成を重ねない。後続の通常itemは連番表示へ戻る。
保存原文・plain編集・保存schema・設定・依存・品質ゲートは変更しない。

## 実装前の成立性と修正前red

証跡は`D:/NeNeFolio/agents/404-markdown-containers/`。
手書きRTFを専用非入力desktop・120dpiのMSFTEDITへ投入し、初回92件と追加47件の確認を実行した。
これらの件数は投入/原文/印の回数/測定/画像保全の結果であり、配置の採否はnative値と画像で別に判断した。
単一markerのquote/headingへの明示tabは成立。同一code行案は面の左側に切欠きができ、複数markerの同一行案は
本文880twipに対して1440twip相当へ進んだため不採用。codeは専用marker段落、複数markerは外側専用段落を採用した。
初回のSDK macro名衝突によるcompile失敗と、修正後のsource/RTF/BMP/生ログを保全した。

`build-container-red.ps1`は修正前の対象8ソースだけを計装compileし、
`7. parent\n   > quote\n\n   continued\n1. next`で次itemが1へ戻る失敗をexit1で再現した。
`container-red.log`とsource hashを保全。ASan起動の`interception_win: unhandled instruction`診断行も保持し、実行結果と区別した。

## 完成候補の検証記録

完成候補の変更13ファイルは`candidate-source-hashes.json`と最終sourceの比較で不一致0。
元のcheckoutは`5c1240e`、作業branchは`fix/404-markdown-containers`。証跡のcommand/source/input hashから候補を識別する。

| 対象 | 実行と結果 |
| --- | --- |
| Debug製品と対象unit | `build-container-candidate.ps1`から固定toolchainで`cmake --build D:/NeNeFolio/outputs/404-markdown-containers/build --target nenefolio folio_tests -j 4`。clang-tidy込みexit0 |
| Markdown unit | 同buildの`folio_tests.exe --markdown-rtf`はexit0。親復帰/連番、lazyとblock開始、空itemのEMPTY/CONTENT/CLOSED、同親siblingと子list、task、純粋なstrict照合、CR/LF/CRLF、tab残余、全列literal、512段list/512frame混合pathを含む |
| 確保失敗 | `build-container-measurement.ps1`で正典のASan＋allocation probe/profile測定。対象selectorは29箇所の確保拒否で公開out不変、その後の成功まで確認してexit0。canonical buildのinactive表示/rejected0は注入成功に数えない |
| 実core RTFのnative投入 | `build-product-native.ps1`、106 checks / 0 failures、exit0。専用非入力desktop/120dpi、同梱font、600px/100%と330px/150%、日本語＋emojiの選択、元Markdownからのplain再投入、装飾復帰、元thread desktopへの復帰と専用handle解放を確認 |
| 段落実値・画像 | 106件のassertとは別に設計サナがmeasure出力と4画像を読んで受理。親本文440/quote920twip、復帰後440、番号7→8。quoteの同一行印、headingの11pt非bold印と22pt見出し、code前の専用印と一続きの背景・復帰を確認 |
| format / 規約 | 変更したC/header/test13ファイルへの固定clang-format `--dry-run --Werror`はexit0。`python eng/conformance.py`は0 violations/exit0。生ログは`final-format.log`と`final-conformance.log` |

初回の対象buildはblocksのネストとlineの複雑度、その次はownerの複雑度で停止した。
space幅の字句操作、fence行出力、ownerのpreflight/commit/publishを責務に沿って整理し、抑制や閾値変更を加えず修正した。
追加unitの`paragraph\n-\nnext`はsetext見出しになる正しい実装に対してoracleが誤っていたため、
空bulletの非interrupt確認を`+`へ修正した。失敗ログを保全し、setextの優先順位を変更していない。

確保失敗測定の初回だけ、正典templateへ追加したundefined/nullability sanitizerが、既存`text_buffer.c:24`の
malloc拒否からnonnull fieldへ代入する箇所で停止した。sourceコピー/hash/追加flags/ログを
`initial-extra-sanitizer-*`へ保全して別件候補として分離した。`eng/coverage.py`と同じASan＋allocation probeへ戻して再測定し、
製品DebugのASan/UBSan/nullability、既存コード、ゲート設定は変更していない。
測定成功ログにはASan起動の`interception_win: unhandled instruction`診断行が残るが、selectorはexit0まで完了した。
profileと対象branch集計は保存したが、このselectorだけで全体90%gateの成功を主張しない。

nativeは実coreが生成したRTFをMSFTEDITへ投入したfixtureであり、手書きRTFの成立性診断と区別する。
geometryを自動assertした件数として106を報告しない。原文保持/選択/plain復帰等のassertと、段落実値・画像の親レビューを分ける。
同じfont/字下げである対象について、成功不変の結果を担当交代やcommitだけで再実行しない。

最終PR headの`pwsh -NoProfile -File ./eng/check.ps1`と必須CIは親が実施する。ローカル全件gateは実行していない。
commit/push/PR作成・main統合・通常exe反映・作業木/一時出力の整理はこの実装担当の対象検証結果へ混ぜない。

## 限界

native表の左端とcontainer字下げ、残り幅の再配置は別Issueで未解消。#404は所属と終了を修正する範囲である。
512段/混合containerは停止性・解放・印の保持の対象で、極端な深さの全native可視性を保証しない。
専用desktopの合成Markdownと実MSFTEDITの結果であり、物理入力/IME/全OS/人によるreader聴取の保証へ広げない。
公開済みv0.3.0 betaの配布物は変更していない。
