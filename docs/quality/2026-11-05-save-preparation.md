# 表示準備の失敗で保存履歴を消費しない — #617

名前付きノートの保存では、表示用RTFを準備してから履歴と本文を書き込むよう順序を直した。
修正前は7番目の確保を失敗させると、本文書込み0回のまま履歴が1回回転することを単体境界で再現した。
修正後は同じ短い本文の16確保点すべてで、履歴・本文書込み0回と状態保持、解除後の保存を確認した。

## 根拠と変更

基点はmain `d8d7974d1bca853ee81d3921baf637589ed083ec`。
独立読取レビューが`store_edited`のarchive→RTF準備という順序を指摘し、親が実装とADR0006/0012を照合した。
実adapterの履歴は5世代で最古を削除するため、純粋な表示準備の失敗で回転を繰り返すと古い版を消費し得る。
今回の動的再現は既存fake portの呼出しであり、実ファイルの5世代を削除する試験は行っていない。

`src/application/folio_state.c`の既存`store_edited`内で、`markdown_rtf_create`を`archive_before_store`より前へ置く。
archive拒否時は準備済みRTFも破棄する。成功時のarchive→write、同じ本文では双方0回、失敗結果の型は維持する。
新しい保存経路・公開API・schema・依存・thread・品質ゲート設定は加えない。

規則: ARC-001/004/010、C-004/005/008/012/016、QLT-008/009/010/012/013、GIT-001〜004。
判断は[ADR0006](../adr/0006-edit-mode-and-save-path.md)、[ADR0012](../adr/0012-note-history-before-save.md)、
[ADR0044](../adr/0044-tests-at-completion-and-integration.md)に従う。追加waiver none、既存WVR-0001は変更なし。

## 再現と対象検証

`tests/unit/state_tests.c`の既存fake/`edited_state`とallocation_probeを使う。
保存済み`# Hello\r\n\r\nbody`から`changed`への保存を対象に、確保を一つずつ失敗させる。
失敗後に同じ注入で`end_edit`も拒否させ、本文・閲覧RTF・編集mode・選択先B/two・保存世代の保持を確認する。
注入解除後は同じ`end_edit`でarchive/write各1回、本文採用と閲覧modeへの移行を確認する。
16点を拒否し17番目の試行で正常保存した。試行番号はAPIの契約や製品定数にはしていない。

既存の保存ガード、同一本文、変更問い合わせ、未保存判定、履歴順、履歴拒否/ABSENT、通常保存、write拒否の8fixtureを
`run_save_state_tests`へまとめ、全体suiteと狭い`--save-state`が同じ関数を呼ぶ。
`--save-preparation-oom`は注入が実際に効かなければ拒否する。製品へのテスト用入口は追加しない。

| run | 実行と結果 |
| --- | --- |
| configure-01 | 固定toolchain、正典Debug configure。子/記録0、記録error0 |
| build-red-01 | 既存coverage.pyのbuild_measurementを使用。85製品TU/31test TUを同Debug sanitizer vectorで計装。子/記録0 |
| red-01 | 旧製品＋回帰test、`folio_coverage.exe --save-preparation-oom`。7番目でOOM・archives1/writes0、期待するassertionで子/記録1 |
| build-green-01 | 修正後の同じ測定build。子/記録0、記録error0 |
| green-01 | 同selector。16確保失敗・各反復拒否・解除後retry成功、子/記録0、記録error0 |
| build-debug-01 | 正典Debug `cmake --build build --target folio_tests --parallel 2`と4変更fileのformat検査。警告error/tidyを維持、子/記録0 |
| save-state-01 | Debug `folio_tests.exe --save-state`。既存8fixture成功、stderr空、子/記録0、記録error0 |

conformanceと差分/参照確認の結果も同task原本に記録する。
ローカルで無関係な全件suite/フルゲートは反復せず、最終PR headの必須CIで`pwsh -NoProfile -File ./eng/check.ps1`を要求する。
対象検証成功をCI/統合完了とは扱わない。入力が不変の対象結果はpush/review/mergeで再利用する。

## 証跡と限界

全runは`tools/record-run.py`で入力snapshot/hash・raw stdout/stderr・成果物・実終了値を保存した。
原本は`D:/NeNeFolio/agents/617-save-preparation/`、一時出力は同名`outputs/`、作業木は同名`worktrees/`。
`measurement-*-commands.json`は既存測定builderのcompile/link呼出しと出力、nullability proofは各測定directoryにある。
`independent-review.md`に委任範囲と親の受理を残した。通常app・実ノート・Storeは操作していない。

測定exeはASanの`interception_win: unhandled instruction`をstderrへ出した。既存測定経路の通知を保持し、
完全なinterceptionやメモリ不具合の不存在を主張しない。回帰の判定は実注入・公開結果・port副作用・状態保持のassertionで行う。
実write失敗や履歴回転の途中失敗における非原子性はADR0012の既知限界のまま。全Markdown入力の確保点を走査したとは扱わない。

最終head CI・正典統合・通常版反映・全成果物保全/安全整理の確定原本は同taskの`integration-completion.json`。
追加worktreeと不要出力は取込状態・未保存/ignored・唯一の成果物・稼働参照・リンクを確認してから整理し、branch/commitを保持する。
