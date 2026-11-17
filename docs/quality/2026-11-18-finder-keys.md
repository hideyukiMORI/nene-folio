# 品質記録 — 2026-11-18 「ノートを探す」の入力欄の↑↓・PageUp/Down・原文の折り返し（#933）

hideの11月18日朝の指示（外回りの間、マイルストーンに沿って進め、試したことを全部記録する）で、次の版の候補#928のうち、決めることが残っていない部分を先に実装した。
左右キーで名前/本文を切り替える件とCtrl+Enterは、入力欄の文字のカーソル移動・既存のEnterと衝突するので、hideが案を選ぶまで#928に残す。

## 変更

- 入力欄で↑↓: 候補を動かし、フォーカスは入力欄に残す（`discovery_pick`、[ADR0109](../adr/0109-finder-typing-keys-and-wrapped-preview.md)）。
- 入力欄でPageUp/PageDown: 原文previewを1画面ぶん縦に送る。previewが出ていなければ候補のページ送り。
- 原文previewを折り返す（`ES_AUTOHSCROLL`と`WS_HSCROLL`を外す）。行番号は論理行の1行目にだけ付ける（論理行の開始位置を記録し`EM_LINEFROMCHAR`で写す）。
- 文書: ADR0109、操作とキー（KEY_BINDINGS.md）、仕様FR-047。

## 試験と結果

作業木 E:/NeNeFolio/worktrees/933-finder-keys、構築 E:/NeNeFolio/outputs/933-finder-keys/build（Debug、`--parallel 4`）。証跡の写しは D:/NeNeFolio/agents/933-finder-keys/。

| 回 | 内容 | 結果 |
| --- | --- | --- |
| b1 | 最初の構築 | 失敗。targetの名前を`NeNeFolio`と書いた（正しくは`nenefolio`）。手順の誤り |
| b2 | 構築・整形 | 構築は成功、clang-formatの差が4か所（自分の追加行）。`clang-format -i`で直した |
| t2 | `ctest -R discovery`（4件） | `body_discovery_native`だけ失敗。新しい試験が、`SetWindowText`の後のキャレット（先頭）で文字を送っていて「xneedle」になった。製品ではなく試験の誤り。キャレットを末尾へ置いてから打つように直した |
| t3 | `ctest -R discovery`（4件） | 4件成功 |
| t4 | `--body-discovery`の直接実行（計測の表示） | ↓で選択0→1・フォーカスは入力欄のまま。previewは論理23行が表示40行に折り返し、横スクロールなし。PageUp/PageDownで最初の可視行が19→1→19、選択とフォーカスは不変 |
| visual | `--body-discovery-visual`（3言語×明暗×高コントラストの9通り、478 checks） | 成功。見本の37行目を長い行にして、折り返しと行番号の位置を採取した（body-discovery-wide-0.png・preview-0.png） |
| c1 | `eng/conformance.py`（ソース・構築）、`git diff --check` | すべて0 |

clang-tidyは構築の中で走る（CMAKE_C_CLANG_TIDY）ので、構築の成功で通っている。

## 統合前の独立レビュー（読み取り専用の席1つ）と第2回

前日の気づき（入口や配置を変える変更は統合の前にレビューを1回）に従い、PR934の差分を読み取り専用の席に見せた。重大な欠陥は無し。指摘の4件と扱い:

1. ホイールで候補を動かしても、入力欄にいればフォーカスが入力欄に残る。→ 意図どおり（入力欄の↑↓と同じ）。ADR0109に書いた。
2. PageUp/PageDownは、previewが出ていれば送り、出ていなければ候補のページ送り。→ 意図どおり（ADR0109の決定2）。
3. 窓の幅が変わると折り返しが変わり、一致がpreviewの外へ出ることがある（折り返す前は起きなかった）。→ 直した。previewの幅が変わったら、一致があるときだけ寄せ直す。
4. 原文にNULがあると、`WM_SETTEXT`がそこで止まり、後ろの行番号が最後の行に重なる。→ 直した。行の開始位置の記録をNULで止める。

| 回 | 内容 | 結果 |
| --- | --- | --- |
| b8 | 幅の試験を足した構築 | 失敗。clang-tidyの関数の長さ（63行、上限60）。閾値は変えず、幅の試験を別の関数へ分けた |
| t8・t9 | 窓を900・820へ狭める | 試験として無効。窓を狭めてもpreviewの幅が変わらなかった（527のまま）。広げる（1250×850、既存の採取と同じ）に替えた |
| t10 | 広げる | previewの幅527→641、最初の可視行0→13、一致が可視範囲 |
| m1 | 変異（寄せ直しを無効にする） | 期待どおり失敗（最初の可視行0→0、「一致が見える」で落ちる）。元へ戻した |
| t11 | `ctest -R`でdiscovery・search_native・palette_nativeの6件 | 6件成功 |
| t12 | `--body-discovery-visual`（9通り） | 478 checks成功 |
| c2 | `eng/conformance.py`（ソース・構築）、`git diff --cached --check` | すべて0 |

### 最初の必須CIの失敗と第3回

統合の手順（`tools/merge-pr.ps1 -Number 934`）の必須CIで、`body_discovery_native`の新しい試験「the wider window widens the preview」だけが失敗した（run 44191418340、63件中62件成功）。
原因は試験の前提: 検索の面の幅は「960（論理）と、窓の内側−32の小さい方」。手元は120 DPIで、1050 pixelの窓では面が窓に押されて狭く、1250へ広げると面も広がった。
CIは96 DPIで、1050 pixelの窓で面はすでに960いっぱいなので、広げても変わらない。前日の#839（code_copy_scroll_native）と同じ型の誤り。

- 直し: 窓を論理900（窓の倍率で換算）にする。どちらの倍率でも広い配置のまま、previewの幅が変わる（96 DPIでは狭く、120 DPIでは広く）。
- 再発の防止: #839の`--code-copy-scroll-96`と同じく、`--body-discovery-96`（スレッドを96 DPIにする）とCTest `body_discovery_96_native`を足し、両方の幾何を手元でも流すようにした。
- 結果: 120 DPI 窓1125・previewの幅527→572・最初の可視行0→19。96 DPI 窓900・幅513→458・0→12。どちらも一致が可視範囲。`ctest -R`の7件成功、規約0。

手順の誤り: 統合の手順がReadyにしたままの枝へ、Draftへ戻す前に直しをpushした（ワークフローは「Ready後のhead変更は先にDraftへ戻す」）。pushの直後にDraftへ戻し、統合の手順をやり直した。

NULの直しは試験を足していない（実際の原文にNULが入る経路を確かめていない。防御のための1行）。

## 確かめていないこと

- 実機の物理キーボードと日本語入力（変換中の↑↓・PageUp/Downは既存の`command_composing`の経路で変換へ渡す。隔離desktopの試験だけ）。
- 読み上げ（UIA）で、入力欄のまま候補が動いたことが伝わるか。選択の変更は従来どおりUIAの更新を予約するが、フォーカスが入力欄にあるので候補のフォーカスの通知は出ない。
- 変異は幅の寄せ直し（m1）だけ。↓とPageUp/Downは変異していない。↓の試験は、フォーカスが候補へ移る元の実装では`GetFocus() == input`が偽になる作りで、PageUp/Downの試験は元の実装では選択が動く作り。
