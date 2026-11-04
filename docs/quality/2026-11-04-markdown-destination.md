# 画像パスとタイトルの丸括弧を構造から分ける — #595

`![x](<images/open(.png>)` / `close)`や、引用符title内の片側丸括弧を一般の括弧対応付けへ数え、画像参照を見失っていた。
実画像4入力で書き出し自体はREADYでも、ZIPがMarkdownだけになる反例を確認した。
既存inline tokenの対応付けで、destinationの山括弧内と引用符titleをliteralとして扱い、同じ画像/リンク経路へ戻した。

## 再現と設計

main`0eaf2ee`の製品入力で、既存成功Debug3libをSHA照合して再利用した。初期原本は
`D:/NeNeFolio/agents/markdown-destination-observe-20261104/observation.json`。
14原文のうち画像の問題4入力は参照0件、plain/balanced/escape/percentの6対照は1件。
Webの山括弧例はHYPERLINK存在だけを観測し、label/fieldの正しさはこの段階では認定しなかった。
実image_adapter_port→image_export ALL_LOCAL→note_archiveで6合成入力を実行し、問題4件のZIPはMarkdown1fileだけ、対照2件は画像を同梱した。
独立Python zipfileで全ZIPのCRC/member bytes、元Markdown/画像の不変を確認した。

境界の一次根拠は既存ADR0070の採用方針と[CommonMark0.31.2 §6.3](https://spec.commonmark.org/0.31.2/#link-destination)のdestination（例499）/title。
完全互換を追加するものではない。Issue595を作り、D側作業木で既存unitのredを確認後、製品編集前にADR0070へ方針を追記した。

製品は`src/core/markdown_inline_tokens.c`だけ。認識済みlabelの直後の`(`に属する範囲に限り、
山括弧destinationと空白後のsingle/double quoted titleを構造括弧から除外する。
既存literal tokenの内部からtitle引用符を探し、元の字句分割を維持する。
山括弧のnested `<`/改行は打ち切り、escapeされた閉じ文字を終端にしない。成功範囲を飛ばし、先頭空白の確認は直前へ逆走する。
妥当性/原文span/escape/entityは既存markdown_destination、local percent展開はimage_sourceが引き続き担当する。
新しい公開API/owner/持続状態/全体文字列コピー/依存/schema/版/gateは追加しない。

初案は引用符を全本文の字句境界へ加えたため、引用符の多い409600byte通常文のDebug診断が旧50.205→178.793msとなった。
この案を採用せず、destination内だけへ走査を絞った。同じ入力の最終候補は51.282ms。元の初案source/rawも保存する。
起動診断があるため正確なRelease/UI性能や単一原因の証明とは扱わないが、複数入力サイズでの大きな増加を実装見直しの根拠にした。
ARC-001/002/003/010/012、C-002/003/005/008/011/012/016、QLT-008/009/010/012/013を自己レビューし、追加の独立レビューは行わない。

## 対象検証

既存image_testsへ22positive、未閉じ/コード/escape除外5例、連続3画像を追加した。
丸括弧/角括弧/日本語、single/double quote title、escapedquote、percent、画像を囲むlink、引用/list/tableを含む。
実参照のtargetと元bytes内のspan位置を照合し、原文不変を要求する。web_link_testsへ6fieldのURLとtitle非漏出を追加した。
既定CIにも含まれる。`--images`は既存の画像/Markdown/Webリンク/transfer/UI文言境界をまとめて通す。

| run | 結果 |
| --- | --- |
| 初期link-01 / observe-01 | 14原文の観測、子/記録側0・stderrなし |
| 初期link-export-01 / export-01 / inspect-01 | 実画像6例とZIP独立読取、子/記録側0。問題4件で画像欠落を認定 |
| build-red-01 | 固定Debug/tidy、folio_tests+nenefolio_adapters、parallel2。子/記録側0 |
| red-01 | `folio_tests.exe --images`、最初の角括弧path回帰で子/記録側1、記録エラーなし |
| build-green-01 / green-01 | 初案の対象build/unit成功、子/記録側0 |
| link-native-01 / native-01 / verify-01 / audit-01 | 初案の実画像6件/関連規約/独立読取成功。性能診断の制限は下記 |
| build-green-02 / green-02 | 通常文へのtoken増加を除いた最終候補。子/記録側0、stderr/記録エラーなし |
| link-native-02 / native-02 | 同じ実fixture6件すべてREADY・画像1件同梱、子/記録側0、stderr/記録エラーなし |
| verify-02 | 整形/conformance違反0/symbols2library違反0/diff成功。子/記録側0、stderrはCRLF案内のみ |
| audit-02 | ZIP6件のCRC/member全bytes、対照2ZIPの修正前全bytes一致、原文6/画像3不変。子/記録側0 |

最終6ZIPは全て`A/note.md`と正しい画像1fileで、Markdownの記法・タイトルを含め元bytesが一致する。
通常/escape対照2件は修正前ZIPと全bytes/SHA一致。初案の同じ機能成功は保持し、実装変更後の対象だけを再確認した。
正典Debugのallocation injection inactive表示を保持し、今回の確保失敗注入成功とは呼ばない。
全runはtools/record-run.pyの入力snapshot/raw/終了値へ記録し、既存runを上書きしていない。

## 長い入力の診断と失敗の記録

未閉じ角括弧/未閉じquotedtitle/記法でない記号列/引用符の多い通常文の4種を、各256/1024/4096/16384回反復した。
各条件5sampleのうち初回を別に保持し、残り4件の中央値を比較した。全16条件のRTFは旧/初案/最終候補で全bytes/SHA一致。
各native processは型付き変換結果/画像0件/原文不変を確認した。測定はDebugで、UI/Releaseのbenchmarkではない。

初回scale-01は旧processのASan interception_win unhandled instruction診断1行で評価1、summary未生成で記録125。
浮動小数printfをraw QPCtick/frequencyへ変更したscale-02でも同じ診断が残り、評価1/記録125。両方のrawを保持した。
currentとfinalは別の明示的診断runで子/記録側0だが同じ初期化診断1行を出すため、scalingSanitizerSuccess=falseとする。
これらをsanitizer成功へ読み替えず、抑制/無効化/設定緩和を行っていない。unitと実画像nativeのstderrは空。

最大反復の旧→最終中央値は下表。長さ4倍の観測でも極端な増加は見られなかったが、一般的な計算量の証明ではない。

| 入力 | bytes | 旧ms | 最終ms | 比 |
| --- | ---: | ---: | ---: | ---: |
| 未閉じ山括弧 | 212992 | 116.332 | 144.620 | 1.243 |
| 未閉じquotedtitle | 376832 | 114.070 | 128.922 | 1.130 |
| 記法でない記号列 | 409600 | 106.194 | 113.584 | 1.070 |
| 引用符の多い通常文 | 409600 | 50.205 | 51.282 | 1.021 |

単一processずつの診断でありOS負荷/実行順を分離していない。未閉じ入力の増加も記録し、無費用/高速化を断定しない。

## 完了記録と限界

製品1/unit2とADR/MARKDOWN/品質記録/gate-proofs/日報/引き継ぎ/currentの7文書を変更。
GIT-001〜004/QLT-008/012に従い、最終head必須CI/正典統合/通常反映/data照合/保全整理を行う。
確定原本は`D:/NeNeFolio/agents/595-markdown-destination/integration-completion.json`、全試行は同dirの保全ZIPへ。
初期観測を含む3rootの全hash/取込/未保存/ignored/稼働参照/linkを確認してから整理し、branch/commitを保持する。
追加waiverはnone、既存WVR-0001はWinHTTP callbackの範囲のみ。実app/実ノート試験/Store操作なし。
任意Markdown/全Unicode/全heap/実UI/Release性能は保証しない。推測path/glob検索失敗もjournalへ分離して記録した。

文書生成の初回はMARKDOWN.mdを既定cp932で読んで失敗。書込み済みの品質記録等は重複生成せず、残る2文書だけUTF-8で完了した。
文書verify-doc-01はコード例の画像パスを実リンクと誤認して失敗。外部確認scriptでbacktickのliteral範囲を区別し、verify-doc-02を新規実行した。製品gateや除外は変更していない。
