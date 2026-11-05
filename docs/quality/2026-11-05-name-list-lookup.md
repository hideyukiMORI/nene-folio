# 名前集合の照合を同じ所有者内に集約する — #630

name_listの重複判定/containsが毎回挿入順全体を走査していたため、大量の名前や長い共通接頭辞で費用が増えていた。
挿入順の文字列所有を維持し、同じitems確保の後半に辞書順の借用参照を置く。append/containsは同じprivate lower-boundを使う。
公開API・名前妥当性・byte完全一致・挿入順・借用寿命・永続化schemaを変えない。

## 正典経路と採用判断

Issue #630。ARC-001/004/006/008/009/010/012、C-003/004/005/008/012/016、QLT-008/009/010/012/013、GIT-001〜004。
[ADR0004](../adr/0004-first-drawer-slice.md)へ製品編集前に同一owner内の索引案を記録した。
name_list.cだけを製品変更し、利用者ごとに別の索引や重複判定を作らない。

- itemsは2×capacity個のpointer。前半は挿入順の所有文字列、後半は同じ文字列への借用参照。
- 比較はmemcmp(min length)と長さ。NUL終端のない長さ付き入力を越えて読まない。大小/UTF-8正規化を追加しない。
- reserveは乗算前の上限検査後、reallocと旧後半→新後半のmemmoveを行う。既存文字列の位置は動かない。
- copy成功後だけ後半を挿入shiftし、前半末尾とcountを採用する。copy失敗後も移設した索引が有効で、再試行可能。
- 解放対象は前半count個の文字列だけ。確保回数とreserve→copyの順は不変。

pointer領域の確保量は2倍で、x64のcapacity16384では従来より128KiB増える。
文字列の複製は増えない。逆順/不規則挿入のpointer移動は二次のままで、全体が線形になったとは扱わない。
小集合の一部で増加する費用を受け入れ、大集合の重複照合と台帳読取の大幅削減を理由に候補1を採用する。
追加waiver none。既存WVR-0001はWinHTTP callbackだけ継続。依存/閾値/抑制/ゲートは変更しない。

## 同一条件の比較

8/128/2048/16384件×short/prefix/japanese×forward/reverse/interleavedの36fixture。
prefixは234byte、日本語はUTF-8、各fixtureのnamesと独立JSON台帳を生成して全bytes/hashを保存する。
buildはcreate→全append→destroy、lookupは全N件の存在とN件の不在照合、ledgerはparse→destroy。
各observerの計時前後に挿入順全bytes、台帳全bytes、存在/不在、全duplicate拒否を検査する。

正典Release coreの固定archiveとcompile_commandsからO2/Ob2/DNDEBUG/MT/C23/警告vectorを復元した。
変更するname_list.objだけを先に明示linkし、MAPの6公開symbolがそのobjectを使うことを照合する。
observer sourceは両版共通、thread affinity mask4、優先度不変、QPCとthread cyclesを取得する。
caseごとA/B/B/A、block5sampleで各版10sample。sample内反復は8件512回、128件32回、2048件2回、16384件1回。
fixture読取/出力/検証を計時外へ置き、測定中に親のbuild/hash/archiveを重ねていない。
OS待機や割込は含まれ得るため、一般の実UI速度やOS idleを保証する値ではない。

単位ms、中央値。比は候補/基点。

| kind/order/count/mode | 基点ms | 候補ms | ms比 | cycle比 |
| --- | ---: | ---: | ---: | ---: |
| short/forward/8/build | 0.000782 | 0.000809 | 1.035 | 1.035 |
| short/forward/8/lookup | 0.000446 | 0.000492 | 1.102 | 1.103 |
| short/interleaved/2048/build | 10.011050 | 0.428150 | 0.043 | 0.043 |
| short/interleaved/2048/lookup | 19.684350 | 0.443725 | 0.023 | 0.023 |
| short/reverse/16384/build | 567.839850 | 21.593550 | 0.038 | 0.038 |
| short/reverse/16384/lookup | 1225.053600 | 3.881350 | 0.003 | 0.003 |
| prefix/reverse/16384/build | 3079.479550 | 35.284450 | 0.011 | 0.011 |
| prefix/reverse/16384/lookup | 7576.664000 | 14.037600 | 0.002 | 0.002 |
| prefix/reverse/16384/ledger | 2993.069850 | 46.751950 | 0.016 | 0.016 |
| japanese/interleaved/16384/ledger | 929.183700 | 13.709500 | 0.015 | 0.015 |

108case/2,160timed sampleを完了した。最大のms比はshort/interleaved/8/buildの1.176。
8件の短名などで増加を観測し、全条件の高速化とは報告しない。rawの全108caseを保持する。
audit-01で72fixture file、raw sample/中央値/cycles、MAP、固定reference hash、全input終了時不変を確認した。
整形前の計測sourceと整形後sourceは空白除外で一致し、format-reuse.jsonにhashを記録して再計測を省いた。

## 対象検証

- 16種類の名前×16start×8odd-stride＝128挿入順。各追加後に全prefixの件数/順序bytes/存在/不在を確認。
  入力はexact allocation・非終端で、append後に解放する。大小、prefix、日本語、合成/分解Unicodeを区別する。
  全duplicate、NUL拒否、reserve後も同じborrowed pointerであることを確認する。
- count0/3/4/7/8/15/16×3挿入位置で実確保を順に拒否する。33失敗点を実測。
  失敗後のcount/順序/bytes/contains/borrowed pointer不変、同ownerで反復拒否、解除後retryと重複拒否を確認する。
- 新selector --namesはname_listとnote/category名・台帳の既存回帰を実行する。
  --names-oomは実注入で失敗件数が0なら失敗し、通常Debugを注入試験と誤認しない。
  新testは既存全体suiteにも接続した。

configure-debug-01/build-debug-01、Debug/tidyの--names、既存coverage.build_measurementからの--names-oomが成功。
測定buildはDebug compile_commandsのASan/UBSan/nullabilityとrecover禁止を引き継いだ85製品TU/31test TUで、
ローカルの全suite/coverage判定は実行しない。最終head必須CIのeng/check.ps1で既存統合ゲートを維持する。
--namesはstderr空。--names-oomは子/記録0で、ASan起動時のinterception_win unhandled instruction通知1行を保持した。
stderr空とは扱わず、サニタイザによる全範囲の検出能力をこの成功だけで保証しない。抑制は追加していない。
独立read-only reviewは範囲/移送/比較/所有/失敗後再試行/selector接続に必須修正0。主担当が実装・試験・採否判断した。

## 全試行と限界

最初のmeasure-profile-01は6分布×3modeを予定し、13case完了後に長prefix逆順16kのlookupが診断60秒へ到達した。
所有childを終了し子/記録1、完了値/partial rawを保持した。比較は診断上限だけ180秒とし、observer処理やCI制限は変えない。
configure-release/build-core/fixture/compile両版/measure-candidate-01は成功。profileの既成功部分を再profileしなかった。
補助script名・ADR名の推測readは不存在、PowerShellのbrace列挙は構文拒否、進行中集計readはJSONDecodeError。
いずれも製品/test結果と分け、一覧/正確なpath/stdout追跡へ訂正した。ADR追記の最初の空patchはcontext不一致で未適用。

基点main0e879fb、設定修正#628のmain1dc20301をff済み。実ノート/通常app/Storeは試験に使わない。
恒久原本D:/NeNeFolio/agents/630-name-list-lookup/、全体journal D:/NeNeFolio/agents/quality-20261105/journal.md。
各runの実argv/source/object/exe/入力/raw/終了値はrecord-run.pyで保持する。
文書検査・最終head必須CI・統合・通常反映/data不変・全保全/専用worktree/output整理の確定はintegration-completion.jsonへ。
branch/commitは保持する。任意の大集合、実ディスク障害、実UI待ち時間を網羅する試験ではない。

## 統合・反映・保全整理の確定

PR631 head0e8d3e34の必須CI41949515350 full-gate/check成功後、main23397d7へ正典統合、Issue630 CLOSED。
通常反映/data32file bytes/hash/作成更新日時不変、通常app起動0。1,639fileを全保全/hash再照合した。
取込tree/clean/ignored/link/稼働参照を監査後、630専用worktree/outputを削除、branch/commit保持。
確定原本D:/NeNeFolio/agents/630-name-list-lookup/integration-completion.json。
