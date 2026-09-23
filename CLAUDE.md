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

2026-09-25 後半: **#120（`:e!`）・#119（音と印）・#118（名前入力面の一覧）を設計・統合した**。
main は **`49042a3`**（clean。#121=PR#124→`a2538ba`・#122=PR#125→`0463cbb`・#123=PR#126→`44cb483`・
#120=PR#127→`f523073`・#119=PR#128→`203cc49`・#118=PR#129→`49042a3`）。#118〜#123 はすべて閉じた。
#118 は現物調査で「操作 ▾」が OS の `TrackPopupMenu` と分かり、開閉するコンボをやめて**常時開いた owner-draw の一覧（4 行）**にした（ADR 0035 補正 17〜21）。
**開いている Issue は #112 / #70 / #42 とこの記録の #130 だけ。次は #70（一致の入れ物のメモリ）→ #42（同梱フォント）。**
判断待ち: #112・パレットで `:q!` が先頭に来る並び・82-9 と 100%/150%・OS のライト/ダーク切替の実機確認・简体中文の字面・
#119 の音の実機確認・#118 統合後の名前入力面の目視。
最新は[日報](docs/reports/2026-09-25.md)の「後半」/ [引き継ぎ](docs/handoffs/2026-09-25.md)。

2026-09-24 停止 2: hide の指示で停止した。**明示的な再開指示まで続行しない。** #114 / PR #115（「再開後」の記録）まで統合済みで main は **`b907b9b`**（clean）、背景席 0・未コミットの編集 0。
開いている Issue は **#70 / #42 / #112** とこの記録の #116 だけ。再開点は[引き継ぎ](docs/handoffs/2026-09-24.md)の「停止 2」（次は #70 → #42・probe と下ごしらえは Sonnet で試す・統合は `tools/merge-pr.ps1`）。

2026-09-24 再開後: hide の再開指示で、引き継ぎの「順番」を**同じセッションで全部**進めた。main は **`9813454`**（clean）。
(a) **運用 ADR 0034**（PR #108 → `1ae0b91`。手 1 席ごとのモデル・手 2 下ごしらえのスクリプト化・**手 3 Opus の実装席は 1 席 1 仕事で道具出力を小さく**・施主指示の型。
`docs/DEVELOPMENT_WORKFLOW.md` §10 と [指示書テンプレート](docs/IMPL_SEAT_BRIEF_TEMPLATE.md)から参照）。
(b) **`tools/` の 3 本**: #101 `prepare-real-machine.ps1`（PR #109 → `ad35359`）・#102 `merge-pr.ps1`（PR #110 → `ba1ea52`。差し戻し 2 回: merge 直後の Issue の状態は最長 30 秒待つ／skipped の check を完了と数えない）・
#103 `compare-screens.ps1` と `capture-window.ps1`（PR #111 → `e9dc421`）。**以後の統合は root の main で `tools/merge-pr.ps1 -Number N -Subject "…"`**。枝が BEHIND なら `chore(sync): main を取り込む (#N)` の件名で取り込んで push してから再走。
(c) **#82 / ADR 0035**（PR #113 → `9813454`。草案の 0034 は運用 ADR が番号を取った）: comctl32 v5 のまま新規 `dialog_theme` の 1 本（`WM_CTLCOLOR*` ＋ owner-draw の釦とコンボの項目 ＋ DWM の題の帯 ＋ `dialog_theme_frame`）で名前入力面と失敗の箱を塗り、
**失敗の箱は `MessageBoxW` をやめて自前のモーダル**（`failure_box_show` は `const struct folio_state *` の 3 引数。作れないときだけ `MessageBoxW` へ退避）。面のテーマは開く瞬間に固定。
受理前の probe で **owner-draw の釦は `WM_GETDLGCODE` が `DLGC_BUTTON` だけで Cancel にフォーカスして Enter → IDOK** と分かり、subclass（V4）は `BM_SETSTYLE` が型を書き換えて塗りが止まるので却下、
**面の `WM_COMMAND` で `GetFocus()` を見て読み替える V5** を採った。`ODS_DEFAULT` は立たないので既定は面が渡す。枠の色は `chip_background`（5.47 / 8.80:1）。
独立レビュー（止める 2・直したい 6）と production の `dialog_theme.c` を一緒にコンパイルした probe（PASS 93 / FAIL 2 = ClearType の丸め・対比 18 件すべて合格・最長 en の箱は 2 行）を経て統合。分岐網羅 93.26%。
**変わる振る舞い: 失敗の箱の警告アイコンと音が消える。描いた絵そのものと実機の面・箱は見ていない**（チェックリスト 82-1〜82-10・全 95 項目のうち結果は 2 行）。
この日の背景席は **14 席すべて手 3 の型**（最大 53 回の道具呼び出し・171K tokens・差し戻し 5 回はすべて新しい席。Sonnet / Haiku は 0）。
新しい Issue **#112**（Draft で skipped の必須 check を GitHub が CLEAN と見なす。hide の判断）。
**開いている Issue は #70 / #42 / #112 と記録の #114 だけ。** 次は **#70**（一致の入れ物のメモリ）→ **#42**（同梱フォント）。probe と下ごしらえは Sonnet で試す。
最新は[日報](docs/reports/2026-09-24.md)の「再開後」/ [引き継ぎ](docs/handoffs/2026-09-24.md)の「再開点 2」。

2026-09-24 運用変更（hide・Issue #100）: **次のセッションの最初に**、背景席のモデルを仕事で切り分ける運用（裁定・受理・仕様の文言 = 設計席自身／実装と差し戻し = Opus／
現物調査・棚卸し・記録・突き合わせ = Sonnet／照合・差分の列挙 = Haiku。判断基準は「間違えたとき誰が直すか」）と施主指示の型を運用 ADR にして配る。
繰り返しの下ごしらえは `tools/` のスクリプトにして設計席が直接実行する（chore #101 実機用 checkout・#102 PR の統合・#103 画素比較を発行済み）。
**手 3（同日の追加・#106）: Opus の実装席は 1 席 1 仕事**（probe・実装・差し戻し対応をそれぞれ新しい Agent にし、SendMessage の使い回しと fork は使わない）、
道具出力を小さく（対象テストだけ・失敗行だけ tail・ログはファイルへ落として grep・最終報告 30 行以内で親にはパスと数字だけ）、task は S 級に切る。
指示書の型は [docs/IMPL_SEAT_BRIEF_TEMPLATE.md](docs/IMPL_SEAT_BRIEF_TEMPLATE.md)。**次に立てる Opus の実装席から適用する。**
詳細は[引き継ぎ](docs/handoffs/2026-09-24.md)の「次のセッションの最初にやること」。

2026-09-24 停止: hide の指示で日報・引き継ぎを保存して停止した。**明示的な再開指示まで続行しない。**
main は **`4e57483`**（clean。#96 の docs = PR #97・CI 35740104937 成功）で、**開いている Issue は #82 / #70 / #42 と
この記録の #98 だけ**。作業木は 28 本ともすべて統合済みの枝である。**production コードはこの期間 1 行も書いていない。**
#96 のあと **#82（名前入力面と失敗の箱のテーマ追随）**の実測 probe と **ADR 0034 の草案・批評**まで進み、**受理せずに止めた**。
成果は `out/`（git の管理外）の 2 か所: 実測が `out/design/2026-09-24/prompt-theme-probe/`、
草案が `out/design/2026-09-24/adr-drafts/0034-dialog-theme.md`（**末尾の「批評への設計リナの判断」節が停止点**）。
実測: `WM_CTLCOLOR*` で地・STATIC・EDIT の中・**コンボの閉じた字の面**・ドロップダウンのリストは塗れる（15.06 / 17.78:1）が、
**EDIT の縁・コンボの矢印・選択の帯・スクロールバー・押し釦は塗れない**（`WM_CTLCOLORBTN` は来るが無視される。
押し釦は `BS_OWNERDRAW` なら塗れる）。**コンボは有効なら `WM_CTLCOLOREDIT`・`EnableWindow(FALSE)`（改名の経路）なら
`WM_CTLCOLORSTATIC`**。**comctl32 v6 は後退する**（`SetWindowTheme` は v5 で無効・v6 では色が uxtheme の `#333333` 固定で、
さらにコンボの閉じた面が塗れなくなる）。題の帯は `DWMWA_CAPTION_COLOR` で palette にできる。
**`MessageBoxW` は OS がダークでも白く色を渡せない**。自前なら折り返さず 805px・**幅 480 で最大 2 行**で、高さは `DT_CALCRECT`。
**13 か所（ドロワー 5・主窓 8）はすべて「知らせて戻る」だけ**でモーダルを必要としていない。
草案の骨: **v5 のまま**・塗りは新規 `dialog_theme` の 1 本・EDIT は `WS_BORDER` を外して面が枠を描く・
コンボは `CBS_OWNERDRAWFIXED | CBS_HASSTRINGS`・**失敗の箱は自前のモーダル**（`failure_box_show` は
`const struct folio_state *` の 3 引数へ）・**面のテーマは開く瞬間に固定**。
批評の 28 所見（止める 8・直したい 12・追補 8）はすべて受け入れたが**本文へ未反映**で、
**受理の前に S6（owner-draw の釦の既定釦と Tab → Enter）と S8（子窓 owner の無効化）の probe が要る**。
実機の目視は[統合チェックリスト](docs/quality/2026-09-22-visual-checklist.md)**85 項目のうち 40-3 / 40-5 の 2 行だけ**で、
残り 83 行は空欄（手順のビルド SHA はこの記録で「main の最新をビルドし直す」に直した）。
再開手順は (1) S6 / S8 の probe → (2) ADR 0034 を書き直して受理 → (3) #82 の実装（専任 agent）→ 独立レビュー → merge →
(4) #70 → #42。最新は[日報](docs/reports/2026-09-24.md) / [引き継ぎ](docs/handoffs/2026-09-24.md)。

2026-09-23 再開後 2: **hide が main `2448987` の実機をはじめて見た**（「おおむねいい」）。見つかった 3 点が
**#85**（PR #87 → `c7497fb`。Ex を `:` で開いた直後の字が `:` の前に入る。`EM_SETSEL(-1, -1)` は「末尾へ飛ぶ」ではなく
選択を解くだけで、解いた先は 0。`GetWindowTextLengthW` の長さで当てる 1 行に直した。**#37 以来の欠陥**）・
**#86**（PR #90 → `9c1a35e`。絞り込みの語が索引の描き直しで消える。**原因は `WS_CLIPSIBLINGS` ではなく z 順**で、
あとから作った子は兄弟の背面に入る。`SetWindowPos(HWND_TOP)` を 1 回。あわせて絞り込み中の**枠・×・件数**を足した。
ADR 0024 の補正 8 項）・**#88**（PR #92 → `262b53f`。ADR 0033。閉じる・設定・折畳・選択の印を画面案 C の
**24 の viewBox の点の表**にし、OS 同梱の **GDI+** で既存の DC へ塗る。SDK の `gdiplus*.h` は C++ 専用なので
`windows.h` だけを読む**型定義 0 の自前の宣言 18 本**で結ぶ。`ui_text` から `GLYPH` の 2 値が消え、
**単独で描く字形は 0** になった。96 DPI の × は 1.53 倍濃く、カーソルの角は 18px 左へ動く）になった。
そのあいだに **#76**（PR #89 → `4dcd3c7`。単位 C・ADR 0032。表示言語 **日本語 / English / 简体中文**。
`ui_text` の表を 3 列に、`settings.json` を**版 3**（`language`）に、書体は core の `ui_font` と ui の `ui_face`。
入口は設定画面の 2 段目と `:set language=`。新しい **CNF-011** が列の網羅を守る。**日本語の字面の変更は 2 件だけ**）を統合して
**親の #38 を閉じた**。後続で **#93**（PR #94 → `58122eb`。絞り込み欄の × も `icon_paint_fill(CLOSE)` へ寄せ、
**× を描く経路を 1 本に**した）と **#91**（PR #95 → `2a13318`。`WM_DESTROY` で `handle` を消した後の `WM_NCDESTROY` が
**`nullptr` の HWND で `DefWindowProcW` を呼んでいた**。`window_finalized` が引数の HWND で既定処理を呼んでから消す）を直した。
main は **`2a13318`**（clean）。最終フルゲートは各 PR（分岐網羅 **93.26%**（2710 / 2906）・conformance 89 テスト・実ツール反例 **17 本**）。
probe は #85 が 14・#86 が 26・#76 が 73・#88 が 43・#93 が 16・#91 が 14 項目成功。
#76 の独立レビューは**止める所見 S1**（起動時と閲覧中の言語切替で既定書式の face が当たらず、`i` で入った本文が `Segoe UI` になる）と
**訳 14 件**を同じ PR で直した。🔴 **简体中文は母語話者の確認を得ていない。**
[統合チェックリスト](docs/quality/2026-09-22-visual-checklist.md)は **85 項目**で、**結果が入っているのは 40-3 / 40-5 の 2 行だけ**である。
次は **#82**（名前入力面と失敗の箱のテーマ追随）→ **#70**（一致の入れ物のメモリ）→ **#42**（同梱フォント）。
最新は[日報](docs/reports/2026-09-23.md)の「再開後 2」/ [引き継ぎ](docs/handoffs/2026-09-23.md)の「再開点 2」。

2026-09-23: **#38（設定・テーマ・言語）を ADR 0029 の 4 単位に分け、前提 0 と単位 A・B を統合した**。mainは`72fb136`（clean）。
順に **#73**（PR #77 → `53acb20`。`folio_state_create` の 3 ポートを `struct folio_ports` へ束ねて 2 引数にした。振る舞い・文言・スキーマの変化 0）、
**#67**（PR #78 → `ba0e538`。検索欄・Ex・パレット・置換の 2 欄・絞り込み欄の**どこでも Ctrl+S で保存し、欄は開いたまま**。
保存は `command_save` の 1 本のままで第 2 の経路を作らない・ADR 0016 の補正 2）、
**#69**（PR #79 → `d238c01`。最小 560×360 でパレットの操作行が 0 行になる**#37 以来の欠陥**を直した。
箱の高さは `command_box_height(rows, help_rows)` の 1 本・操作に下限 3 行・ヘルプは PgUp / PgDn のページ送り・印は ASCII の「n/m」だけ）、
**#74**（PR #80 → `f881f16`。単位 A・ADR 0030。利用者に見える文言 123 値を core の `ui_text` の表 1 か所へ集め、
**字面は 1 字も変えていない**。言語は最初から引数、句は置換子（`{k}` `{n}` `{name}` …）で組み、**語や数を連結して句を作らない**。
新しい **CNF-010**（字句検査）と **C-018** を対で active にし、直書きは 73 行 → 0 行）、
**#75**（PR #81 → `72fb136`。単位 B・ADR 0031。**テーマ（OS に従う／ライト／ダーク）**と**設定画面**を足し、
`data/settings.json` を**版 2**（`{"version":2,"number":<bool>,"theme":"…"}`。版 1 は読めて `system` を埋める・書くのは常に版 2）にした。
選択の 3 値と描画の 2 値を型で分け、解決は core の純関数 `folio_theme_resolve` 1 か所。**先に書いてから採用**し、
再着色は `Undo(tomSuspend)` と変更印の退避復元で**本文・Undo の段数・未保存の印・選択・スクロール位置を失わない**。
`WM_SETTINGCHANGE`（ImmersiveColorSet）で OS の切替に追従し、配色は Ubuntu 風の 2 組。入口は歯車・パレット／メニューの「設定」・`:set theme=`）。
独立レビューの**止める所見 S1**（パレットから開いた設定画面の行が 1 つも描かれない）は「**行を決めてから配置する**」順へ直し、
D1（設定の失敗を欄の 1 行へ）・D6（閲覧は `SCF_ALL` を当てない）・D7（確保失敗時の不変）・絞り込み欄のテーマ追随も同じ PR で直した。
最終フルゲートは各 PR（分岐網羅 **93.31%**（2680 / 2872）・conformance 79 テスト・実ツール反例 16 本）。
probe は #67 が 62・#69 が 70・#74 が 60・#75 が 68 項目成功。**実機の目視は 1 項目も行っていない**
（[統合チェックリスト](docs/quality/2026-09-22-visual-checklist.md)は 55 項目で結果は全行が空欄）。
次は**#76（単位 C：言語 ja / en / zh-Hans・版 3・ADR 0032 の草案から）**で、それが終われば親の #38 を閉じる。
そのあと #82（名前入力面と失敗の箱のテーマ追随）・#70（一致の入れ物のメモリ）・#42（同梱フォント）。
最新は[日報](docs/reports/2026-09-23.md) / [引き継ぎ](docs/handoffs/2026-09-23.md)。

2026-09-22 再開後: hideの再開指示で**#41（正規表現置換・FR-023 / ADR 0028）を完成させ、PR #68 → main `f4e7bcf` へ統合**した（Issue #41は閉じた）。
停止時点で未着手だったui層・docs層・probeを足した。置換の欄は下端の入力面（ADR 0016の補正）で、パターンと置換文字列の**2つのEDIT**・
自前描画の「1 件」「すべて」・状態の1行を持つ。Enterが1件・Ctrl+Enterがすべて・Tabが欄の往復・Escで元の区画へ戻る。
適用は`note_pane_replace`（`EM_EXSETSEL`→`EM_REPLACESEL(TRUE)`）1回の**Undo 1単位**で、mdは書かない。
Ex `:%s/パターン/置換/[g]`は欄を開かず直接当て、`g`無しは各論理行の最初の一致。成功なら黙って閉じ、0件なら閉じずに「対象名 / 0 件」。
**独立レビューで止める所見B1**（失敗した下見のまま当たる）を構造で直した: applicationは`folio_state_preview_replace`がREADY以外を返す
すべての経路で**下見を捨て**、下見が無い適用は既存の`REPLACE_STALE`になる（結果の値は足さない）。UIが下見の成否を見るのは
**表示の都合**（出ている失敗の1行をSTALEで上書きしない）で、安全の正本はapplicationである。
併せてD1（一致の列を広げるreallocの確保失敗を測定ビルドで通す）・D2（`note_replace_build`が`count > capacity`を
`NOTE_REPLACE_PARTIAL_MATCHES`で断り、黙って部分適用しない）・D3（ゼロ幅の一致を空文字列で置き換える1件は本文も選択も変えない。
特別規則は足さず操作表に記録）・D4（死に枝3か所）と、`inline_outcome()`を**全42値のswitch**にする修正を入れた。
最終HEADでフルゲートexit 0（CTest 2/2・分岐2586/2772=**93.29%**・15 proofs・conformance 69件）。
実アダプタprobe 32項目・Win32部品probe 32項目が成功（[確認記録](docs/quality/2026-09-22-replace-checks.md)。ICU 72.1 / Unicode 15.1）。

hideの判断4点: (1) **入力面のCtrl+Sは「保存して欄は開いたまま」**にする → **Issue #67**（この単位では直していない）。
(2) 実機の`data/`は**同期フォルダの中ではなくローカルNTFS**。(3) #39〜#41の**実機目視は#41統合後にまとめて**行う
（[統合チェックリスト](docs/quality/2026-09-22-visual-checklist.md)）。(4) 置換の欄の**画面案（Artifact）は不要**。
新しいIssue: **#67**（入力面のCtrl+S）・**#69**（最小寸法560×360でパレットの「キー操作を表示」の行が0行。#41以前からの欠陥）・
**#70**（`struct regex_match`が200バイト超で、`regex_match_limit`直下では`found`と下見の写しに各約200MBを要する。出力上限8MBと釣り合っていない）。
次の単位は**設定・テーマ・言語 #38**で、`settings.json`を版2にする前に**`struct folio_ports`でportを束ねる**
（`folio_state_create`は#41で4引数に飽和した・ADR 0028の決定2）。

2026-09-22 停止: hideの指示で日報/引き継ぎを保存して停止。mainは`4101196`（#39 / #65まで統合済み・Issueは閉じた）。
#41（正規表現置換・ADR 0028）は`out/worktrees/41-replace` / `feat/41-regex-replace`のHEAD `802e2de`でcore・adapters・applicationの3層が済み
（フルゲートexit 0・分岐93.21%）、**ui層・docs層・probeの実行は未着手。Draft PRは無い。** `execute_command()`の`REPLACE` / `SUBSTITUTE`は仮の分岐で、
パレットの「置換」は出るが何も起きない。最新の停止点は[引き継ぎ](docs/handoffs/2026-09-22.md)の先頭節と[日報](docs/reports/2026-09-22.md)。明示的な再開指示まで続行しない。

2026-09-22: #39（原文の行番号）はPR #64でmain `39099c5`へ統合済み。**失敗の文言の表引き（#65 / ADR 0027）**を
`out/worktrees/65-failure-lines` で実装した。`folio_state_failure_line` と `unfinished_failure_line` の
full-enumのswitch 2つ（どちらもC-012の60行に飽和）を、`failure_lines[]` の指示付き初期化子
（`[FOLIO_STATE_X] = "…"`）を1回引く関数に置き換えた。**文言は1字も変えていない**（33値を変更前の本文から
機械的に写し、単体の同じ形の表で固定した）。表の外の値へは落ちないので添字の範囲検査は書かない。
switchを離れて失った `-Wswitch-enum` の網羅性（C-002）は新しい**CNF-009**（`eng/conformance.py`）が守り、
設定は `eng/conformance-rules.json` の `lineTables`（列挙のヘッダと表のファイルの対）に置く（検出語は
検査器のソースに直書きしない）。`eng/prove-gates.py` は全ファイルを写した木で実 `conformance.py` を走らせ、
表から1値を消すと落ち戻すと通ることを確かめる（実ツール反例14→15本）。**結果の値を足すときは表へ1行足す**。
最終フルゲート・CIは#65のPRを参照する。次は#41（置換）か#38（設定・テーマ・言語）。

2026-09-17 停止: hideの指示で日報/引き継ぎを保存して停止。mainは`3ab187d`（#40まで統合済み）。
#39は`out/worktrees/39-number`のDraft PR（実装の最終`4a58fbf`・フルゲートexit 0）で、独立レビュー・Ready・統合は未実施。
最新の停止点は[引き継ぎ](docs/handoffs/2026-09-17.md)の先頭節と[日報](docs/reports/2026-09-17.md)の「再開後」。明示的な再開指示まで続行しない。

2026-09-17: **原文の行番号（FR-021 / #39 / ADR 0025・ADR 0026）**を `out/worktrees/39-number` で 5 層に実装した。
`:set number` / `nonumber` / `nu!`（別名 `nu` / `nonu` / `invnumber` / `invnu`）と、パレット・「操作」メニューの
「行番号の表示を切り替える」が application の同じ意図（`folio_state_set_number`）へ渡る。値は
`data/settings.json` の版 1（`{"version":1,"number":<bool>}`）に残り、**先に書いてから採用する**。
無いファイルは既定値（非表示）で始めて起動時には作らず、読めない設定は既定値で起動して理由を 1 回だけ見せ、
そのセッションは設定を変えず**上書きもしない**。番号は**編集モードで文書があるときだけ**主窓が本文の左の空きに描き、
閲覧（RTF）では出さない（無題の編集中は出す）。継続行には番号を重ねず、空行と末尾の空行にも付く。
論理行の数え上げは core の `line_index`、表の所有者は `note_pane`（`EN_CHANGE` と本文の差し替えで印を付け、
次に描くときだけ作り直す）、y は表示行ごとに `EM_POSFROMCHAR` から得る（**行高は一定でない**）。
**`EM_STREAMIN` は `EN_CHANGE` を 1 回出す**ので、`EN_CHANGE` では本文へ問い合わせない。
帯の幅は桁数 × 数字の幅 ＋ 8px で、3〜4 桁（9999 行）までは既存の 36px の空きに収まり本文は動かない。
`ADR 0016` の決定 1 と `ADR 0018` の決定 2 は core の閉じた述語 `folio_command_listed` で補正し、
語を要る `:set` だけがパレットとメニューに出ない。前提として `paint_pane` を更新矩形に限る修正を先に入れた。
単体・実アダプタ 24 項目・Windows 部品 57 項目が成功（[確認記録](docs/quality/2026-09-17-number-checks.md)）。
分岐網羅 92.99%。最終フルゲート・CI は #39 の PR を参照する。**描いた絵そのものは見ていない**ので実機の目視が残る。

2026-09-17 再開後: hideの再開指示で#40 / PR #63の独立レビューと修正を行った（ADR 0024の補正節）。最終フルゲート・CI・統合はPR #63を参照する。
次は#39（行番号）で、実測は`out/design/2026-09-17/number-probe/`、設計はADR 0025（`data/settings.json`の版1）とADR 0026（行番号の描き方）の草案で、#39の枝の最初のコミットで置く（まだリポジトリには無い）。
最新は[引き継ぎ](docs/handoffs/2026-09-17.md)の先頭節と[日報](docs/reports/2026-09-17.md)の「再開後」。

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
1000ノート×10KBで最初の語301.0ms・以後の1打鍵49.8ms・全一致の配置1枚0.20ms。分岐網羅は92.85%。
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
