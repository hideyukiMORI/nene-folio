# 一覧の行選択を配置順から探す — #635

drawer_layout_hitの先頭からの走査を、行の下端の順序による二分探索へ変えた。
ノート一覧のクリック・右クリック・UIA位置照合は、引き続きこのcore入口を共用する。
通常の有効な合成一覧を使った40条件の比較で、後半の行照合に行数に比例する費用が残ることを測り、
同じ入力と結果で費用を減らせる候補を採用した。実UI全体の遅延原因を特定した報告ではない。

## 契約と変更範囲

Issue #635。ARC-001/003/004/007/011/012、C-005/008/012/016、QLT-008/010/012/013、GIT-001〜004。
[ADR0009](../adr/0009-drawer-scroll-and-overflow-fade.md)の表示座標・頭の帯、
[ADR0015](../adr/0015-cursor-stops-on-collapsed-categories.md)の行参照、
[ADR0024](../adr/0024-index-filter.md)の絞込後も元台帳番号を保つ契約に従う。

既存layout内の行は上から詰めて置かれる。内部座標が行の下端以上なら、その行と前の行は候補から外せる。
下端が内部座標を越える最初の行を求め、最後にその上端を照合してカテゴリ間の余白を除く。
頭の帯を先に除外する条件、スクロール量の加算、上端を含み下端を含まない判定、非一致時のout保持を維持する。
空一覧では行を参照しない。探索区間は各反復で狭まり、保存済み行数以外の状態は持たない。

製品差分はsrc/core/drawer_layout.cの同関数だけ。確保・名前の所有・表示行の作成・mark/reveal/dropは不変。
新しいAPI/cache/owner/schema/依存/ゲートはない。通常UI寸法を使う既存配置の前提を変更していない。
追加waiver none、既存WVR-0001はWinHTTP callbackだけで継続する。

## 計測方法と結果

基点main7113a4c、D専用refactor/635-drawer-hit-lookup。
正典Releaseのnenefolio_core.libとcompile_commands、旧drawer_layout.cをSHA256一致で固定した。
基点と候補のdrawer_layout objectをそれぞれ同じcore archiveより先に結び、MAPの使用7symbolが
明示objectを指すことを照合した。製品への計装や時計の持込みはない。

- 8/128/2048/16384本×1/8カテゴリの8fixture、合計行数9〜16392。正常な名前と台帳の16fileを固定保存。
- top/middle/bottom/gap/bandの5位置、各位置の近傍32座標を繰り返す。表示行の区間から計時外に期待値を求める。
- 各条件でA/B/B/A、各block5sample、各版10sample、計40条件/800sample。
  小さい一覧では表示領域内の空白照合も含む。
- 1sampleあたり262144/65536/8192/2048呼出し。QPCとQueryThreadCycleTimeを取得し、毎回の一致/行番号/out保持を照合。
- observerの1threadだけCPU affinity mask4、優先度変更なし。開始前の既知build/probe0、計時中に親のbuild/hash/archiveを重ねない。
- 台帳parse・layout構築/破棄・期待値生成・file IOは計時外。結果の確認分岐は両版で同じ計時区間に含む。

1カテゴリ、1呼出しあたりの中央値（ns）:

| ノート数 / 位置 | 旧 | 候補 |
| --- | ---: | ---: |
| 8 / top | 2.2671 | 5.3783 |
| 8 / bottom | 5.9036 | 4.0834 |
| 128 / middle | 29.6722 | 7.4371 |
| 16384 / middle | 8525.7568 | 14.6484 |
| 16384 / bottom | 17099.4141 | 15.4053 |
| 16384 / top | 2.2949 | 14.5752 |

全40条件で結果が一致した。最大の費用増は16384本のtopで+12.2803ns（6.351倍）。
元から先頭付近で終わるケースの増加を隠さず、後半の行と通常の空白の走査を減らせることを採用理由にした。
追加のメモリ確保はない。profileの20条件は基点診断であり、比較のA側に混ぜていない。

## 対象検証

新selector --drawer-layoutは既存run_layout_testsと既存の絞込配置をまとめたrun_filter_layout_testsを呼ぶ。
全suiteも同じ関数を使い、別のテスト実装を作らない。

- 1〜10行の一覧、先頭/中央/末尾の3scroll量で各313座標、計9390回。
  期待値はfixtureで定めた14..48のカテゴリ行と28pxのノート行から求め、製品のhitや探索を呼ばない。
- 展開・空・折り畳みを含む8行の既存fixtureを、明示した上端/高さで293座標照合。
- 絞込後の行位置と元note/category番号、カテゴリ間の余白、末尾ノート、0件のout保持を5点追加確認。
- 既存の行所有、mark、scroll丸め/reveal、カテゴリ/ノートのdrop回帰を同selectorで維持。

Debug/ASan/UBSan/nullability/no-recover、clang-tidy、警告error、5変更C/headerのformat検査を維持した。
製品の確保経路は変えておらず、今回は追加の確保失敗注入を行っていない。最終CIの既存計装を維持する。

## 実行原本と失敗

全対象実行はtools/record-run.py。原本D:/NeNeFolio/agents/635-drawer-hit-lookup/、
補助出力はD:/NeNeFolio/outputs/635-drawer-hit-lookup/。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

| run | 結果 |
| --- | --- |
| configure-release-01 / build-core-01 | 正典Release、83actions成功 |
| fixtures-01 / reference-01 / compile-baseline-01 | 正常一覧生成・基点固定・observer構築成功 |
| measure-profile-01 | 基点の20条件、成功 |
| compile-candidate-01 / measure-candidate-01 | 40条件/800sample、成功 |
| audit-01 | 補助scriptが未使用も含む全公開symbolをMAPへ要求して停止。実際に使用する7symbolの明示集合へ訂正 |
| audit-02 | 全入力hash/raw中央値/cycles/MAPを照合。整形前後の空白除外一致を保存し、計測を再利用 |
| configure-debug-01 | 正典Debug設定、成功 |
| build-debug-01 | 追加testのnesting4を上限3で拒否。未生成exeのartifact記録error1も保持 |
| build-debug-02 | 期待区間の照合をhelperに分け、残り24actions成功。規則/除外/閾値は不変 |
| drawer-layout-01 | folio_tests.exe --drawer-layout成功、stderr空、入力終了時不変 |

初期の補助読取でdrawer_windowとADR/waiver/旧補助scriptの推測pathが存在せず、一覧で実所在へ訂正した。
製品試験の失敗とは分ける。audit初回は未生成2artifactの記録errorも残した。失敗を上書きしない。
文書参照/差分/conformance/自己レビュー、最終head必須CI、統合、通常反映/data不変、全保全/専用領域整理は
同taskのintegration-completion.jsonで確定する。branch/commitは保持する。

## 限界と継続範囲

反復した合成入力の関数費用であり、OS idle・周波数・冷cache・UIイベント配送・実一覧の構築費用を保証しない。
列挙/描画/mark/reveal/dropなど他の行数依存の処理は残る。通常app・実ノート・Storeは試験に使っていない。

hideは12時頃、安全判定の通知を共有した後、通常の品質対応を安全に続けるよう指定した。
#634のJSON変異試験は生成とobserver構築途中で保留し、未実行の比較を成功扱いしない。
同試験の再実行・言い換え・別モデルによる判定回避は行わず、今回の通常一覧の確認とは分けて記録する。

## 統合・通常反映・保全整理の確定

PR636最終headc04338adのCI41961125755 full-gate/check成功、main9a84bebへ正典統合、Issue635 CLOSED。
通常build/check成功、data32fileのbytes/hash/作成更新日時不変、通常app起動無し。
1457fileをZIPへ全保全・hash照合し、取込tree/dirty/ignored/リンク/稼働参照を確認後、専用worktree/outputのみ削除。
branch/commit保持。原本D:/NeNeFolio/agents/635-drawer-hit-lookup/integration-completion.json。
後続#637の[配置準備の計測](2026-11-05-drawer-preparation.md)では、hitの前後にあるcreate/mark/scroll/destroyを区別した。
単独hitの速度倍率を、UI全体や毎回の配置準備を含む速度倍率として扱わない。
