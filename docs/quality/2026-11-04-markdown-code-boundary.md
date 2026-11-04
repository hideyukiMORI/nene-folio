# destination内backtickと後続codeを分離する — #597

画像パス/title中のbacktickが後続本文のbacktickと先に対応付けられ、画像参照または後続codeの書式を失っていた。
`markdown_inline_tokens`のcode/autolink/label/destinationを左から確定し、成立したdestinationを外側code候補へ渡さない。
最終対象検証は成功。最終headの必須CI・正典統合・通常反映/data照合・全保全/整理は完了原本へ確定する。

## 再現・範囲・互換変更

main `dd50b81`と製品入力が一致する#595最終Debug3libを全SHA照合して再利用し、16入力を観測した。
bare path単独は画像1件だが、後続code付きでは0件。parenthesized title、末尾backtick、連続画像も欠落し、
山括弧path/quoted titleは画像が残る一方、後続codeが通常文字と末尾backtickへ崩れた。bare Webリンクもfieldが消えた。
原本は`D:/NeNeFolio/agents/markdown-code-boundary-20261104/observation.json`。

一次根拠は[CommonMark0.31.2 §6.3](https://spec.commonmark.org/0.31.2/#links)と既存ADR0070。
公式package `commonmark@0.31.2`のregistry SHA512/SHA1を取得archiveと照合し、dist/library/package/licenseの4named regular fileだけD側へ抽出した。
同16入力のAST/HTMLを実行取得。新しい製品依存にはしていない。画像altの装飾は既存の文字保持仕様を維持し、参照実装との比較外。

旧#422の交差入力一例は、outer-link優先から、左から成立した画像destinationをliteralとして扱う優先順位へ明示変更した。
旧期待と説明は当時の未対応画像の文字保持設計へ合わせたもので、独立レビューでも維持必須の別要件は見つからなかった。
固定参照に加え、旧/候補nativeの追加14入力で画像数とtargetを照合した。候補14件は全て参照と一致する。
裸pathの開き角括弧とparenthesized titleの閉じ角括弧で壊れていた外側link書式も回復する。
旧fixtureを削除せず完全RTF一致の期待を訂正し、外側成立/不成立、target/span、Web fieldの対照を追加した。
既存の別fixtureでリンク後の本文をdestinationへ吸収していた期待も、固定参照で本文保持を確認して訂正した。
これらを互換不変とは報告しない。成立済み外側linkのlabel内に完全に収まる画像だけを含める制約は維持する。
画像参照の増加は既存の遅延読込/ZIP同梱にも反映されるため、表示だけの変更ではない。

単に未決の外側括弧がある場合に内側画像を拒否する案は、別の正常画像まで失う。
候補ごとの前方確認も深い反復で二乗化し得ると独立レビューで指摘され、不採用。製品へ実装していない。

## 実装と自己レビュー

製品変更は`src/core/markdown_inline_tokens.c`だけ。既存tokenへraw丸括弧の対応を計算し、既存scratchへ裸pathのsuffix境界を右から計算する。
通常本文のcode/autolinkとlabelを同じ前進走査で確定し、成立したdestinationを飛ばす。失敗候補は通常のinline走査を続ける。
先頭空白と成功して消費する範囲はtokenを前へ進め、parenthesized titleの先読みだけ有界な二分探索を使う。
codeの次候補表は既存tokenへ保存してから同じscratchを境界表へ再利用し、後のcoherenceにも同じ領域を使う。
tokenの構造/確保回数/容量guard、公開API/owner、原文/保存schema/依存/版/gateは不変。
構造範囲の判定だけを行い、値の検証・元bytesspan・escape/entityは既存markdown_destination、local percentはimage_sourceが担当する。
RTF出力と画像exportの経路は変更しない。旧全体code先行処理を残す別経路は作らない。

ARC-001/002/003/004/010/012、C-002/003/005/008/011/012/016、QLT-008/009/010/012/013を自己レビュー。
読み取り専用の独立レビューは境界の互換変更を指摘し、根拠/完全一致assert/入出力影響を明記した採用判断を確認した。
費用改善の追加差分も読み取り確認。scratch初期化/容量、indexの前提、更新順に追加の具体的欠陥は見つからず、一般的証明とは扱わない。
原本は`D:/NeNeFolio/agents/597-markdown-code-boundary/independent-review.md`。

## 対象検証と全試行

unit3fileへ画像target/span16例、連続画像2件の独立性、inline完全一致19例、外側境界4対照、Web field6例を追加した。
既存2fixtureの完全一致期待を上記根拠で訂正した。既定CIにも含まれ、既存`--images`で画像/Markdown/Web/transfer/UI文言の直接境界を確認する。

| run | 結果 |
| --- | --- |
| 初期link-01 / observe-01 | 16入力、子/記録側0、stderr空、原文全不変 |
| reference-metadata-01 / pack-01 / reference-01 | 固定公式packageの取得/実行。全0、stderr空、integrity照合 |
| 初期audit-01 | angle Webもcode崩れと予想しassert1、artifact未生成で記録125。実際はautolinkが優先し正常 |
| 初期audit-02 | 16入力/参照/RTFを実測へ訂正して独立照合、全0 |
| build-red-01 / red-01 | 正規Debug/tidy build成功、画像欠落の回帰で対象unit1、記録エラーなし |
| build-green-01 / green-01 | build成功、旧#422交差fixtureの期待でunit1。互換変更をADRへ明記 |
| link-edge-old/current-01 / edge-old/current-01 / reference-edge-01 / reference-edges-01 / edge-audit-01 | 境界14入力の旧/候補と固定参照、全0、stderr空。28RTFをhash照合 |
| build-green-02 / green-02 / reference-tail-01 | build/参照成功、旧本文吸収期待でunit1。本文保持の独立根拠を取得 |
| build-green-03 / green-03 | 初案の全対象unit成功、全0、stderr/記録エラー空 |
| link-native-old/current-01 / native-old/current-01 | 合成画像12入力を両版で書出し/再変換、全0、stderr空 |
| native-audit-01 | contained参照も再配置されるという外部oracleの誤りで1、artifact未生成で記録125 |
| native-audit-02 | image_export.resolveの既存条件を確認し、推奨contained参照は元bytes保持へ訂正。実行データを再利用して全0 |
| build-release-old/current-01 / link-scale-old/current-01 / scale-01 | 正規Release coreと費用初回24条件、全0。24RTF全bytes一致だが費用増加を観測 |
| build-green-04 / green-04 / build-release-current-02 | 前進参照へ絞った最終候補。全0、stderr/記録エラー空 |
| link-native-final-01 / native-final-01 / native-final-audit-01 | 最終候補の同12入力と独立照合、全0、stderr/記録エラー空 |
| link-scale-final-01 / scale-final-01 | 3版を逆順も含め6processで計測、全0、全24条件RTF一致、stderr/記録エラー空 |

すべてtools/record-run.pyの新規run-dirへ入力snapshot/raw/終了値/artifactを記録し、成功済みの旧版・不変fixtureを再利用した。
正規Debugのallocation injection inactive表示を保持し、今回の対象実行だけで確保失敗注入成功とは呼ばない。

## 実画像・ZIPの独立照合

専用D側へ決定的な17×11 RGBA PNG5枚を合成した。backtick/角括弧/丸括弧のfile名、plain/escape、2画像、コード内/未閉じの除外を含む12入力。
実image_adapter_port→image_export ALL_LOCAL→note_archiveと、export後note.bodyのRTF再変換を実行した。
最終12件はexpected asset数/再変換画像数/code fragment/Web fieldが一致。問題4件で旧版のasset0から必要画像が同梱される。
独立Python zipfileで旧/最終24ZIPのCRC/member全bytes、Markdown元bytes、PNG5枚不変、96artifactの実SHAを照合した。
対照8ZIPは旧版と全bytes/SHA一致。containedかつ推奨images参照なので再配置せず、12Markdownは元bytesのまま残る。
この条件を最初の外部oracleが取り違えた失敗も保存し、製品を変えてoracleへ合わせていない。
原本は`native-final-audit.json`。GUI投入、実ノート、外部ネットワーク、任意ZIPreaderの確認ではない。

## 費用と制限

通常mainのbuildはDebugだったため、D側に旧/候補の正規Release coreだけを構築した。
同じ外部/O2 helperで6種×256/1024/4096/16384反復を測定。QPCtick/frequencyを記録し、RTF構築だけを時計で囲む。
初回は旧→初案の各1process、各条件5sampleの初回を保持し残り4件中央値。最大通常文4.648→5.742ms、
未閉じ山括弧11.549→18.785ms、通常リンク24.967→38.151ms等の増加があり、毎候補の二分探索を前進参照へ変更した。
初案のsource/exe/Release libとrawは保持する。

最終比較は旧→初案→最終→最終→初案→旧の6process。各条件の初回sampleは保存し、残り4×2の8件中央値を用いた。
24条件の初回RTFは6processすべてSHA一致（144file）。最大反復の結果は次のとおり。

| 入力 | bytes | 旧ms | 初案ms | 最終ms | 最終8件の範囲ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 引用符の多い通常文 | 409600 | 6.065 | 7.111 | 4.611 | 4.476〜5.338 |
| 未閉じ山括弧 | 212992 | 18.535 | 16.484 | 14.961 | 12.381〜21.407 |
| 未閉じquoted title | 376832 | 15.374 | 14.996 | 16.045 | 14.100〜16.494 |
| 未閉じlink括弧 | 65536 | 7.238 | 6.812 | 6.301 | 5.976〜10.564 |
| 交差候補の深い反復 | 98306 | 9.825 | 9.943 | 8.436 | 7.413〜10.530 |
| 通常Webリンク | 491520 | 35.940 | 27.666 | 26.837 | 23.931〜36.419 |

実行順を逆にしてもOS負荷・周波数・schedulingは分離できず、範囲が重なる。高速化や個々の変更の因果は断定しない。
今回の6入力系列で極端な長時間化はなく、未閉じ/交差候補の反復も完了したが、一般的計算量/任意入力/実UI遅延の証明ではない。
Release費用計測をsanitizer成功の代わりにせず、Debugの対象成功と区別する。今回の計測にはASan診断や抑制はない。

## 完了経路と残る範囲

製品1/unit3、ADR/MARKDOWN/品質記録/gate-proofs/日報/引き継ぎ/currentの7文書を変更。
文書完成後に整形・conformance・symbols・差分/参照をまとめて確認し、最終headの必須CIから正典tools/merge-pr.ps1へ進む。
確定原本は`D:/NeNeFolio/agents/597-markdown-code-boundary/integration-completion.json`。
3root（追加作業木/対象output/初期観測output）の全file/hash、取込/未保存/ignored/稼働参照/linkを確認して保全・整理し、branch/commitを保持する。
追加waiverなし。既存WVR-0001はWinHTTP callbackの範囲だけで変更なし。実app/実ノート/Store操作なし。
全Markdown/全Unicode/全heap/全Windows環境/実UIの保証ではない。推測file名/glob検索の失敗はjournalへ分離した。
