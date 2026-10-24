# いまのタスク — NeNe Folio

記録日: 2026-10-24。#351は[PR #363](https://github.com/hideyukiMORI/nene-folio/pull/363)でmain `1567439`へ統合済み。
その後のhideの実機報告を受け、テーマ候補の表示回帰 [#364](https://github.com/hideyukiMORI/nene-folio/issues/364) を修正した。
再開時は現在のhide指示・PR・checkoutを照合し、同日の古い途中記録を新しい停止/再開指示として扱わない。

## 最新の修正

- テーマ一覧は内部で候補が動いても、検索EDITにfocusがあると枠が消えていた。候補カーソルを常に示し、採用印・native/UIA focusと区別する。
- `fix/364-theme-cursor`の対象検証は140 checks/0 failures（同じfixtureの修正前は24 failures）。詳細は[品質記録](../quality/2026-10-24-theme-cursor.md)。
- CI・統合・通常試遊への反映・起動・整理は `D:/NeNeFolio/agents/theme-navigation-20261024/completion.json` と#364のPR Checksを正本とする。
- 対話席が単体で修正/検証した。前の#351の委任を新しい担当固定にはしていない。

## 現在の成果

- 名前入力/失敗通知、設定/テーマ、Help/履歴、検索/置換帯をADR0060の面の規則へ統一。最小窓の本文、失敗理由の改行、native面終了後のfocus、Helpの描画費用も是正。
- 詳細な入力・コマンド・結果・制限は[対象検証記録](../quality/2026-10-24-surface-rules.md)。履歴は[日報](../reports/2026-10-24.md)、再開条件は[引き継ぎ](../handoffs/2026-10-24.md)の同日後半の節を参照。
- #351の枝 `feat/351-surface-rules`とcommitを保持し、追加作業木は削除済み。既成功の対象検証は関係入力が不変なら再利用する。
- 通常試遊の正規更新は `tools/prepare-real-machine.ps1`。CI/統合後のビルド・未公開配布候補・実exe確認・整理の実行結果は `D:/NeNeFolio/agents/351-surface-resume/completion.json` とPR Checksを照合する。
  稼働中アプリや未保存本文を終了しない。公開Releaseの作成は今回の依頼に含めていない。

## 維持する判断

- 全ノート検索EDIT Nameへの追加providerは不要。fresh native UIAをmanaged UIAと同じprocessへ混ぜない。
- 検索はliteral、置換は編集中のregex、CtrlHは本文caretの既存意味。theme Escは設定へ1段、×は設定全体を閉じる。native DialogBoxとchild面のAltF4の意味を混同しない。
- native箱終了後のfocusはwrapperのlocal値から既存復帰処理へ合流する。UIAの設定/保存Invoke後のroot復帰は、同じ入口の無失敗baselineと比較する。
- schema・実行時依存・gate閾値/除外は不変。音分類表だけを既存CNF-009へ追加登録。Waivers: none。

## 保持物と未測定

追加作業木/不要な一時物は、取込状態・未保存/無視ファイル・唯一の成果物・稼働参照・リンクを確認して整理し、枝とcommitを保持する。
その結果は上記completion.jsonを参照する。恒久 `agents/`・確認用 `check/`・`release-candidates/` は保持する。

前の作業で自動承認レビューに再帰削除を拒否された6箇所は、今回も別手段で削除しない:
`D:/NeNeFolio/outputs/350-menu-surface/`、`350-release-20261024/`、`351-native-name/`、`351-dialog-feasibility/`（後3つも同じoutputs配下）。
さらに `D:/NeNeFolio/outputs/351-surface-rules/`、`351-release-20261024/`（後者も同じoutputs配下）。
旧#336/#338/#340/#342/#348/#349の一時物は今回対象外。旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。

物理IME・実reader・物理DPIをまたぐ移動・DWM窓外影は未測定。10,000件一覧の性能は本件では改善していない。
今回のAstra設計・SOL/LUNA実装/調査/検証・ClaudeCode `/design`との議論はhideが現在の作業で明示した分担であり、他案件や次の依頼へ固定して引き継がない。
