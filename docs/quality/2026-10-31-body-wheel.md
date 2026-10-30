# 本文ホイールを短く減速して止める — #473

- Issue: [#473](https://github.com/hideyukiMORI/nene-folio/issues/473)、base main `40dc7c4`。
- 規則: ARC-001/002/004/005/007/008/011/012、C-002/008/012/013/016/017、QLT-008/012/013、GIT-001〜004。
- 方針: [ADR0026の#473補正](../adr/0026-line-numbers.md)、[ADR0067](../adr/0067-note-display-zoom.md)。
- 原本: `D:/NeNeFolio/agents/body-wheel-20261031/`、作業木/出力はD側の`473-body-wheel`。

## 根拠と変更

hideの実機報告は、編集・閲覧でホイールを速く回すと、入力を止めても上端/下端まで長く移動すること。
希望は徐々に減速する操作感。単発入力を調べた#392の確認では連続入力後の残りを捉えていなかった。
専用desktopのnative診断で、60入力後にも編集/readonlyの両方で約4.6秒移動し続けることを観測した。

製品変更は`src/ui/win32/note_pane.c`だけ。通常wheelを同じ本文の短い減速処理へ集約し、
nativeへのwheel配送とその再入guardを削除した。Ctrl+wheelは既存の倍率変更を維持する。
OSの行数設定とnative表示行の位置から画素量を導き、小deltaを比例配分する。
未消化量は可視高1画面以内、最後の入力から240msの二次減速、16ms間隔とする。
逆転・クリック・鍵・本文/倍率/配置変更・無効化・破棄で残りを捨てる。

`note_pane`がmessage-only窓とtimer、未消化量、画素端数、残り期間を所有する。
時計を直接読まずTIMERPROCのtickを使う。RichEditの非公開timerとは別窓とし、UIスレッドで通常dispatchする。
実位置・本文・選択はRichEditのまま。`EM_SETSCROLLPOS`後に既存の行番号通知を使う。
状態所有の文書とADRを実装前に補正した。公開API、schema、版、依存、gateの変更なし。Waivers none。

## 対象確認

全stageは`tools/record-run.py`で入力snapshot・raw出力・成果物・終了値を保存した。
正典Debugのcompile_commands、Ninja、File API、tool/runtime hashと環境からfixtureを派生する。
ASan/UBSan/nullabilityとrecover禁止、4つのruntime、既存の警告を保持し、console entryのみ検証用に変更する。
保存した製品`folio_window.c`全bytesの後ろにfixtureを追加し、元window archive memberがlinkされないことをmapで確認した。
`note_pane.c`は保存した正典libraryの実装をそのまま使用する。合成ノートだけを専用dataへ置く。

| 実行（driverは原本directory） | 結果 |
| --- | --- |
| `record-debug-v1.ps1 -Stage Configure -Attempt configure-debug-01` | 正典Debug configure、child/wrapper0、記録error0 |
| 同driver `-Stage Build -Attempt build-baseline-01` | 修正前Debug/tidy、0/error0 |
| 同driver `-Stage Build -Attempt build-candidate-02` | 修正後Debug/tidy、3actions、0/error0 |
| `product-driver.py prepare baseline-v2 --build build-baseline-01` / `candidate-v2 --build build-candidate-02` | それぞれcompile/link成功、0/error0 |
| `product-run.py baseline-v2` | 90checks/20fail、native/child/wrapper1、記録error0 |
| `product-run.py candidate-v2` | 同じfixtureの90checks/0fail、0/error0 |
| `product-driver.py prepare boundary-v2 --build build-candidate-02` と `product-run.py boundary-v2 seed-v2 boundaries` | 追加境界24checks/0fail、0/error0 |

60回・約8ms間隔・上下の同条件で比較した。修正前後のfixture bytesは同一。

| 本文・方向 | 入力後の最終移動まで（修正前→後） | 入力後の移動画素（修正前→後） |
| --- | --- | --- |
| 編集・下 | 4812→234ms | 10348→585px |
| 編集・上 | 4688→250ms | 10076→585px |
| 閲覧・下 | 4782→235ms | 12478→585px |
| 閲覧・上 | 4750→235ms | 12464→585px |

可視高585px。候補では8回の途中位置を観測し、終盤の移動量が序盤より小さくなる。
試験の停止判定は配信遅延込み450ms、最低180msの安定を観測する。上記は一回の観測値で性能保証ではない。
20failは契約判定の件数であり、独立した20欠陥とは数えない。

単発と4×30deltaの同等性、逆転、両端、クリック/鍵/Ctrl+wheel、無効化/再有効化、非表示/再表示、
本文切替、本文・選択・dirty・focus・倍率保持、実Redo→Undo、nativeバー非表示と幅0、
最後の行番号paintと実viewportの一致を確認した。
追加境界はnative表2文書、50/300%倍率、最大signed delta、resize、モード切替、本文外入力、余韻中の破棄。
OS行数設定の変更や失敗注入は行っていない。

製品試験は65535pxを超える位置から始め、倍率300%では1317524pxからの移動を確認した。
native座標診断でも200000pxまでの7px相対移動を確認したが、公式資料の16bit記載との違いを全OSへ一般化しない。
環境: Windows `10.0.26300.0`、Msftedit.dll `10.0.26100.9949`、専用非入力desktop、1200×800窓。
物理wheel、実reader、DWM呈示は未確認。通常data、共有cursor、実機の未保存本文は操作していない。

失敗試行も保持する。candidate build01は関数複雑度を検出し、対象行選択を名前付き関数へ分けてbuild02で成功した。
baseline compile-v1はdriverの型取り違え、candidate compile-v1は環境照合不一致、boundary compile-v1はfixtureのinclude不足。
それぞれ別の新規run-dirで修正し、旧ログと未生成artifactの記録errorを成功扱いしていない。
成功済み対象検証は製品入力不変のまま再利用する。

## 自己レビュー・統合

ARC-001/004/008/011: 通常wheelの旧native経路を除き、短命UI状態と実位置の所有を分けた。
ARC-007、C-013: 時計API・thread・入れ子pumpを製品へ追加しない。
C-002/012/016/017: 開いたWin32 messageだけdefaultを使い、距離を可視高とLONG範囲に収め、
timer窓のuserdata切離しと破棄、別入力による取消、実本文保持を確認した。
QLT-008/012/013: 同一fixtureのred/greenと直接境界を用い、単発試験を連続入力の証明にしない。

最終文書参照/format/diff/conformanceと最終head必須CIを確認する。
通常版は00:52時点でPID42532が起動中。保存・終了の確認までは反映せず、プロセスを終了しない。
main統合、通常版の反映/起動、全記録保全と整理の確定値は原本の`integration-completion.json`へ残す。
旧削除拒否54箇所、未完#466、停止中の継続goal、通常data、公開betaは変更対象外。
追加worktree/出力は取込・保全・未保存/未追跡/ignored・link・稼働参照を監査して整理し、branch/commitは保持する。
