# 品質の現在地 — 2026-10-29

文書Issue: [#436](https://github.com/hideyukiMORI/nene-folio/issues/436)。規則: GIT-001〜004、QLT-008/012/013、ADR0043/0044。
16:41までの統合確定値と、直後の限定静読を整理する。新たな製品修正・仕様・schema・waiverは無い。Waivers: none。

## 最新の統合と通常反映

スクロールの加算前飽和 #434 / [PR #435](https://github.com/hideyukiMORI/nene-folio/pull/435)は、
finalhead `64a8b78ea77579315287972ebfde3205e8f99461`のCI `40836070731`で必須full-gate/checkが成功した。
正規`tools/merge-pr.ps1`でmain `875d058cf9c875551ff332d85caef803e1f4b942`へ統合、Issue CLOSED/main同期を確認した。
CIの`pwsh -NoProfile -File ./eng/check.ps1`は4754/5240分岐、規約検査0違反、検査器112件成功。
旧UBSan、完成Debug/tidyと共用scroll selector、最終staticは[対象品質記録](2026-10-29-drawer-scroll-delta.md)を参照する。

通常exeは正規`tools/prepare-real-machine.ps1`を記録wrapper経由で反映し、子/wrapper0・記録エラー0だった。
350指定入力と生成exeの現物/snapshot全hashが一致した。更新16:39:26.661 JST、5,476,864bytes、版0.3.0.0。
SHA256: `70B301D2E636D11C21740D4DBF821A56FCD117AB8D434FBA487B467C7244165D`。
反映時の通常process0件、更新後未起動。通常dataは操作していない。

全2589files/118,730,484bytesを恒久archiveへ保存し全hash照合した。
取込tree一致、未保存/未追跡/ignored/リンク/稼働参照無しを確認してworktreeを削除し、branch/commitを保持した。
output削除のみ自動承認レビューが実行前に`blocked by policy`で拒否した。追加理由はなく、再試行しない。
全確定原本は`D:/NeNeFolio/agents/434-drawer-scroll-delta/integration-completion.json`と同directoryの各記録。

## 限定静読の結果

| 対象 | 読み取れた範囲 | 今回行っていないこと |
| --- | --- | --- |
| 本文入力・保存 | UTF-8検証、埋め込みNUL拒否、署名一個の消費と本文先頭U+FEFFの保存保護、一時fileからのpublish | 新たな実ファイル障害注入、durability保証、通常data操作 |
| 表示位置・本文検索 | RichEditのCR内部表現を前提とするline_index、順逆巡回、ASCII大小無視の既存契約 | 新unit/native/IME、Unicode全域casefold、性能計測 |
| 全ノート検索pending | 保存/同長変更、copy失敗、改名途中、削除、新規公開の進捗無効化とrequest保持、借用解放と完成所有 | 新fixture/build、実データ再現、全経路の安全性証明 |

親が本文入力等を静読し、SOLが検索mutationの独立読取レビューを担当した。
双方とも、今回の限定範囲で新しい具体的不具合候補は0件。既存unitを読むことを今回の成功試験として数えない。
検索レビューの10入力hashを親が照合し、報告を受理した。pending中のカテゴリ作成/並替は既存FILTERED拒否の対象で、
その操作を許可した反例を新たに仮定していない。name/bodyはstep呼出し中だけの借用、termはbuild破棄/SEALEDまでの借用という既存契約を照合した。

原本は`D:/NeNeFolio/agents/quality-20261029/`の`post-434-input-audit.md`、
`post-bounded-mutation-review.md`、`post-bounded-parent-acceptance.json`。入力hashと未実行範囲を各原本へ残す。
親の静読開始mainは0803ff5、hash採取前に434が875d058へ統合された。差分は受理済みscroll箇所で、静読した検索/置換/設定部は不変。
存在しない推測pathとWindowsで展開されないrg wildcardの読取失敗は、実一覧・`-g`指定へ訂正し、製品試験失敗と区別した。

## 計測と残る範囲

16:34台の一度のprocess inventoryでも他案件compile/tidyが稼働していたため、新しい性能計時は0件。
`search-load-preflight-1634.json`に照会を残し、連続load pollはしていない。
#428でmatcherが変わったため、旧memchr候補の計測値を現在版の改善率へ読み替えない。memchr候補は未採用。
idleの65536比較workは実時間上限ではなく、語の準備・集計/確保・lookup・OS read・先行修復・reveal/layoutの同期処理は残る。
背景I/Oと極狭幅表の可読性は未解消。保存先設定・Store公開は今回の品質文書反映へ含めない。

## 文書反映の検証と保全

#436はcurrent・日報・引き継ぎと本記録だけを更新する。localの製品build/unit/native/性能試験は追加しない。
文書差分・参照先と原本の整合を確認し、最終headの必須CI成功を統合条件とする。実結果は同taskの記録と実PRを照合する。
製品無変更なので通常exeの再buildは不要で、上記hashの保持を確認する。

新設した空の`D:/NeNeFolio/outputs/436-quality-checkpoint`は不要となり非再帰削除を要求したが、
自動承認レビューが実行前に`blocked by policy`で拒否した。追加理由なし。空のまま保持し再試行しない。
検証記録は同taskの恒久agentsへ直接保存する。旧39pathと計40pathの保持一覧は[current](../todo/current.md)を参照する。
追加worktreeは統合・成果物保全・安全監査後に整理し、branch/commitを保持する。
