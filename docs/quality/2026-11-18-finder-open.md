# 品質記録 — 2026-11-18 「ノートを探す」の←→／Alt+←→と、Enter／Ctrl+Enter（#940）

#928の未決だった点を、選択のページ（D:/NeNeFolio/design/928-finder-choices/）でhideが「全部おすすめで」と決めた（2026-11-18）。そのうちキーの2点。決定は[ADR0110](../adr/0110-finder-mode-keys-and-two-opens.md)。

## 変更

- 入力欄が空のときの←→で名前/本文、どこでもAlt+←→で切替（`discovery_switch_mode`）。二択に「Alt+←」「Alt+→」を表示。
- Enterは開く（本文検索は閲覧表示で開き、ノート内検索で表示の中の最初の一致を選ぶ: `discovery_reveal`）。Ctrl+Enterと新しい「編集で開く Ctrl+Enter」は編集表示で開く。
- 開くボタンを2つに（`ACCESSIBLE_SURFACE_DISCOVERY_EDIT`、`DISCOVERY_FOCUS_EDIT`）。文言は三言語。
- 文書: ADR0110、操作とキー、仕様FR-047。

## 試験と結果

作業木 E:/NeNeFolio/worktrees/940-finder-open、構築 E:/NeNeFolio/outputs/940-finder-open/build。証跡の写しは D:/NeNeFolio/agents/940-finder-open/（編集に使ったscriptも）。

| 回 | 内容 | 結果 |
| --- | --- | --- |
| 編集 | Bashのheredocに入れたPythonの置き換えscript | 失敗（shellが引用符を解釈して起動前に止まった）。Writeで作ったscriptへ替えた |
| b1 | 最初の構築 | 失敗。clang-tidyの認知的複雑度が2関数で11（上限10）。閾値は変えず、`discovery_open_note`と`discovery_hinted`へ分けた |
| b2 | 構築・整形 | 成功 |
| t1 | `ctest -R "discovery"`と`folio_unit`（6件） | 5件成功、`body_discovery_native`が30秒の上限で打ち切り。同時刻のCPUは100%（他の席）。直接の実行は終了値0（37秒） |
| 比較 | 同じ負荷で#933の構築の`--discovery` | 19秒（朝は1.7秒）。#940は23秒。遅さは端末の負荷で、この変更ではない |
| 直接 | `--discovery`・`--body-discovery` | どちらも終了値0。空の入力欄の→で本文・←で名前、文字があるときの→はキャレットが0→1で切り替わらない、Alt+→／Alt+←で切替、変換中は切り替えない。Ctrl+Enterで「two」を編集表示で開き入力欄に改行が入らない。本文検索のEnterで閲覧表示・表示の中の最初の一致（5..11）が選択されノート内検索の語が「needle」、Ctrl+Enterで編集表示・原文の一致 |
| visual | `--body-discovery-visual`・`--discovery-visual`（各9通り） | 終了値0。最小の窓（560×360）で二択のキー表示と2つの開くボタンが収まる（日本語・英語の画像を確認） |
| 規約 | `eng/conformance.py`（ソース・構築）、`eng/symbols.py`、`git diff --check` | すべて0 |

既存の試験の変更: 本文検索で「開く」を押して編集表示になることを前提にしていた11か所を「編集で開く」へ替えた（意味はそのまま）。ボタンの名前の照合を新しい文言に合わせた。

## 確かめていないこと

- 実機の物理キーボードのAlt+←→（試験は`WM_SYSKEYDOWN`を送っている）と、日本語入力の実物。
- 閲覧で開いたとき、画像の読み込みが後から入って一致の位置がずれる場合（ノート内検索の既存の振る舞い）。
- 中国語の最小の窓の画像は目で見ていない（採取はしている）。
