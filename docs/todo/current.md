# いまのタスク — NeNe Folio

記録日: 2026-10-13。#298の着手main `1a9739862fbed90b95d26faa6e373ce4e86c2227`、公開tag `v0.1.0`。
これは状態の要約で、作業許可の正本ではない。hideの現在指示、checkout、Issue/PRの実状態を照合する。

## 現在の到達点

- [Issue #298](https://github.com/hideyukiMORI/nene-folio/issues/298): 無題から別ノート・次の新規・通常終了へ進むときに、
  保存／保存しない／キャンセルを選べる実装。[ADR0050](../adr/0050-unsaved-note-departure.md)を実装前に記録した。
  明示保存・本文Esc・保存して閲覧は保存専用とし、移動先を準備するまで作りかけを保持する。
- 枝は`feat/298-unsaved-note-choice`、作業木は`D:/NeNeFolio/worktrees/298-unsaved-note-choice`。
  対象unit・確保失敗注入・隔離Win32確認の結果は当日日報に記録。最終CI・統合・通常exe反映・整理の実結果は、紐付くPRと
  `D:/NeNeFolio/agents/unsaved-note-choice-20261013/completion.json`を確認する。
- hideが今回明示した分担に従い、Astraが設計・判断・文書・受理・統合、SOLが実装・対象検証を担当する。
  別のSOL席は必要な独立レビューだけを読み取りで行う。他案件・次の依頼の既定にはしない。

## 確認済みの基点と次回の入口

- 再開時は#298と紐付くPRの実状態を先に確認する。MERGEDかつ`completion.json`で通常exe反映・整理済みなら、
  今回の枝を再実装せず新しいhide指示から始める。公開Releaseの更新は今回の依頼に含めない。
- #294 / PR #295の125テーマ追加、#296 / PR #297の日報整備は、必須CI・main統合・必要反映・作業木整理まで完了。
  製品基点`1fe120c`。#298着手時の通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`で、
  SHA256 `41FAFE1143D7AFC31D7BE2786AA701AC3C64427E6140A89A6D26893C6E548C56`が前回記録と一致した。
- 経緯は[当日日報](../reports/2026-10-13.md)、再開の詳細は[引き継ぎ](../handoffs/2026-10-13.md)。
  操作は[キー表](../KEY_BINDINGS.md)、機能は[README](../../README.md)、要件は[SPECIFICATION](../../SPECIFICATION.md)。
- 公開済み[v0.1.0ベータ](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)の基点は`9e93ae1`。
  後続の設定/カテゴリ/表示/テーマ更新と#298は公開版へ含めていない。
- 表示切替の「両方」の文言と2トグル案は相談のみ。今回の#298へ混ぜない。
- 全125配色の個別目視、物理IME、別OS/DPI、スクリーンリーダー、#294のhide実機受理は未測定。
  #292の未測定区分も維持する。今回の確認で受理済みに読み替えない。
- #243はcodepage65001なしでU+FFFDと隣接VTを保てる新しい根拠が再開条件。
  旧C側41作業木と`D:/NeNeFolio/worktrees/210-rename-state`の整理は従来のhide判断待ち。

成功済み対象検証は関係入力が不変なら再利用し、最終headの必須CIは維持する（QLT-012 / ADR0044）。
通常exeを反映するときは現在の起動状態を確認し、未保存内容を保護する。Waivers: none。
