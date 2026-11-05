# 編集破棄とnative Undo/Redo — #702

2026-11-05 23:17頃、hide「お風呂に入ってくるので止めるまで進めて。」により品質対応を再開した。
主担当単体で調査・実装・検証し、追加委任なし。基点main `e1df61c0b14c073b95e20d08c00726dbf3d8555a`。
規則: ARC-001/004/005/011/012、C-008/016/017、QLT-008/010/012/013、CNF-006、GIT-001〜004、ADR0016/0043/0044。
追加waiver none、既存WVR-0001継続。

## 根拠と変更

[ADR0016補正4](../adr/0016-command-catalog-and-shared-execution.md)は、編集破棄で最後に読んだか保存したメモリ本文へ戻し、
保存・ディスク再読込みをせず編集モードを保つと定める。application単体では戻す本文を確認しているが、
実RichEditのUndo/Redoと保存履歴の組合せは恒久nativeテストで固定されていなかった。
とくにUndo後は本文が保存済みと一致していても、Redoで戻せる未保存編集が残り得る。

`tests/win32/image_status_tests.c`に専用`--discard`を加え、同じfixture・公開操作メニュー・実RichEditを使う。
`CMakeLists.txt`は既存targetの`discard_native`登録、30秒timeoutと同じ`image_status_data` lockの2行だけを追加する。
製品C/API/保存schema/依存/閾値/抑制は不変。別のUndo実装や製品公開窓口を作らない。

1. 保存済みの日本語・絵文字・Markdown本文と全5履歴slotのsnapshotを保持する。
2. 追記してCHANGED・Undo可能の状態から、公開の編集破棄コマンドを実行する。
3. 保存済み全文/SAME・編集可能・本文focusを確認し、Undo/Redo可否と実操作の両方で破棄済み編集が戻らないことを確認する。
4. 新しく追記してUndoし、本文SAMEかつRedo可能になった状態でも編集破棄して同じ契約を確認する。
5. 破棄後の新規追記→Undo→Redoが各全文/dirty状態を正しく戻すことと、再度の破棄を確認する。
6. 各境界で保存本文・全履歴の存在/全bytes・store_revision不変を照合する。画像要求は0。

## 対象検証

専用Debugの`image_status_tests --discard`は **127 checks / 0.691320秒**、終了0、stderr空。
既存`eng/toolchain.ps1`とCMake/Ninja、clang-formatと警告error/静的解析を使って同targetをbuildした。
新しいCTest登録は`ctest --show-only=json-v1 -R '^discard_native$'`で、同exe/selector/lock/30秒を読取り照合する。
差分・参照・conformance/build graph、最終source/hashと既存テスト保持を確認し、最終head必須CIで同じ`eng/check.ps1`を要求する。

環境はローカルWindowsの専用desktop `NeNeFolioImageStatus520`、専用portable dataは初回0file。
`D:/NeNeFolio/outputs/702-discard-native/debug/verification-images/`だけにfixtureを作り、
公開UI actionとSendMessageで操作する。物理キー/IME・全DPI・障害復旧・無題の破棄は今回の対象ではない。
既存の単体や失敗系の成功を新たな実行として数えず、全件ローカル回帰・通常exe再buildを行わない。
通常app/実ノート/Store/Ledgerを試験へ使わない。[gate-proofs](gate-proofs.md)にも環境を収載する。

入力snapshot/raw/終了値/成果物はD:/NeNeFolio/agents/702-discard-native/runs/。
対象結果はcontract-01.json、規則別自己レビューはself-review.json、最終CI/統合/通常不変/全保全整理はintegration-completion.json。
初回build01はmainのselector追加で認知的複雑度11が上限10を超え、native未起動で停止した（子1・記録125、未生成exeのartifact error1）。
全selectorで繰り返していたargc判定を一度にまとめ、各選択の意味を保って訂正した。閾値/検査規則は不変、初回input/rawも保持した。
原本未完の段階を統合成功と扱わない。完了後は取込・未保存/untracked/ignored・唯一の成果物・link・稼働参照を監査し、
702専用worktree/outputだけを整理する。branch/commitと恒久証跡は保持する。

## Store報告と残る境界

23:17頃のhide「Pre-processing」は、直前の1.3.1提出の文脈の本人報告として記録した。
原本はD:/NeNeFolio/agents/release-1.3.1-20261105/store-status.json。画面/提出package hash/申請番号/正確な申請時刻は独立未確認。
認定・公開・Store署名版導入へ読み替えず、公開済みGitHub 1.3.1/source/tag/配布物を変更しない。
保留#466/#479/#634と旧削除拒否領域は維持し、今回の再開で解除しない。
