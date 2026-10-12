# #288 — ドロワーのカテゴリ追加入口の確認

- 日付: 2026-10-12。基点main `3aaf37c`、枝`feat/288-category-entry`。
- 設計: [ADR0039補正8](../adr/0039-note-trash-and-category-operations.md)。FR-012/013/016/024/034、ARC-001/002/004/011、C-002/018、QLT-008/012/013。
- 証跡: `D:/NeNeFolio/agents/ui-category-entry-20261012/`。Waivers: none。

## 対象と結果

検索欄下の36px固定帯、3言語のカテゴリ見出しと＋、共通矩形の押下/解放、既存作成経路への接続を確認した。
source/testは6ファイル。名前検査・保存・filter guard・本文の採用経路・settings schema版3は変更していない。
rootが全差分を読み、band_heightとpaint/hit/scroll、capture解除の再入、既存run_itemへの合流、plus形状の共用を確認した。

| 確認 | 実コマンド/範囲 | 結果 |
| --- | --- | --- |
| 整形/Debug build | `pwsh -NoProfile -File D:/NeNeFolio/agents/ui-category-entry-20261012/check-candidate.ps1` | 変更6ファイルformat、File API query付き新規configure、`cmake --build D:/NeNeFolio/outputs/288-category-entry/candidate-20261012-1850/build --target nenefolio --verbose`がexit 0。clang-tidyを含む |
| 規約 | `python eng/conformance.py --root D:/NeNeFolio/worktrees/288-category-entry --build-dir D:/NeNeFolio/outputs/288-category-entry/candidate-20261012-1850/build --today 2026-10-12` | 0違反。`git diff --check`も成功 |
| GUI | `pwsh -NoProfile -File D:/NeNeFolio/agents/ui-category-entry-20261012/probe-category.ps1` | 最終実行41 assertions成功、`gui-results.json` |
| 入力 | `candidate-inputs.json` | 未commit候補のsource/tests/eng/CMake/font等のhash。commit/統合時に同じ入力を照合して結果を再利用する |

Windows build 26300、120 DPI（125%）、1000×750物理px。候補exe SHA256:
`B59D28DA3F1EC42B7CBBF1FABE678BA570AC23B0879BFA647FA83487D3240045`。
PID/HWNDを所有fixtureに限定し、前面windowは1577658のまま。正常終了と空stderrを確認した。
撮影は`tools/capture-window.ps1 -TargetPid <所有PID> -Title <対象の題> -Out <PNG>`のPrintWindow。

- 空一覧の見出しと＋を表示（`start-diagnostic.png`）。帯の余白、外でrelease、capturelostは作成に入らない。
- ＋で名前入力を開き、取消は0件を維持。First作成を完了し、カテゴリ行へ着地（`final-created-first-category.png`）。
- 固定帯追加後の最初の行hitとscroll後のNote03へのhitが一致。見出しと＋はscrollしても固定。
- 編集中のカテゴリ追加とfilter中の拒否で本文hash/長さ、modify、canUndo、selection、最初の論理可視行を保持。
  最後に実Undoで元のDIRTY挿入が消えることを確認した。

初回の空fixtureでは最初の撮影時に既存の「ノートを選んでから編集してください」の箱を検出し、中断した。
原因は特定していない。製品sourceを変えずに行った起動段階ごとの診断はdialog=0で前面も一定だった。
fresh fixtureで初期dialog=0を明示した最終実行は全件成功。初回ログ/画像は成功証跡と分けて保持する。
最終実行の`final-empty-category-entry.png`は初期描画途中の背景だけのため、空一覧の表示証拠には正常な`start-diagnostic.png`を使う。
作成後と操作後の画像もrootが視認した。

## 統合と範囲

最終headのフルゲートは必須CIへ集約する。成功済みの設定matrix、永続化回帰、local全件unitは再実行していない。
3言語の列網羅/文言表は既存gateへ委ね、今回の実OS操作は日本語/暗色/125%に限る。別OS/DPI・物理IME・hide目視受理とは呼ばない。
CI、main、通常exe、成果物保持、作業木と一時出力の整理は紐付くPRと`D:/NeNeFolio/agents/ui-category-entry-20261012/completion.json`で確認する。
