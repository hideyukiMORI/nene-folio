# 空状態で主スレッドが回り続ける（#356）

2026-10-19、設計席（Fable 席）からの依頼による実装席の記録。Issue: [#356](https://github.com/hideyukiMORI/nene-folio/issues/356)。
規則: ARC-001/004、C-002/008、QLT-008/012/013、GIT-001〜004。基点 main 4f03030（#349 c78a398 の後は文書だけの変更）、作業枝 `fix/356-empty-state-message-storm`。
証跡と道具: `D:/NeNeFolio/agents/356-empty-state/tools/`（駆動）、`D:/NeNeFolio/outputs/356-empty-state/`（計測の exe・fixture・結果）。repo には置かない。
新しい theme トークン・pragma・waiver・閾値の変更なし。Waivers: none。

## 原因

ADR0060 の決定 13（#349）で、文書が無いあいだ本文の RichEdit を隠すようにした。`arrange` は `body_shown`（`pane_visible` かつ文書あり）が真のときだけ本文を
`MoveWindow` する。一方、毎回の WM_PAINT で本文の左端を確かめる `check_pane_inset`（ADR0026 の決定 6）は述語が `pane_visible` のまま残った。
空状態では本文が期待の左端へ置かれないので比較が毎回ずれ、次の自己ループになる。

WM_PAINT → `check_pane_inset` → `PostMessageW(folio_message_gutter_resized)` → `rearrange_keeping_line` → `arrange`（ドロワー・検索欄の `ShowWindow` で
WM_SHOWWINDOW → UIA の再予約）＋ `InvalidateRect`（client 全体）→ WM_PAINT → …

カテゴリの有無に依らず「文書が無い」起動直後から起こる（data 無し・data あり・空の data フォルダの 3 条件で再現）。
依頼時に疑った「`refresh_snapshot` 中の `accessibility_host_schedule` の再帰」は起きていない（下の計装で refresh 中の再予約は 0 回）。

## 経路の確定（一時の計装、ソースは戻した）

計装版の Debug を隔離デスクトップ `NeNeFolio-356` で data 無し・入力なしで 10 秒動かした。
`accessibility_host_schedule` が実際に投函した回数と `host->refreshing` 中の回数・呼び出し元のスタックを窓の property へ書き、
主ループで取り出したメッセージを id ごとに数えて終了時に stderr へ出した。

- 主スレッド CPU 6.97 秒 / 10 秒（Running）。投函 484 回、うち refresh 中 0 回。最後の投函元は `drawer_procedure` → `accessibility_host_changed(WM_SHOWWINDOW)`。
- 主ループのメッセージ: WM_PAINT 484、`folio_message_gutter_resized`（WM_APP+4）483、`folio_message_accessibility_changed`（WM_APP+6）484。三者が同数で 1 周ごとに 1 通ずつ。

## 変更

`check_pane_inset` の述語を `arrange` が本文を置く条件と同じ `body_shown` にした。隠した本文は配り直しても動かないので比べない。
配り直しの合流点（WM_PAINT の検査 → `folio_message_gutter_resized` → `rearrange_keeping_line`）は 1 つのまま。
文書があるときの挙動は変わらない（本文が見えるあいだ `body_shown` と `pane_visible` は一致する）。
`accessibility_host_schedule` に refresh 中の抑止は入れない。根本原因ではなく、入れても配り直しと描画のループは残るため。

## 完成時の対象検証

隔離デスクトップ `NeNeFolio-356`、入力は PostMessage、起動した PID だけを Path と SHA256 で照合して操作した。
修正前（`bin-base`、SHA256 0E5BC7AA…D0AA）と修正後（`bin-fixed`、SHA256 958FFAD5…7B84）を同じ駆動（`tools/matrix.ps1`）で比べた。
各条件: 起動後に入力なしで 20 秒の主スレッド CPU → PostMessage の Ctrl+N（WM_CHAR 0x0E）→ 本文が開けば `:set nu` の後 5 秒の CPU → Esc `:q!` Enter で終了。

| 条件 | 修正前 20 秒の主スレッド CPU | 修正後 | Ctrl+N（本文が開く） | `:q!` で終了 |
| --- | --- | --- | --- | --- |
| data 無し | 19.94 秒（Running） | 0.00 秒（Wait/UserRequest） | 開かない（カテゴリ 0 件。仕様どおり作れない理由を出す） | 前後とも exit 0 |
| data あり（カテゴリ 2 件） | 19.97 秒（Running） | 0.00 秒（Wait/UserRequest） | 前後とも開く。修正後 `:set nu` の後 5 秒で 0.00 秒 | 前後とも exit 0 |
| 空の data フォルダ | 19.94 秒（Running） | 0.00 秒（Wait/UserRequest） | 開かない（カテゴリ 0 件） | 前後とも exit 0 |

合格の条件は「3 条件とも起動後 20 秒の主スレッド CPU 合計 1 秒未満、PostMessage の入力が届く」。修正後は 3 条件とも満たした。
カテゴリ 0 件の Ctrl+N は KEY_BINDINGS の規則どおり無題を開かないので、入力の到達は `:q!` の終了で確かめた。

- `pwsh -NoProfile -File ./eng/check.ps1`（57d6b01）: exit 0、「NeNe Folio full gate passed」、分岐網羅 93.23%（3552/3810）。
- 単体試験は足していない。原因は ui/win32 の配置と描画の間にあり、既存の単体試験の範囲（core / application）に入らない。
  `accessibility_host` の再帰の単体試験は、再帰が原因でないため足していない。

## 未測定・限界

- hide の実機の「マウス・キーが届かない」は再現していない。隔離デスクトップでは PostMessage の入力は修正前も届いた。
  実機では 1 周ごとの全面描画と UIA の再予約が続き、物理入力の処理が後回しになったと見ているが、実カーソル・物理キーでは確かめていない。
- `:set nu` の確認では 1 桁の帯が既存の 36px に収まり、本文の左端は動かなかった（276 → 276）。桁数が増えて左端が動く配り直しは、本修正で経路が変わらないため再実行していない。
- I-B の保存遷移（`2026-10-18-action-bar-and-save-state.md`）の対象確認は、文書があるときの経路が変わらないため再実行していない。
