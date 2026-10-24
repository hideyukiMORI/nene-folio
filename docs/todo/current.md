# いまのタスク — NeNe Folio

記録日: 2026-10-24。#351はPR #363、候補枠の#364はPR #365でmainへ統合済み。
その後hideが求めた候補の即時プレビューを[#366](https://github.com/hideyukiMORI/nene-folio/issues/366)で実装した。
再開時は現在のhide指示・PR・checkoutを照合し、同日の古い途中記録を新しい停止/再開指示として扱わない。

## 最新の修正

- 候補へ移動すると画面全体を一時的にプレビューする。Enter/クリックで保存、Esc/戻る/×は直近確定へ戻す。
- 保存値は既存settingsだけが所有し、取消のRTF準備/native再着色失敗では面を閉じない。Ctrl+Sは本文だけを保存する。
- `feat/366-theme-preview`の対象unit/確保失敗測定/Debug/tidyが成功。nativeは158 checks/0 failures。
  詳細は[品質記録](../quality/2026-10-24-theme-preview.md)、判断は[ADR0061](../adr/0061-theme-live-preview.md)。
- CI・統合・通常試遊への反映・起動・整理は `D:/NeNeFolio/agents/theme-preview-20261024/completion.json` と#366のPR Checksを正本とする。
- 対話席が単体で設計/実装/検証し、必要な独立レビューだけ読み取り専用で実施。前件の委任を担当固定にしていない。

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

前の作業で自動承認レビューに再帰削除を拒否された8箇所は、今回も別手段で削除しない:
`D:/NeNeFolio/outputs/350-menu-surface/`、`350-release-20261024/`、`351-native-name/`、`351-dialog-feasibility/`（後3つも同じoutputs配下）。
さらに `D:/NeNeFolio/outputs/351-surface-rules/`、`351-release-20261024/`（後者も同じoutputs配下）。
さらに `D:/NeNeFolio/outputs/364-theme-cursor/`、`theme-navigation-20261024/`（後者も同じoutputs配下）。
旧#336/#338/#340/#342/#348/#349の一時物は今回対象外。旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。

物理IME・実reader・物理DPIをまたぐ移動・DWM窓外影は未測定。10,000件一覧の性能は本件では改善していない。
前の#351でのAstra設計・SOL/LUNA実装/調査/検証・ClaudeCode `/design`との議論は、その作業だけの明示分担である。#366以降へ固定して引き継がない。
