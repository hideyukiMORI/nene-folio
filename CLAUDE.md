# CLAUDE.md — NeNe Folio

Claude Code / AI エージェントがこのリポジトリで作業するための**中核ハンドブック**。
簡潔な英語版の入口は [AGENTS.md](AGENTS.md)。詳細の正本は `docs/` にあり、ここには複製しない。

---

## 0. まず読むもの（production コードに触れる前に必ず）

1. [docs/ARCHITECTURE_CONSTITUTION.md](docs/ARCHITECTURE_CONSTITUTION.md) — 憲章（ARC-NNN）
2. [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md) — モジュールと依存方向
3. [docs/CODING_RULES.md](docs/CODING_RULES.md) — C (C23, clang-cl) 規約（C-NNN）
4. [docs/QUALITY_GATES.md](docs/QUALITY_GATES.md) — **いま何が機械で守られているか**（QLT-NNN / CNF-NNN）
5. [docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) — 手順
6. [docs/COMMIT_CONVENTIONS.md](docs/COMMIT_CONVENTIONS.md) — Issue・ブランチ・コミット・PR（GIT-NNN）
7. [docs/GLOSSARY.md](docs/GLOSSARY.md) — 用語
8. 該当する ADR（`docs/adr/`）と有効な waiver（`docs/waivers/`）

---

## 1. このリポジトリの統治原則

> **一つのことを実現する方法を 1 つに固定し、そのことを人の記憶ではなく機械に守らせる。**

その帰結として、次の 3 つを常に守る。

1. **正典の経路を先に特定してから編集する。** 「ここで書いたほうが早いから」で第 2 の経路を作らない（ARC-001 / ARC-012）
2. **ゲートを弱めて通さない。** 検査が落ちたらコードを直す。閾値・除外・重大度を触るのは ADR 相当の判断（QLT-010）
3. **`planned` を `active` と書かない。** 未実装の強制を実装済みに見せるのは、この規約体系で唯一「壊す」行為（[ADR 0001](docs/adr/0001-strictness-is-mechanically-enforced.md)）

---

## 2. このプロジェクトで間違えやすい所

### 現在時刻を読む場所は 1 つしかない

現在時刻・乱数・既定ロケール・環境変数・ファイルを読んでよいのは **`src/adapters/win32`** だけである（ARC-007 / ARC-003）。
中核で必要なら、型のあるポートから注入する。**テストが実時刻を読むことも決定性の破壊である。**
検査はソースの名前ではなく **リンカのシンボル**で行う（`eng/symbols.py`）。`time()` は `_time64` として現れる。

### 網羅性検査を殺す分岐を書かない

閉じた選択肢の分岐に `default` / `else` / `_` を書かない。選択肢が増えたらコンパイルが落ちるのが正しい状態（C-002）。

### 期待される失敗は `NULL` や `-1` で表さない

検証エラー・見つからない・拒否・非互換は閉じた `enum` の結果型で返し、`[[nodiscard]]` を付ける（ARC-010 / C-005）。

### 中核の型はヘッダに不完全型だけを置く

`struct note_id;` と関数だけを公開し、メンバーは実装ファイルに閉じる（C-003 / C-007）。これが C で `{0}` と直接コピーを書けなくする唯一の手段。

### コンパイラは clang-cl だけ

`cl` は C23 を話さない（ADR 0003）。`eng/toolchain.ps1` が `CC=clang-cl` を固定する。VLA・`const` 剥がし・分岐漏れ・網羅済みの `default` はコンパイルが落ちる。

### 抑制は言語で封じられない

`#pragma clang diagnostic` は `-Werror` を黙らせる。だから CNF-003 が pragma を拒否し、行単位の抑制には waiver が要る（C-015）。

### 結果の `enum` は型ごとに別ファイル

CNF-002 は「1 ファイル 1 型定義」を字句で数える。`struct x;` の不完全型と関数は `x.h` に、`enum x_outcome` は `x_outcome.h` に置く。
実装ファイルの外部関数は `x_` 接頭辞。`nenefolio_` 接頭辞は要らない（`eng/symbols.py` はアーカイブ内と宣言済み依存で解決する・ADR 0004）。

### ポートの文脈は不完全型で受ける

`void *context` は C-006 が禁じる。application が `struct persistence_adapter;` を宣言し、adapters（とテストの偽物）が定義する。

### 確保失敗の経路は測定ビルドでだけ走る

`malloc` が失敗する分岐は正典のビルドでは到達しない。`eng/coverage.py` の測定ビルドが `-Dmalloc=folio_probe_malloc` で
中核の確保を `tests/unit/allocation_probe.c` へ向け、`allocation_tests.c` が全確保を 1 回ずつ失敗させる。
製品コードにテスト用の窓口を置かない（C-008）。閾値（90%）と除外は触らない（QLT-009 / QLT-010）。

### OS ライブラリは `nenefolio_system_link` でだけ足す

CMake の既定リンクライブラリは kernel32 だけに絞ってある。user32 / gdi32 を呼ぶ target は `eng/architecture.json` の
`platformLibraries` にあるものを明示的に結ぶ。無ければ configure が落ちる（ARC-002）。

---

## 3. 検証コマンド

```bash
pwsh -NoProfile -File ./eng/check.ps1          # 唯一の完了定義（ローカルと CI で同じ）
```

開発中は最も狭い検査を使ってよい。フルゲートは **PR を Draft → Ready にする直前**に必ず通す。

🔴 **`pwsh -NoProfile -File ./eng/check.ps1` が通っていないものを「できた」と報告しない。**
実行していないコマンドの結果を書かない。テストの失敗を隠さない。
テストが本当の欠陥を見つけたら、期待値ではなく production コードを直す。

---

## 4. 変更の進め方

[docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) が正本。要約すると:

Issue → 正典経路の特定 → ブランチ → （設計を変えるなら先に ADR）→ 最小の実装 →
テスト → 狭い検査 → `pwsh -NoProfile -File ./eng/check.ps1` → 規則 ID ごとの自己レビュー → PR（draft）→ Ready → squash merge。

コミットは Conventional Commits（`type` と `scope` は英語、説明は日本語、末尾に `(#N)`）。形の正本は GIT-003。

---

## 5. 完了報告の形

作業を終えたら必ず次を報告する。

```text
Issue / 規則 ID:
変更したファイルと振る舞い:
実行した検証コマンドと結果:
ドキュメント・スキーマの変更:
Waivers: none | WVR-NNNN
残るリスク:
```

調査だけを頼まれたときは、編集・コミット・push・PR 作成・外部状態の変更を行わない。

---

## 6. いまの状況

2026-09-17 後半: hideの指示で日報/引き継ぎを保存して停止。mainは`b7038f6`（#55・#56・#58・#61統合済み）。
#40は`out/worktrees/40-filter`のDraft PR #63（HEAD `6c36ef9`・フルゲートexit 0）で、レビュー・Ready・統合は未実施。
最新の停止点は[引き継ぎ](docs/handoffs/2026-09-17.md)の先頭節と[日報](docs/reports/2026-09-17.md)の「後半」。明示的な再開指示まで続行しない。

2026-09-17: **全ノートの絞り込み（FR-032 / #40 / ADR 0024）**を `out/worktrees/40-filter` で3層に実装した。
ドロワーの頭に「すべてのノートを検索」の欄を常設し、Ctrl+Shift+Fでどの区画からでもそこへ移る。
打つと名前か本文が一致するノートだけが索引に残り、一致を持つカテゴリは折り畳んでいても展開して並ぶ。
**本文は起動時ではなく、空でない語が初めて来たときに1回だけ読む**（遅延読み込み）。以後は保存・新規・改名・移動で
その写しだけを入れ替え、読めないノートは写しを持たず一致しない。
判断はcoreの `index_filter`（ASCIIの英字だけ大小無視のUTF-8バイト比較・名前か本文に部分一致）、
写しはcoreの `note_corpus`（宛先はカテゴリ名とノート名なので並び替えでは動かない）、
語と一致集合はapplicationの `folio_state_set_index_filter`（UTF-16の入口の7本目）。
配置は同じ `drawer_layout` のままで、入力を `drawer_source`（台帳の列とnullableの絞り込み）へ束ねた。
**絞り込み中は並び替えと折畳／展開を `FOLIO_STATE_FILTERED` で断る**（色・閲覧・編集・保存・新規・改名は可）。
UIはその意図を送らず静かに無視する（カテゴリ行のクリック・`h` / `l`・ドラッグ。失敗箱は出さない）。
絞り込みの対象は保存済みの本文で、編集中の未保存の内容は一致しない。
現在の文書は一致しなくても閉じず、カーソルだけが最初に見える行へ移り、語を変えるたびにスクロール量が0に戻る。
Esc と Enter は索引へ戻して絞り込みを保ち、空にすると解ける。
単体とWindows部品48項目が成功（[確認記録](docs/quality/2026-09-17-filter-checks.md)。レビュー対応で33→48）。
1000ノート×10KBで最初の語305.4ms・以後の1打鍵51.0ms・全一致の配置1枚0.22ms。分岐網羅は92.85%。
最終フルゲート・CIは#40のPRを参照する。次は**原文の行番号（FR-021 / #39）**。

2026-09-17: #58はmain ab01654へ統合済み。#61は `out/worktrees/61-reparse`。改名の再解析ポイントの判定を
`GetFileInformationByHandleEx(FileAttributeTagInfo)` のタグへ変え、`IsReparseTagNameSurrogate`（シンボリックリンク・
junction）だけを拒むようにした（ADR 0022の決定4の補正）。同期フォルダのプレースホルダに置いた `data/` でも改名できる。
実アダプタprobeは17/17成功（[確認記録](docs/quality/2026-09-17-reparse-checks.md)）。次は**全ノート検索（FR-032 / #40）**。

2026-09-17: #56の名前変更はmain 69431cbへ統合済み。#58 / ADR 0023の現在ノートの検索を `out/worktrees/58-search` で
3層に実装した。GUI「このノート内を検索」・Ctrl+F・索引と閲覧本文の `/`（前方）・`?`（後方）がADR 0016の同じ入力面を開き、
元のフォーカスを覚えてEscで返す。Exと違いEnterでは閉じず、Enterは直前の方向・Shift+Enterは逆方向へ進む。
`n` / `N` は欄が閉じていても効き、編集中の本文と各入力欄では文字入力のまま。対象はRichEditが今表示している平文で、
段落区切りをCR 1つのまま取り出してEM_EXSETSELの位置と1対1にする。判断はcoreの `note_search`（前方／後方・巡回・
anchor・件数。ASCIIの英字だけ大小無視）、語と方向はapplication、選択はRichEditが持つ。
**INDEXの `?` はこの単位で後方検索へ移し、ヘルプはGUI「ヘルプ」・F1・`:h`・Ctrl+Pに残した。**
レビュー対応でADR 0023へ補正節を足した。前方はanchorの**開始の次**から見て重なる一致を飛ばさず、
反転したanchorは `NOTE_SEARCH_BAD_SPAN` で拒む。F3／Shift+F3は向きを名指しして進み、覚えている向きを変えない
（変えるのは `/` と `?` だけ）。壊れた語は欄の中の1行で、表示本文を取り出せない事象は `PANE_UNAVAILABLE` で区別する。
単体2/2とWindows部品30項目が成功（[確認記録](docs/quality/2026-09-17-search-checks.md)）。
最終フルゲート・CIは#58のPRを参照する。次は**全ノート検索（FR-032 / #40）**で、語も状態もノート内検索とは別に持つ。

2026-09-16: #55の別名保存はmain f35870dへ統合済み。#56 / ADR0022の名前変更を `out/worktrees/56-rename` で
5層に実装した。GUI「名前を変更」・F2・`:rename 名前`が同じ登録表を通り、mdと `data/.history` の履歴を新しい名前へ移す。
順は「編集中なら保存 → `data/.rename.json` を版1で新規公開 → 履歴 → md → `index.json` → 記録の削除」で、
移動は `SetFileInformationByHandle(FileRenameInfo, ReplaceIfExists=FALSE)`。再開は旧/新の存在と128bit識別子だけで
段階を決め、合わなければ記録を消さずに止める。同じ `data/` の二重起動は `data/.nenefolio.lock` で断り、
書けない `data/` は閲覧のために起動を許して各操作の失敗にする。未完了の改名があるあいだ、保存・切替・並替・色・新規・
別名保存・終了確認は先に同じ改名を再試行し、`:q!` でも意図は残って次回の起動が続ける。
記録を公開した後の失敗は `RENAME_PENDING` と `RENAME_HALTED` の 2 値で、どちらも意図を保持する。
単体・実アダプタ41項目・Windows部品19項目が成功（[確認記録](docs/quality/2026-09-16-rename-checks.md)）。
最終フルゲートとCIは#56のPRを参照する。次は**#58（現在ノートの検索・ADR 0023 草案あり）**。
`/` の全ノート検索（FR-011 / #40）はその後の別単位。
最新は [日報](docs/reports/2026-09-17.md) / [引き継ぎ](docs/handoffs/2026-09-17.md)。

2026-09-13 18:18 JST: hideの指示で日報/引き継ぎを保存して停止。#53はmain35d86afへ統合済み。
#55の別名保存は7708802で全体ゲート成功、PR57はReadyだがGitHub障害でCI開始未確認・未統合。
#56はout/worktrees/56-renameで設計/純粋中核のみ実装、2baa0d1で全体ゲート成功。実ファイル改名/GUIは未実装。
最新の停止点はこの作業枝の[引き継ぎ](docs/handoffs/2026-09-13.md)と[日報](docs/reports/2026-09-13.md)。

現在のタスクは [docs/todo/current.md](docs/todo/current.md)。GitHub Issue が正で、そこは要約。

2026-09-13: #37へINDEXの `?` とキーバインド説明を追加。パンくずのPowerline案は#45。
#37 / PR #44、#45 / PR #46、採用済みGUI計画#47 / PR #48は最終フルゲート・CI成功後にmain統合済み。
#49 / PR #50はmain b3a6b49へ統合済み。日本語の操作入口・共通ヘルプと閲覧からの編集開始を追加した。
#51 / PR #52は最終HEAD d07fcf6のフルゲート・CI成功後、main 7ff99c2へ統合済み。編集本文でCtrl＋hjklが使える。
#53 / PR #54は最終HEAD16a4353のフルゲート・CI成功後、main35d86afへ統合。無題・初回保存・名前入力を追加した。
#55 / ADR0021は `out/worktrees/55-save-as`。元の保存内容を保つ別名保存を実装し、単体とWindows部品測定が成功。
最終フルゲート・CIは#55のPRに記録する。改名・ノート内検索へ続け、`?`移行は実際の後方検索と同時に行う。
最新は [日報](docs/reports/2026-09-13.md) / [引き継ぎ](docs/handoffs/2026-09-13.md)。

2026-09-12: hide の追加要望を [コマンド・検索・設定の計画](docs/plans/2026-09-12-command-workspace.md)（Issue #36、統合済みPR #43）へ記録した。
コマンドの初版は [Draft PR #44](https://github.com/hideyukiMORI/nene-folio/pull/44) に実装。フルゲートは成功、Win32実機確認は未完了。
現状・確認条件は [日報](docs/reports/2026-09-12.md) と [引き継ぎ](docs/handoffs/2026-09-12.md)。画面案はhideの承認後にArtifact公開・Chrome確認済み。
#37の実機確認後、設定・Ubuntu 風テーマ・日本語／English／简体中文（#38）、
同梱 Google Fonts（#42）、原文行番号（#39）、`/` からの本文検索（#40）、現在 md の正規表現置換（#41）へ進む。
#38〜42は計画で、下記の 2026-09-11 時点の実装済み機能とは区別する。当時はモデル別の分担を指定していたが、現在はサナの単体実行が既定。
必要な独立レビューだけ読み取り専用で依頼し、ClaudeCodeのデザインリナとの採用済み相談は計画・デザイン文書に残す。

2026-09-11: Phase 3 の縦切り 11 本と窓の修正（Issue #3 / ADR 0004、#5、#7、#9 / ADR 0005、#11 / ADR 0006、#13 / ADR 0007、#15 / ADR 0008、#19 / ADR 0009、#21 / ADR 0010、#22 / ADR 0011 と 0014、#23 / ADR 0012、#24 / ADR 0013）。起動して `data/` の索引を枠なし窓のドロワーに描き、カテゴリ行のクリックでトグルして `expanded` を書き戻し、ノート行のクリックで右ペインに md を表示し、頭の「編集」の札で同じ RichEdit を平文の編集にして Ctrl+S と編集を抜ける操作で同じ md へ原子的に書き戻す。改行の形は元のまま・BOM は書かない・同じなら書かない。行をドラッグすると挿入線が出て、離すと表示順が変わり `categories.json` / `index.json` へ書き戻る（ドロップ先は core の `drawer_layout_drop` が決める・ADR 0007）。ノートは別のカテゴリへも移せて、md の rename が先に走り、移動先 → 移動元の順に両方の `index.json` が追随する（同名は拒み、台帳だけ書けなければ次回の起動の照合で揃う・ADR 0008）。索引が窓に収まらなければホイール（1 刻み = ノート行 3 つ）と ↑↓ / PgUp / PgDn でスクロールし、あふれている側の端に 24px のフェードが出る。スクロール量は application が要求量で持ち、上限と表示座標は core が決める（ADR 0009）。カテゴリ行の右クリックで OS の色の選択が出て、選んだ色を `categories.json` へ `expanded` と同じ経路で書き戻す（ADR 0010）。見た目はデザイン「案2 堅」（ADR 0005）で、OS のライト／ダークに従う。5 層すべてに正典の経路があり、ARC-002 / ARC-003 / ARC-007 / QLT-009 が active。枠なし窓の client は `WM_NCCALCSIZE` の TRUE / FALSE とも自分で答えて作成の瞬間から窓の矩形全体で、最小サイズは 560×360（96 DPI）、右ペインの頭のパンくずはノート名 → カテゴリ名の順に末尾を省略する（ADR 0011）。md を書き戻す直前の本文は `data/.history/<カテゴリ>/<ノート>/1.md`〜`5.md` に連番で残り、履歴を書けなければ保存しない（FR-017・ADR 0012。`.` で始まるディレクトリは走査しない）。白い帯の再発（非アクティブ化の `WM_NCACTIVATE` の既定処理が `WS_THICKFRAME` の縁を描く）は `WM_NCACTIVATE` → TRUE / `WM_NCPAINT` → 0 で根治した（ADR 0014）。フォーカスの区画は Win32 のフォーカスそのもの（主窓 = 索引・RichEdit = 本文）で、索引の `j` / `k` / `gg` / `G` / `h` / `l` / `Enter`、本文の `Esc` が効き、切り替えのたびに編集中なら保存してモードは保つ。カーソルはノート行と「見えるノート行を持たないカテゴリ行」（折り畳み・空）に止まり、カテゴリ行では右ペインを変えず `l` で中へ入る（ADR 0015）。Ctrl+S は両区画で効き、Escape で窓は閉じない（FR-018・ADR 0013）。検索・破棄（保存せずに抜ける）・改名・Undo・履歴を戻す UI・ドラッグ中の自動スクロールはまだ無い。次は FR-011（検索。検索窓が 3 つ目の区画になるので ADR 0013 の区画の扱いを先に決める）。仕様は [SPECIFICATION.md](SPECIFICATION.md)。
