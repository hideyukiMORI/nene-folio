# #348 — 操作部品の3様式＋選択群と1つの状態規則（I-A）

2026-10-18、hideが明示した分担（設計=Fable席、実装/調査/テスト=Opus席）に基づく実装席の記録。
Issue: [#348](https://github.com/hideyukiMORI/nene-folio/issues/348)。設計の正本は [ADR0060](../adr/0060-ui-part-styles-and-state-rule.md)。
規則: ARC-001/004/011、C-002/008/012/017/018、QLT-008/011/012/013、GIT-001〜004。基点 main 7233647。
恒久証跡: `D:/NeNeFolio/agents/348-ui-parts/`（final/ が3言語の matrix、final-head2/ が最終の C 差分（カテゴリ＋の tooltip を含む）の exe での ja の対象確認の再実行、run1〜4・hoverprobe・uiaprobe* は途中診断）。
schema・依存・ゲート・閾値の変更なし。新しい theme トークンなし。Waivers: none。

## 変更境界

- 描画の正典を `src/ui/win32/ui_part.c` の1本にした（様式 A/B/C/S × normal/hover/pressed/selected/focus/disabled、群の枠、入力欄の枠）。
  共通ヘッダー・本文操作帯・ドロワーのカテゴリ＋・全ノート検索欄・ノート内検索欄・名前入力がこれを通る。XOR の `DrawFocusRect` は0件。
- 線の色は core の `theme_contrast_line`（`src/core/theme_contrast.c`）が役割ごとの地すべてに3:1以上の文字系トークンを選ぶ。
  UI は `folio_palette` に写した派生値を使い、トークンを直接選ばない。
- 押下は `pressed_part` の1つの値。マウス・キーボード・UIA は `folio_window_accessible_action` の1経路で実行する。
- 表示名・UIA Name・AcceleratorKey・tooltip は `accessible_controls` の1つの表（label/key/hint）から引く。
- 閲覧/編集の札は位置と段（パンくず段の右端）を変えず、S 様式へ揃えた（設計席の補足どおり）。

## 完成時の対象検証

- `pwsh -NoProfile -File ./eng/check.ps1`（作業木）: f055574 で exit 0（`check-1.log`）、最終の C 差分 7dbcb96 で exit 0（`check-2.log`）。
  どちらも「NeNe Folio full gate passed」。以後の文書だけのコミットは C/H を変えない。最終 head のフルゲートは必須 CI に集約する。
- `folio_tests`: 既存に加え、`theme_tests` の「線の役割／隣接色 ≥ 3:1」を全127テーマ×7役割で検査（独立の pow の式で比を計算）。
  畳んだ sRGB 表と WCAG の式の一致（256値）、既定トークン維持・最初の合格トークンへの切替・どれも届かないときの最大比の3例を合成パレットで確認。
- 文言 unit: 追加した tooltip/キーの9 ID を期待表へ足し、3言語の網羅は CNF-011 のまま。

## 隔離 Win32 fixture（`D:/NeNeFolio/agents/348-ui-parts/final/`）

専用デスクトップ `NeNeFolio-348-ui-parts` で作業木の Debug exe を複製した fixture を起動し、Path と SHA256 を照合した。
入力は PostMessage、撮影は PrintWindow(PW_RENDERFULLCONTENT)。前面化・SendKeys・実カーソル移動なし。終了は WM_CLOSE、3回とも exit 0。
hide の稼働プロセスとデータには触れていない。データは合成の fixture（`data-pristine` の複製）を毎回置き直した。

- geometry: 120/144/168 DPI（実モニタへ移動）× ja/en/zh-Hans × 両方/ノート/一覧 の27条件＋各 DPI・言語の560×360 両方の9条件、計36枚。
  全条件で UIA の Button/RadioButton の高さは論理28（36条件とも1種類）。ヘッダーは 8+28+8、表示3択は 56×3 と群の余白4、本文操作帯は 4+28+4。
  一覧のみ（幅320）で新規と表示群の間の移動領域は論理24を保つ（下の補正）。560幅ではノート内検索が虫眼鏡の絵へ縮み、消えない。
- press-release-cancel: 表示「ノート」を押下しただけでは表示は変わらず capture が主窓にある。押したまま外へ出すと押下の見た目が消える（差分は項目の矩形内だけ）。
  外で離す・WM_CANCELMODE の後に内で離す → 実行なし。内で離す → 実行。新しいノートを外で離す → 無題は作られない。
  操作▾は押下でメニューが開き、Esc で閉じる。カテゴリ＋を外で離す → ダイアログは出ない。
- focus: マウスの押下の後は枠なし（focus-00）。F6/Tab の後だけ2pxの実線（単独は1px外側、群の項目は内側）。
  F6 の巡回は 本文 → 新規 → 検索欄 → 選択中の札。Tab は表示3択に1回だけ止まり（選択中の「両方」）、次は設定。
  →で focus だけが「ノート」へ移り選択は「両方」のまま、Enter で選ぶ。Space は押下では実行せず離した時に1回だけ実行。
  自動繰り返しの Enter・Space は無視。本文操作列も同じく札の群を1回で抜けて保存へ進む。
- tooltip: TTM_ENUMTOOLS / TTM_GETTEXT で製品の TTN_GETDISPINFO を通して読んだ。主窓の12部品とドロワーのカテゴリ＋に登録があり矩形は描画と一致。
  字付きはキーだけ（Ctrl+N・Ctrl+Shift+1〜3・Ctrl+S・F1・Ctrl+F）、一覧は「窓を細くします。」を添え、キーの無い字付きは補足（保存して閲覧・本文の編集を始める・このノートの操作）、
  絵だけは名前＋キー（閉じる / Alt+F4、設定を開く、新しいカテゴリ）。
- UIA: 部品の Name は表示名と一致し、AcceleratorKey を公開（例 新しいノート=Ctrl+N、一覧=Ctrl+Shift+1、閉じる=Alt+F4）。
  ヘルプ面の釦の名前は描いた字と同じ「キー操作を表示」→ Tab 後「キー操作を閉じる」。

## 単体の描画確認（`D:/NeNeFolio/outputs/348-ui-parts/render-probe/`）

製品の `ui_part_draw` / `icon_paint_fill` / `folio_palette_from` をそのまま呼ぶ使い捨て exe（repo に置かない）で、
様式×状態の格子を dark / light / darkblue / vscode-high-contrast × 96/120/144/168 DPI で描いた。
hover の内側の枠・押下の塗り・選択の塗りと下辺の棒・単独の外側/群の内側の focus・disabled・入力欄の静止1px/focus2px を目視した。
保存・検索・ヘルプ「?」の面パスを 16/20/24/28/32/48/96px で描き、潰れ・抜けの誤りがないことを目視した。ヘルプ「?」の配置は I-B のため製品では未使用。

## 未測定・限界

- hover の画面上の見た目は fixture で撮れない。隔離デスクトップでは実カーソルが (0,0) にあり、TrackMouseEvent が直ちに WM_MOUSELEAVE を出す。
  一時に作った計装版の exe（ソースは戻し、製品には含めない）で、WM_MOUSEMOVE が部品の hover 値を立て WM_MOUSELEAVE が下ろす配線は確認した（hoverprobe/）。見た目は単体の描画確認だけ。
- 隔離デスクトップの UIA クライアント（.NET と native の両方）は Win32 の EDIT・Button・Static をすべて Pane として読み、client-side proxy が効いていない。
  検索欄の MSAA の Name は「すべてのノートを検索」と読めた。UIA の Name の注釈（Name の性質の GUID）も足したが、実デスクトップのナレーターでは未確認。
- 実カーソル・実キーボード・ナレーターの読み上げ、96 DPI、ハイコントラストの実機、Space の離鍵を物理キーで押す確認は行っていない。
- 既存の保存・RTF・検索性能の試験は再実行していない（ADR0060 の強制と確認）。

## ADR0060 との差（設計席へ報告）

- 表示3択の群と歯車の間は 8 にした。群間16では一覧のみ（幅320）で ADR0059 の移動領域24が取れないため（12+28+24+176+16+28+4+28+12 > 320）。
- 「操作▾」のシェブロンを絵の位置へ置く件は、決定6に面パスが無く、文言（「操作 ▾」）の変更は I-C に掛かるため、字の▾のまま据え置いた。
- B 主操作は描画規則だけ実装し、適用（変更ありの保存）は I-B のまま。disabled は描画規則だけで、主窓に常時無効の部品は無いので出ない。
