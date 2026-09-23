# ADR 0036 — 日本語・简体中文・欧文の書体を固定版で `fonts/` に同梱し、exe の隣から `FR_PRIVATE` で登録して `ui_font` の表で選ぶ

- 状態: 受理（設計リナ 2026-09-25。hide の判断: 資産はリポジトリに入れる・埋め込まず隣のフォルダから読む・書体は 2026-09-12 の選定のまま。現物調査 `out/agents/42-probe/memo.md`、実測 `out/design/2026-09-25/font-bundle-probe/`）
- 日付: 2026-09-25
- Issue: #42
- 影響する規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / ARC-011、C-002 / C-003 / C-005 / C-007 / C-012、CNF-002 / CNF-009 / CNF-011、**新設 CNF-012**、QLT-011（据え置き・注記）
- 関連: ADR 0005（書体の意図）、ADR 0032（言語ごとの書体 `ui_font` / `ui_face` と RichEdit の既定書式）、ADR 0027（`failure_lines[]` の表）

## 文脈

現行（main `5ec77b2`）の書体は OS のもので、言語ごとの face は core の `ui_font.c` の表（JA / EN → `Yu Gothic UI`、ZH_HANS → `Microsoft YaHei UI`）、実在の確認と退避は ui の `ui_face_for`（無ければ JA の face へ）が持つ（ADR 0032）。
主窓の自前描画（パンくず・札・番号帯・入力欄）は `Consolas` 固定で、言語で変えない（ADR 0032 決定 4）。
OS の書体に依ると、同じ md が機械ごとに違う字形と行送りで表示され、简体中文は `Microsoft YaHei UI` の有無に左右される。
Issue #42 は、Noto Sans JP / Noto Sans SC / Arimo を版と SHA-256 を固定して同梱し、OS に入れずプロセス内で登録し、読めない事象を黙って隠さないことを求める。

手元の事実:

- `out/fonts/2026-09-12/` に 6 ファイル（Noto Sans JP / SC の Regular / Bold・Arimo の Regular / Bold・合計 27,028,564 バイト）、ライセンス本文 2 ファイル（SIL OFL 1.1）、
  取得元 URL・upstream の revision・バイト数・SHA-256 を記した `manifest.json`（schema 1）がある。**すべて静的フォント**（可変ではない）で、SHA-256 は 2026-09-25 に照合して一致した。
- 実測（DPI 120・この機械）: 6 ファイルの `AddFontResourceExW(FR_PRIVATE)` は合計 **27ms**（中央値）、解除 1ms。登録後は GDI（`CreateFontW` → `GetTextFace`）・RichEdit 5.0（`CHARFORMAT2W`）・別スレッドの窓のいずれからも face 名で引ける。
  欠損のパスは 0 と `ERROR_FILE_NOT_FOUND`。行高は `tmHeight` が現行より 1〜2px 高く、RichEdit の行間は最大 6px 広い（-16 で 51px 対 49px）。JP と SC の字形は「骨・直・海・与」で 1.46% の画素が違う。
- 実測の限界: **この機械には OS 側に `NotoSansJP-VF.ttf` が入っていて**、同梱前から "Noto Sans JP" が列挙される（同名の衝突込みの実測）。RichEdit で SC と Arimo に漢字を渡した描画が同じ画素で、同梱の SC が選ばれているかは**face 名ではなくファイルの同一性で確かめる必要がある**。
- `build\data` は CMake ではなく手で置かれたもの。`.gitattributes` は png / ico だけ。git lfs は無い。QLT-011（依存は再現可能）は `planned` で、機械強制は無い。

## 決定

**書体の 6 ファイルとライセンスと manifest をリポジトリの `fonts/` に置き、CNF-012 が manifest との一致を機械で守る。exe の隣の `fonts\` から adapters/win32 の `font_bundle` が `FR_PRIVATE` で登録し、core の `ui_font` の表が同梱の face と OS の退避 face を言語ごとに持ち、ui の `ui_face_for` が同梱 → 退避 → 日本語の退避の順で選ぶ。読めなければ OS の書体で起動して 1 回だけ知らせる。**

### 決定 1 — 資産はリポジトリに入れる（`fonts/`）

- `fonts/` に `NotoSansJP-Regular.otf` / `NotoSansJP-Bold.otf` / `NotoSansSC-Regular.otf` / `NotoSansSC-Bold.otf` / `Arimo-Regular.ttf` / `Arimo-Bold.ttf`、`LICENSE-Noto-CJK.txt`、`OFL-Arimo.txt`、`manifest.json`（`out/fonts/2026-09-12/` の写し。schema 1・取得元 URL・revision・バイト数・SHA-256・ライセンスの対応）、`README.md`（何が入っていて、どう更新するか）を置く。
- 取得スクリプトや CI のキャッシュは**作らない**。再現性は git そのものと CNF-012 が保証する。履歴が 27MB 太るのは 1 回きりで、版を上げるのは manifest と 6 ファイルを一緒に差し替える 1 コミットで行う。
- `.gitattributes` に `*.otf binary` / `*.ttf binary` を足す。git lfs は使わない。
- 選定は 2026-09-12 のまま: 日本語 = Noto Sans JP、简体中文 = Noto Sans SC、欧文 = Arimo。ウェイトは Regular / Bold の 2 つ。等幅（`Consolas`）と主窓の自前描画は変えない。

### 決定 2 — CNF-012: 同梱の資産は manifest と一致する（active・機械強制）

- `eng/conformance.py` が `fonts/manifest.json` を読み、(a) `entries` と `licenses` の各ファイルが存在し**バイト数と SHA-256 が一致**する、(b) `fonts/` の下に manifest に無いファイルが無い（`README.md` と `manifest.json` を除く）、(c) 各 entry の `license` が `licenses` に在る、を検査する。設定（manifest のパスと除外名）は `eng/conformance-rules.json` の `bundledAssets` に置き、検査器のソースに直書きしない。
- `eng/prove-gates.py` に実ツールの反例を足す: 1 バイト書き換えると落ち、戻すと通る。
- `docs/QUALITY_GATES.md` に CNF-012 を **active** で足す。**QLT-011 は `planned` のまま**（対象はライブラリの版の lock で、資産ではない）。QLT-011 の節に「同梱の資産は CNF-012 が守る」の 1 行を足す。

### 決定 3 — 登録は adapters/win32 の `font_bundle` の 1 本

- `src/adapters/win32/font_bundle.{h,c}`。ヘッダは不完全型 `struct font_bundle;` と `[[nodiscard]] enum font_bundle_outcome font_bundle_register(struct font_bundle **out)` / `void font_bundle_destroy(struct font_bundle *)`。結果は専用ファイル `font_bundle_outcome.h` の閉じた enum: `FONT_BUNDLE_READY`（すべて登録）/ `FONT_BUNDLE_MISSING`（`fonts\` が無いか空）/ `FONT_BUNDLE_PARTIAL`（1 つ以上が登録できない）/ `FONT_BUNDLE_NO_MEMORY`。
- 読む場所は **exe と同じディレクトリの `fonts\`**（`persistence_adapter` が `data\` を決めるのと同じ `GetModuleFileNameW` の流儀）。中の `*.otf` / `*.ttf` を `FindFirstFileW` で列挙し、1 つずつ `AddFontResourceExW(path, FR_PRIVATE, 0)` する。`FR_PRIVATE` なので OS にも他のプロセスにも入らず、`font_bundle_destroy` が `RemoveFontResourceExW` で解除する。
- 所有者は 1 つ（ARC-004 に行を足す）: exe の入口が窓を作る**前に** `font_bundle_register` を呼び、メッセージループのあとに `font_bundle_destroy` する。application は書体を知らない（ARC-003 のまま）。
- ファイルを読むのは adapters だけ（ARC-007）。`eng/symbols.py` の検査対象に `AddFontResourceExW` / `FindFirstFileW` が無ければ、この ADR で adapters 限定の語として足す。

### 決定 4 — `ui_font` の表は「同梱の face」と「OS の退避 face」の 2 列

- core の `ui_font.c`: `ui_font_face(language)` は同梱の face（JA → `Noto Sans JP`、EN → `Arimo`、ZH_HANS → `Noto Sans SC`）、新設 `ui_font_fallback_face(language)` は OS の退避 face（JA / EN → `Yu Gothic UI`、ZH_HANS → `Microsoft YaHei UI`）。どちらも指示付き初期化子の表で、CNF-009 の `lineTables` に 2 つ目の表を登録して網羅を守る。
- ui の `ui_face_for(language)` は **同梱 → その言語の退避 → 日本語の退避** の順で `EnumFontFamiliesExW` の実在を確かめて選ぶ（現行の「無ければ JA へ」を 3 段にする）。RichEdit の既定書式・閲覧 RTF の fonttbl・ドロワー・名前入力面・失敗の箱は現行どおりこの 1 本から face を受け取る（ADR 0032 の配線は変えない）。
- 英語の UI が `Yu Gothic UI` から `Arimo` に変わる。日本語の行高が 1〜2px、RichEdit の行間が最大 6px 増える（実測）。番号帯は `EM_POSFROMCHAR` で行ごとに y を取るので動かない（ADR 0026）。

### 決定 5 — 読めなければ OS の書体で起動し、1 回だけ知らせる

- `font_bundle_register` が `READY` 以外なら、窓は退避の face で開き（決定 4 の 3 段がそのまま働く）、起動直後に失敗の箱を 1 回出す。文言は `enum folio_state_outcome` に **`FOLIO_STATE_FONT_BUNDLE_UNAVAILABLE`** を足し、`ui_text` の 3 言語・`failure_lines[]`・`state_tests.c` の写しの表に 1 行ずつ足す（ADR 0027 の流儀。「同梱の書体を読めなかったので、OS の書体で表示します。」）。黙って隠さない（Issue の要求）が、起動は止めない（閲覧のために起動を許す ADR 0022 と同じ判断）。
- 入口は `folio_window_create` に `enum font_bundle_outcome` を渡し、窓が最初の描画のあとに箱を出す。application の状態には入れない（書体は application の関心ではない）。

### 決定 6 — ビルドと配布

- CMake は `nenefolio` の POST_BUILD で `fonts/` の中身を `$<TARGET_FILE_DIR:nenefolio>/fonts` へ複写する（`copy_directory`）。`platformLibraries` は不変（`gdi32` は `ui_win32` にあり、adapters 側の `font_bundle` が要る `gdi32` は `eng/architecture.json` の adapters の行に足す）。
- `tools/prepare-real-machine.ps1` は変えない（ビルドが複写する）。インストーラや zip は範囲外。

### 決定 7 — 単位の切り方（S 級 3 本・順に統合）

1. **A 資産と検査**: `fonts/`・`.gitattributes`・CNF-012・`prove-gates` の反例・`QUALITY_GATES.md`・CMake の複写。製品コードは触らない。
2. **B 登録**: `font_bundle`・入口の所有・`FOLIO_STATE_FONT_BUNDLE_UNAVAILABLE` と文言・起動時の箱。表は触らないので見た目は変わらない。受け入れは「登録後に `CreateFontW("Noto Sans SC")` で選ばれた font の `GetFontData` の `name` テーブルが同梱ファイルのものと一致する」probe（face 名ではなくファイルの同一性）。
3. **C 表の差し替え**: `ui_font` の 2 列・`ui_face_for` の 3 段・単体の期待値・CNF-009 の表の登録。実機の目視（76 番台）は hide。

### 決定 8 — やらないこと

exe への埋め込み（`AddFontMemResourceEx`）、OS へのインストール、可変フォント・他のウェイト、等幅の同梱、主窓の自前描画の書体変更、取得スクリプトと CI のキャッシュ、フォントの改変（family 名の変更を含む）。

## 強制

- CNF-012（active・`eng/conformance.py`・`prove-gates.py` の反例）: manifest とファイルの一致。
- CNF-009（既存）: `ui_font` の 2 つの表の網羅。CNF-011（既存）: 文言の 3 列。CNF-002: `font_bundle_outcome.h` の 1 ファイル 1 型。
- ARC-007（`eng/symbols.py`）: ファイルを読む API は adapters だけ。
- 機械で守れないもの: 同梱の face が実際に選ばれること（同名の OS フォントとの衝突）。単位 B の probe と実機の目視で見る。

## 結果

- 得るもの: 3 言語の字形と行送りが機械に依らず同じになる。简体中文が OS の `Microsoft YaHei UI` の有無に左右されない。資産の版が git と CNF-012 で固定される。
- 失うもの: リポジトリと配布物が 27MB 増える。英語の UI の書体が変わる（`Yu Gothic UI` → `Arimo`）。日本語の行間がわずかに広がる。起動が約 30ms 遅くなる。
- 残るもの: 利用者の OS に同じ family 名のフォントが入っているとき（この機械の `NotoSansJP-VF.ttf` のように）、GDI がどちらを選ぶかは family 名では決まらない。同梱の優先を保証する手段はこの ADR では持たない（実測の限界として記録。単位 B の `GetFontData` の照合で「この機械では同梱が選ばれる」ことまでは確かめる）。
- 緑であることが証明しないもの: 描いた字形そのもの。76 番台の目視は hide が行う。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 取得スクリプト ＋ manifest でビルド時に取る | スクリプト・CI のキャッシュ・ネットワーク失敗の退避の 3 つが保守対象になる。git に入れれば再現性は git が保証し、オフラインで済む |
| exe に資源として埋め込む（`AddFontMemResourceEx`） | exe が 27MB 太り、CMake に資源の配線が増える。配布が 1 本で済む以外の利点が無い。ファイルなら差し替えと欠損の検査が単純 |
| 1 本の Noto Sans CJK に統合 | 日本語と简体中文の字形の切替が地域別ファイルより難しく、ファイルも小さくならない（ADR 0032 の実測どおり JP / SC を分ける） |
| 読めないときに起動を止める | 閲覧のために起動を許す判断（ADR 0022）と揃える。黙って隠さないことは 1 回の箱で満たす |
| family 名を変えて同名の衝突を避ける | フォントの改変になる。OFL は改変を許すが版の固定と取得元の照合が崩れる |
| git lfs | 導入と CI の設定が増える。27MB × 数回の更新なら通常の git で足りる |

## 検証

- 実測（受理前）: `out/design/2026-09-25/font-bundle-probe/`（登録 27ms・3 経路で見える・行高の表・JP / SC の字形の差分）。
- 単位 A: フルゲート（CNF-012 の反例を含む 18 本目の実ツール反例）。
- 単位 B: 単体（偽の列挙で MISSING / PARTIAL / READY）、`GetFontData` の probe、フルゲート。
- 単位 C: 単体（表の 2 列・3 段の退避）、フルゲート、hide の実機の目視（76-1〜76-14 をやり直す）。

## 移行

- 補正する文書: ARC-004 の所有者の表（`font_bundle` の行）、ARCHITECTURE_CONSTITUTION の「#42 が入ったら `ui_font.c` の表だけを差し替える」の注記、GLOSSARY「書体」、統合チェックリスト（76 番台に「同梱の書体」の観点）。
- Issue #42 は単位 C の PR で閉じる（A / B は `Refs #42`）。

## 2026-09-25 の補正（単位 B / C の実装の後・決定本文は書き換えない）

1. **`enum font_bundle_outcome` は core（`src/core/font_bundle_outcome.h`）に置く。** ui は adapters を include できず（ARC-002）、単体は core / application にしか依存できない。判定の純関数 `font_bundle_verdict_of`（`src/core/font_bundle_verdict.{h,c}`）も core にあり、adapters の `font_bundle` は列挙と登録だけを持つ。決定 3 の「専用ファイル」はこの置き場で読む。
2. **起動時の箱は `folio_window_create` が最初の描画（`ShowWindow` / `UpdateWindow`）のあとに出す**（`announce_fonts`）。「読めない設定」の既存の通知は `main.c` の窓を作る前の `MessageBoxW` で、契機が違う。決定 5 の「最初の描画のあと」のとおり。
3. **退避の表は別ファイル `src/core/ui_font_fallback.c`**（`ui_font_fallback_face`）。CNF-009 の `lineTables` は「列挙 → 表のファイル」の対で網羅を数えるので、同じファイルに 2 つの表を置くと片方の欠けを見逃す。
4. 実測（単位 B の identity probe・この機械・DPI 120）: 登録後は Noto Sans SC / JP / Arimo の 3 face とも `GetFontData` の `name` テーブルが同梱ファイルと一致し、JP は登録前が OS の `NotoSansJP-VF.ttf`、登録後が同梱の static だった。同名の衝突は「登録後は同梱が選ばれる」と、この機械では言える。
