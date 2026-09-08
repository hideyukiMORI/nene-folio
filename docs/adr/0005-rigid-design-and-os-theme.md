# ADR 0005 — 画面の見た目は「案2 堅」を採り、テーマは OS のアプリのモードに従う

- 状態: 受理
- 日付: 2026-09-09
- Issue: #9
- 影響する規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-007 / ARC-011 / C-002 / C-003 / C-014 / C-017 / QLT-013

## 文脈

3 本の縦切り（#3 / #5 / #7）でドロワーと右ペインの形は揃ったが、見た目は仮だった。施主は `/design` で作った案
（キャンバス https://claude.ai/code/artifact/286c67a2-e89a-4994-98de-2d452124f4a4 ）のうち、
「アプリらしい」初案と「かわいい」付箋案を退け、クールで堅い **案2**（ダーク／ライト）を採用した（2026-09-09）。
見た目の値はドロワー（GDI）と右ペイン（RTF）の 2 か所で使われるので、放っておけば必ずずれる。テーマも「設定ファイル」
「OS に従う」の 2 案があった。

## 決定

**色の正本を core の `rtf_palette`（RTF 用）と ui の `folio_palette`（自前描画用）の 2 つに固定し、どちらもテーマ
（`enum folio_theme`）から関数で引く。テーマは OS の「アプリのモード」（`HKCU\…\Themes\Personalize\AppsUseLightTheme`）
を起動時に読んで決め、設定ファイルは持たない。**

- 寸法とフォントはデザインの値をそのまま写す: ドロワーの頭 44px・カテゴリ行 34px（上に 6px）・ノート行 30px・
  番号は Consolas 11px にカテゴリ色・カテゴリ名は Yu Gothic UI 12px 太字（字間 2px）・折り畳みは − / +・
  選択行は面と右端 6px の角。右ペインの頭は 44px のパンくず（番号・カテゴリ・/・ノート）と「閲覧」の札。
  本文は Yu Gothic UI 11pt・見出し 22pt・コード Consolas 10pt
- 窓の角は DWM の小さな角丸（`DWMWA_WINDOW_CORNER_PREFERENCE = ROUNDSMALL`）、縁はテーマの色（`DWMWA_BORDER_COLOR`）。
  Windows 10 では無視される
- 右ペインの RichEdit にスクロールバーを出さない（FR-012 の流儀に合わせる）。ホイールで動く
- 検索窓と「編集」は機能が入るまで描かない。動かない部品は描かない
- OS のテーマを読むのは `appearance_adapter`（adapters/win32）だけで、advapi32 を `platformLibraries` に足す
- テーマの切り替えは起動時だけ。OS 設定の変更は再起動で拾う

## 強制

- 色の直書きが `src/ui/win32/folio_palette.c` と `src/core/rtf_palette.c` の外に無いことはレビュー事項（ARC-001）。
  `RGB(` と `{0x` の字句検査を CNF に足すのは、直書きが実際に紛れ込んだときの事故駆動で行う
- advapi32 は `eng/architecture.json` の `platformLibraries.adapters_win32` に 1 行足した。他のモジュールが結べば configure が落ちる（ARC-002・active）
- 実機の確認は `docs/quality/gate-proofs.md` 第 5-d 節（QLT-013）

## 結果

得る: ドロワーと右ペインが同じ設計言語になり、テーマの追加は 2 つのパレットに 1 行ずつで済む。
利用者は OS の設定を変えるだけでライト／ダークが切り替わる。

失う・残る:

- 🔴 RichEdit は段落の罫線（`\brdrl`）と、インラインコードの地（`\highlight` が行全体の高さの箱になる）を
  デザインどおりには描かない。引用は字下げと薄い色だけ、インラインコードは色と等幅だけにした。デザインからの逸脱として記録する
- ドロワー下端のフェード（あふれの合図・FR-012）は未実装。msimg32 の `GradientFill` は許可リストに無く、
  自前のアルファ合成もまだ書いていない。スクロールを入れる縦切りで扱う
- OS のテーマが起動中に変わっても追従しない（`WM_SETTINGCHANGE` を見ていない）
- Windows 10 では角が四角く、縁の色も付かない
- 高 DPI・複数モニタでのフォントと字間は 96 DPI でしか見ていない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 設定ファイル（`data/settings.json`）でテーマを指定する | 保存形式が 1 つ増え、版と移行（ARC-009）を持つ理由が「色を変えたい」だけでは弱い。OS の設定で足りる |
| 色を `#define` や各ファイルの `constexpr` に散らす | UI と RTF でずれる。正本を 2 関数に固定した |
| UI のパレットも core に置く | `COLORREF` は Win32 の型で、core は `windows.h` を含めない（ARC-003）。色の値は同じでも型が違う |
| 検索窓と「編集」を先に描いておく | 動かない部品は利用者を騙す。機能と同じ縦切りで描く |
| `WM_SETTINGCHANGE` で即時追従する | テーマの再適用に RichEdit の再流し込みと DWM の再設定が要り、今回の縦切りより大きい。次で扱う |

## 参考

- [ADR 0002](0002-plain-win32-no-ui-library.md) / [ADR 0004](0004-first-drawer-slice.md)
- [DWMWA_WINDOW_CORNER_PREFERENCE](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)
