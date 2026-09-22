# #75（テーマ・設定画面・`settings.json` 版 2）の確認記録

2026-09-23・枝 `feat/75-theme-settings`。ADR 0031 の「検証」に対する結果。
Win32 部品 probe の道具と全出力は `out/design/2026-09-23/theme-ui-probe/`。
設計のための実測（再着色 7 案・DWM・`WM_SETTINGCHANGE`・配色の候補）は
`out/design/2026-09-22/theme-probe/`。

## 1. core / application（単体・フルゲートの中で走る）

| 項目 | 場所 | 結果 |
| --- | --- | --- |
| 版 1 → 版 2 の移行（`theme` に既定値 `system` が入る） | `settings_tests.c` `verify_migration` | OK |
| 書くのは常に版 2（版 1 を読んで書くと `"version": 2` と `"theme"` が出る） | 同上 | OK |
| 版 2 の往復（3 つの選択 × `number` の真偽） | `verify_round_trip` × 3 | OK |
| 版 2 の解析（3 語・既定） | `verify_parse` | OK |
| 拒む形 18 件（未知の版・欠落・未知のキー・重複・型違い・未知の語・キー順・末尾の余り など） | `verify_refusals` | OK |
| `folio_theme_resolve` の 6 通り | `verify_resolve` | OK |
| 選択と OS の値から解決値が決まる・閲覧文書が解決した色で始まる | `state_tests.c` `verify_theme_resolution` | OK |
| `set_theme`：同じ選択は書かない・書けたら採用・閲覧文書が新しい色・編集中でも `pane` は作り直る・モード不変 | `verify_set_theme` | OK |
| `set_theme`：書けなければ選択も `pane` も変えない | `verify_set_theme_unwritable` | OK |
| `refresh_theme`：`system` のときだけ解決値が動く・設定を書かない | `verify_refresh_theme` | OK |
| `SETTINGS_UNREADABLE` のセッションは `set_number` と同じ値で断り、上書きしない | `verify_settings_unreadable` | OK |
| `set_theme` は `synchronize` を通さない（未完了の改名を再試行しない） | `verify_set_number_ignores_resumed_rename` | OK |
| 確保失敗（`folio_settings_with_theme`・`set_theme` と `refresh_theme` の閲覧文書） | `allocation_tests.c` `settings_scenario` / `set_theme_under_probe` | OK |
| `:set theme=` の 3 語と、`theme` / `theme=` / `theme=blue` / `theme = dark` の拒否 | `command_tests.c` `verify_options` | OK |
| `FOLIO_COMMAND_SETTINGS` が listed・別名なし・日本語の表示名 | `command_tests.c` `verify_listed` | OK |
| `catalog[]` の網羅（値を足して行を忘れると落ちる） | CNF-009 の `lineTables`（`folio_command.h` → `folio_command.c`） | active |
| `theme_names[]` の網羅 | CNF-009 の `lineTables`（`folio_theme_choice.h` → `folio_settings.c`） | active |

## 2. Win32 部品 probe（`out/design/2026-09-23/theme-ui-probe/`）

`recolor_probe` 19 / 19・`layer_probe` 9 / 9・`contrast.py` 26 / 26・`geometry.py` 6 / 6 が成功（**合計 60 項目**・FAIL 0）。

### 再着色（ADR 0031 の決定 6 の**未測定だった形**）

1 つの `Undo(tomSuspend)` の中で `SCF_ALL` と `SCF_DEFAULT` を続けて当てる:

| 測った値 | 編集中から | 保存直後から |
| --- | --- | --- |
| 本文 | 不変 | 不変 |
| 選択 `[7,9]` | 不変 | 不変 |
| `EM_GETMODIFY` | TRUE のまま | **FALSE のまま** |
| undoDepth | **1** | **1** |
| 本文の色 / 既定書式の色 | 両方 `#EFE4EA` へ変わる | 同じ |
| `EN_CHANGE` | 2 件 | 2 件 |

### 閲覧の流し直し

流し直しだけならスクロール位置は保たれる（`first=40 → 40`）。選択は `[0,0]` に消える。

### コントラスト比（production の値そのもの）

| 対 | ダーク | ライト |
| --- | --- | --- |
| 本文と地（編集） | 15.06:1 | 17.78:1 |
| 本文と地（閲覧 RTF） | 15.06:1 | 17.78:1 |
| 選択の文字と面 | 9.43:1 | 15.11:1 |
| 番号帯の字と地 | 6.33:1 | 6.10:1 |

参考の対も全部 4.5:1 以上（最小はライトの頭の薄い文字 5.20:1、ダークの札の文字 5.11:1）。
`border` は 1.56:1 / 1.38:1 だが、文字ではなく区切りの装飾なので合格線の対象にしていない。

### レイヤーと寸法

`WS_CHILD` の自作クラスがフォーカスを取り（`WM_SETFOCUS` 1 件）、別の子へ移ると `WM_KILLFOCUS` が 1 件。
`WM_KEYDOWN` は ↑ ↓ Enter Esc Tab 英字の 6 件すべてが届き、`WM_CHAR` は 6 件のうち
Ctrl+S / Ctrl+P / Enter / Tab の 4 件を飲む。`ImmAssociateContext(layer, NULL)` で IME 文脈が NULL になり、
同じプロセスの EDIT の文脈は残る。
歯車は閉じるアイコンと重ならず、560 幅（144 dpi では 840 の実寸）でパンくずに 118px / 177px 残る（最小 48px / 72px）。
設定面の箱は `command_box_height(4, 0) = 272px` で、560×360 の 96 dpi（316px）でも 150%（474px）でも 4 行とも入る。

## 3. 直した所（probe が見つけた 1 件）

**`EM_EXSETSEL` は `EM_SCROLLCARET` を送らなくても選択を見える位置へ寄せる。**
ADR 0031 の決定 6 は「位置は動かない」と読める書き方だったが、空の選択 `[0,0]` を戻すと先頭へ飛ぶ（`45 → 0`）。
`recolor_pane` を**空でない選択のときだけ戻す**形に直した（検索の一致は見せたいので寄るのは望ましく、
選んでいないときは流し直しが保った位置がそのまま残る）。ADR 0031 の「2026-09-23 の補正」節 1 に記録した。

## 4. 限界（この単位で測っていないこと）

- **描いた絵そのものは見ていない**。2 組の配色の見え方・歯車と印の大きさ・縁の色は実機の目視に残る
  （probe の環境では窓が画面取得に写らない・2026-09-22 の `dwm_probe` の節 4）。
- Settings アプリで OS を実際に切り替えたときの `WM_SETTINGCHANGE` の件数・順序・遅れ。
  `HWND_BROADCAST` で投げた場合（トップレベルに 1 件・`WS_CHILD` に 0 件）までが実測。
- IME を ON にして設定画面で打ったときに組成が始まらないこと（`SendInput` がこの環境で届かない）。
- 起動中の DPI 変更とテーマ切り替えの組合せ（ADR 0031 の決定 10 でやらないと決めた）。
- `name_prompt` と失敗箱はテーマに追随しない（OS 既定色のまま・決定 10）。
