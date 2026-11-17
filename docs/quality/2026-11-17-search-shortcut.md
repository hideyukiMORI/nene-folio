# 全体検索のキーの品質記録 — #855

2026-11-17。hideの依頼「左上の全体検索にもショートカットキーが欲しい」。
作業木E:/NeNeFolio/worktrees/855-search-key（branch feat/855-discovery-shortcut）、対象実行の原本E:/NeNeFolio/outputs/855-search-key、
完了後の置き場D:/NeNeFolio/agents/search-key-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 現状の確認

- 一覧（左）の頭の「すべてのノートを検索」の欄には既に`Ctrl+Shift+F`があった（`folio_shortcut.c`の`FOLIO_SHORTCUT_ACTION_FILTER`）。
  ただし欄の中の案内文は「すべてのノートを検索」だけで、キーは画面に出ていなかった（ノート未選択時の案内とF1の一覧にだけ出る）。
  UIAのAcceleratorKeyも、この欄には空だった。
- 「ノートを探す」（名前／本文の検索面）には直接のキーが無く、操作メニューのQかパレットからしか開けなかった。
- hideの言う「左上の全体検索」は、両方の表示で左上にある「すべてのノートを検索」の欄と読んだ。どちらを指しても見つけやすくなるよう、両方を直す。

## 決めたこと

- キーの正本は一つの表`folio_shortcut.c`のまま。そこへ`Ctrl+O` → `FOLIO_COMMAND_DISCOVER_NOTES`を1行足した。
  主窓のアクセラレータ・操作メニューのキー表示・F1の一覧（`key_guide.c`）・UIAは、この表から作られるので第2の経路はできない。
  `Ctrl+O`は未使用（`Ctrl+P`はパレット）。Obsidianのクイックスイッチャーと同じキー。
- 一覧の検索欄が空のあいだ、案内文の右端に`Ctrl+Shift+F`を同じ薄い色で示す。案内文と両方が1行に入らない幅では出さない（文字を重ねない・切らない）。
  キーの文字は同じ表の`folio_shortcut_action_hint`から取る。
- 同じ欄のUIA AcceleratorKeyにも`Ctrl+Shift+F`を出す（ADR0060の決定5「AcceleratorKeyはtooltipと同じキーの文言から写す」と同じ経路）。
- 保存schema・設定・版は変えない。

## 試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| build-01 | clang-tidy: 案内文の右端のキーを描く関数の引数5（上限4）。案内文の寸法を呼び出し側で測って渡す形にした |
| unit-02 | 1件失敗: UI文言の同一性試験が全IDの期待値を持っており、新しい`UI_TEXT_KEY_DISCOVER`が無かった。期待値の表へ`Ctrl+O`を足した |
| native-discovery-01 | 成功（226 checks）。本文から`Ctrl+O`で検索面が開き（開く前は非表示、開いた後は3件）、閉じるボタンで閉じる |
| 実画面1（visual-run-01） | 既定の一覧の幅では、案内文とキーが1行に入らず、キーが出なかった（日本語・英語とも）。「入らなければ出さない」の規則どおりだが、既定の幅で見えないなら依頼に応えない |
| 実画面2（visual-run-02） | 空の欄では右端の「×」（消去）が出ないので、その余白もキーに使い（右の余白は左と同じ幅）、案内文との間を行の高さの半分にした。三言語×明暗の6枚で、既定の幅に収まって見えることを確かめた（`visual/search-key-*.png`） |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/855/verify.ps1 -Tag v1`、#854統合後のmainを取り込んだ後）。

| コマンド | 結果 |
| --- | --- |
| `cmake --build <E:build> --parallel 4`（全target・clang-tidy込み） | 成功 |
| `python -B eng/coverage.py` | 成功。分岐90.88%（下限90・除外不変） |
| `python -B eng/symbols.py --build-dir <E:build> --require core application` | 成功（違反0） |
| `python -B eng/conformance.py` / `--build-dir <E:build>` | 成功（違反0） |
| `ctest --test-dir <E:build> --output-on-failure --no-tests=error` | 61件すべて成功（89.3秒） |
| `git diff --check origin/main` | 成功 |
