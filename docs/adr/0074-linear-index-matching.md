# ADR 0074 — 部分一致を保持して総比較workを線形に抑える

- 状態: 受理（実装前の独立静読と所有・検証配線の補正を反映）
- 日付: 2026-10-29
- Issue: #446
- 規則: ARC-001/004/005/007/010/012、C-002/003/004/005/007/008/011/012/013/014/016、QLT-008/009/010/012/013
- 関連: ADR0024/0068/0073、FR-032

## 根拠

[現matcherの診断](../quality/2026-10-29-bounded-search-diagnostic.md)では、空name・body=a^N・term=a^(m-2)+b+aの
総workが `2+(N-m+1)(m+1)` と一致した。N4096/m17は73442、N16384/m17は294626、N16384/m257は4161026だった。
各stepの有限workと、検索完了までのO(Nm)の再比較は別の問題である。m257は公開coreだけの診断で、現UIの127 UTF-16単位上限とは区別する。
一般phraseの末尾一致は同診断で5058workだった。全byte走査への単純置換では既存の読み飛ばしを失うため、
同じengineに部分一致の情報を保持し、prefixが無い候補だけ既存のtail選択を使う。

この判断は整数workの削減と上限を目的とする。現行版の実時間、Release速度、UI入力latencyの改善率は根拠にしていない。
基本のprefix法は[MisraのKMP説明](https://www.cs.utexas.edu/~misra/Notes.dir/KMP.pdf)と
[Cornellの文字列検索講義](https://www.cs.cornell.edu/courses/cs312/2002fa/lectures/lec26.htm)を参照した。
以下のtail統合・work会計・所有は、この製品向けの設計と静的推論である。

## 同じownerと一つのengine

`index_filter_build`のprivate stateだけを変える。公開13操作、番号列の所有、借用寿命、READYとfinishの前提、
applicationのpending/cursor、65536政策、同期修復とidleの共用経路はADR0073のままとする。
旧HEAD/FULLによる先頭からの再比較は同じ変更で撤去し、別matcher/fallback/適応閾値を置かない。
単文字の既存SINGLE比較とそのwork定義を維持する。

一般語m>=2では、source長Nに対してrを次の比較byte位置、qをその直前まで一致した語prefix長とする。
`r>=q`、`q<m`、`fold(source[r-q:r])==fold(term[0:q])`、開始位置がr-qより前の候補は排除済み、という不変条件を守る。
prefix表・tail表・SCANの比較はすべて既存`ascii_fold_byte`へ揃える。非ASCIIはUTF-8 byte完全一致のままで、正規化や別decode matcherを追加しない。

1. q==0だけでTAILへ入る。N<mなら比較前にsourceを終える。r>N-mならTAILへ進まず終える。
   `source[r+m-1]`の既存256要素skip表を一回読み、1workを消費する。
   非zeroなら`amount > (N-m)-r`の差分で終端を判定し、安全な範囲だけrを進める。zeroなら同じrからSCANへ移る。
2. SCANは`fold(source[r])`と`fold(term[q])`の比較を一回だけ行い、1workを消費する。
   一致ならrとqを一つ増やす。q==mを先に一致として確定し、それ以外はr==Nでsourceを終える。
3. 不一致かつq>0なら`q=prefix[q-1]`としてrを保持する。q>0なら次のworkでSCANを続け、q==0ならTAILへ戻る。
   fallbackのwhileを一つのtick内で無料実行しない。
4. 不一致かつq==0ならrを一つ進めてTAILへ戻る。source切替ではr=q=0とする。
   q>0のSCANへr>N-mの判定を適用しない。たとえばab/abのr=1,q=1でも最後のbを比較する。

entry開始、nameのbyte/NUL長走査は既存どおり各1work。BODY/DONE等のphase遷移は同tickの無料処理に保つ。
name優先、body欠損はnameによらず非一致、nameとbodyの非連結、台帳順、明示長、count0を維持する。
work0は進まない。正workでMOREなら消費し、work1反復で完了する。残work0でentryが終わってもそのcallで完了を返す。
SEALEDのstepは書込もentry/term/prefixの参照も行わない。

## 比較量の上限と限界

SCANでrを増やす比較はN回以下。正prefixの不一致はqを少なくとも1減らし、その減少総量は一致による増加総量以下なので、
追加のSCAN比較もN回以下となる。TAILの開始位置rは毎回厳密に増える。非zero skipがrを進め、zero-tail直後のq0 SCANも
一致/不一致どちらでもrを進める。後のq0 fallbackもその増加後なので、TAILはN回以下となる。
したがって、一般語の一つのsourceに使う比較workは3N以下である。

bodyが存在する一般語entryでname長L・body長Nなら、entry開始/name長を加えて `2+4L+3N` 以下となる。
この式は数学的な上限であり、製品で最大size_tへそのまま乗算するコードを加えない。回帰では小さい固定入力で使う。
SINGLE、body欠損、count0は各既存契約に従う。

語検証、O(m+256)の前処理、確保と解放、applicationの初期集計/lookup、I/O、同期修復、UI/描画は
この比較workの実時間上限の外である。seal時のprefix解放もworkへ数えない資源処理で、壁時計上限を約束しない。
固定phraseの5058以下という回帰条件も、任意phraseの速度や非退行を保証するものではない。

## prefixの構築・所有・解放

prefixは語の各prefixに対する最長proper prefix/suffix長を持つsize_t[m]で、検索中は読み取りだけに使う。
通常createでm>=2かつcount>0のときだけ確保する。SINGLE/count0/removedでは確保しない。
createのNO_TERM→MALFORMED→容量拒否の順を維持し、count+1の既存上限と、prefix長が`SIZE_MAX/sizeof(size_t)`を超えないことを確保前に検査する。
巨大lengthを小さい実体で偽装する試験は、有効spanという前提を破るため作らない。

通常createとremovedは同じprivate `allocate_build(count, prefix_length, out)`を使う。
owner/categories/notes/prefixはnullable localの4資源として取得し、全成功後だけ必須memberへbindする。
失敗時は同moduleのprivate `parts_destroy(build, categories, notes, prefix)`へlocalを渡す。
未完成ownerを公開destroyへ渡さず、失敗したcreateのoutと既存ownerを保持する。
prefix準備は全確保後に同owner内で線形に行い、追加確保をしない。

private `prefix_destroy`だけがprefixを解放し、構築用parts_destroyとsealで共有する。
sealはprefixを破棄してmemberをnullにし、term借用も解除する。公開destroyも成立済みownerから同parts_destroyを呼ぶ。
途中destroyでも全所有資源を片付け、借用term/name/bodyは解放しない。
一般語比較でprefixが存在することをnonnull bindingの前提とし、nullを空結果や別matcherへ逃がさない。
SEALED後のquery/removed/finish OOM retryはtermとprefixの寿命に依存しない。

一時メモリO(m)と新しい確保1回の失敗点は受け入れる。prefixは完成filterへ残さず、既存約2KiBのskip表は引き続き残す。
ARC-005のowner限定許可を拡張せず、公開配列・mutable view・新port・schema・依存・threadを追加しない。

## 検証と移行

旧source/fixture/public work集計/rawとの同一性を照合した#444のW73442を、新上限12290を超す旧挙動の証拠に再利用する。
新しい回帰assertを旧exeで実行して赤になったとは呼ばない。工程だけの旧core再compileや旧native再実行は行わない。

完成候補の既存filter fixtureへ周期4組の既知非一致/3N上限、単文字65538、同じ末尾phrase5058以下を組み込む。
予算0/1/2/不均等/4096/65536の進行・集合・総work一致、ab/abの最後のbyte、abaca/ababacaの重なり、
aaaab/aaacbaaaabの複数段fallback、nameababa/bodycaxxxの非連結、既存fold/UTF8/非終端spanを確認する。
旧一般語abのexact-workは6から5へ契約に沿って更新し、work4でMORE、次1でALL_DONEを要求する。

`--bounded-index-matching-oom`は既に`run_index_filter_size_allocation_tests`を経由して
`run_filter_allocation_tests`を呼ぶ。既存のstrict注入確認を含む経路を使い、直接呼出しや全体runへの重複追加をしない。
prefixを含むcreateの全失敗点、実nonnull sentinel保持、同入力retry、途中destroy、prefix不要経路、
多文字SEALED後term解放、finish OOM retry、removedを確認する。
warm多文字語受理後のprefix OOMがpendingを停止して全索引を保ち、旧確定集合へ戻らず、再要求で回復することを個別に確認する。
cold多文字sweepや既存warm単文字の成功で代用しない。

自己レビューが揃った完成時に、正典Debug/tidy・限定matching/OOM・File API conformance/差分をまとめる。
測定buildはDebug由来のASan/UBSan/nullability/no-recoverを維持し、同一性が証明できる成功済みbinding proofだけ再利用する。
UI/loop/I/Oは不変なので新native/性能計時は行わない。最終PR headの必須CIは既存`eng/check.ps1`。
ADR0073の一般語work/scratch寿命とheader注記を本ADRへ移行し、ADR索引・品質/状態文書を整合させる。
規則のactive/planned・閾値・除外・抑制は変更しない。

初期レビューでは上記OOM間接呼出しを見落とし、追加配線が必要と判断したが、実装中の静読で確認して22:47に訂正した。
追加呼出しは未実行のまま撤回し、既存経路を維持する。新prefix/warm多文字失敗のoracle追加は引き続き必要である。
独立静読と採用原本は`D:/NeNeFolio/agents/quality-post444-search/`。新taskは`D:/NeNeFolio/agents/446-linear-index-matching/`。
統合・通常反映・恒久全hash保全・未保存/ignored/link/稼働参照監査後に新作業木/出力を安全整理し、branch/commitを保持する。
削除拒否44pathは再試行しない。Waivers: none。
