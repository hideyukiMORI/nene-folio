# ミニマップのホイールとコードブロック — #508

日付: 2026-11-01。Issue [#508](https://github.com/hideyukiMORI/nene-folio/issues/508)。
hideは#506の表示を実機受理し、レーンwheelとコードブロックの概形を依頼した。

## 変更と正典経路

`folio_window.c`はレーンの共通矩形でwheelを振り分ける。queueのroot/本文/自前childの配送先に関係なく、
レーン上なら同じroot入口へ結ぶ。`note_pane.c/h`の`note_pane_scroll_wheel`から既存#473のOS行数/ページ設定・
240ms減速・native位置反映/通知へ合流する。別のscroll位置やtimerを追加しない。
本文から親へ戻す時点では余韻を消さず、配送先が本文/レーン以外と決まった時点で`note_pane_cancel_wheel`を使う。
レーンのCtrl+wheelはスクロールであり、本文Ctrl+wheelの倍率は維持する。倍率deltaの残りはレーンへ持ち越さない。
IME/modal/非表示/未準備とdrag取消、本文/選択/Undo/dirty/focusを保つ。

`note_minimap.c`は取得済みsampleのnative背景を面と左辺へ使う。通常サイズの背景付き行をコードブロックの
概形として示し、空行でも帯を保つ。inline codeは背景なし、hrは小さい空段落、表はnative行構造を優先する。
明暗はnativeの背景/文字色を対で使い、HCでは実効paletteと左辺で区別する。現在範囲の外周と左右余白は残る。
新しい文字列問い合わせ・parser・言語分類は追加しない。256sample/16idle/120msを維持する。

`ui_text.c`の3言語の操作案内と`KEY_BINDINGS.md`へwheelを追記した。
ADR0084・0026・0067を補正。保存schema・版・依存・申請済みMSIXは不変。Waivers none。
規則: ARC-001/002/004/007/008/011/012、C-002/008/012/013/016/017/018、QLT-008/012/013、GIT-001〜004。

## 実行結果

原本は`D:/NeNeFolio/agents/minimap-wheel-code-20261101/`、一時出力は同名`outputs/`。
各実行は`tools/record-run.py`へ入力snapshot・raw出力・終了値を収めた。

| 実行 | 結果 |
| --- | --- |
| `build.ps1` / `debug-03` | 正典Debug configure/build・clang-tidy・conformance成功。ASan/UBSan/nullabilityの正典vector |
| `contract.ps1` / `contract-01` | CTest `minimap_structure` 95checks成功。fenced code3行・空行1・hr1、inline codeの背景なし、既存native構造/入力保護。恒久回帰へ追加 |
| `paint.ps1 -Stage run -Label paint-01 -Case wheel` / `wheel-run-01` | 104checks成功。root/本文/drawer宛queue、focus維持、現在範囲追随、停止・反転、直接配送の非循環、小delta、Ctrl、IME/settings/hidden、本文wheel/zoom、余白取消、drag取消、編集本文/Undo/dirty/選択保持 |
| `paint.ps1 -Stage run -Label paint-code-01 -Case code` / `code-run-01` | 20checks成功。明暗/HC/狭幅画像、256/16上限と全文取得なし、DPI矩形 |
| `paint.ps1 -Stage run -Label paint-edge-04 -Case edge` / `edge-run-04` | 10checks成功。部分Ctrl deltaの破棄、drawer二重配送なし、実drawer側の配送維持、先頭/末尾のnative境界 |

実主窓fixtureは専用desktop・合成ノートだけで実行。製品`folio_window.c`と同じcompile/link vectorとlibraryを使い、
元objectの重複linkをmapで拒否する。wheelのOS設定は読取のみ（この環境では3行）。
一刻み-120は123px、-20を6回は122pxで、小deltaが親への配送で消えないことを確認した。
本文/レーンの共通アルゴリズムは#473を維持し、OS設定を変更してページ指定を再試行していない。
95/104/20の成功後は製品コードを変えず、追加のedgeだけを実行した。

`paint-code-01/minimap-{0,1,high,narrow}.png`を目視。空行で切れない背景帯、inline codeとの区別、
HCの左辺、現在位置枠の外周と余白、狭幅非表示を確認。画像は実行環境120 DPI、96/144/192は矩形検証。

## 途中の検出

最初のbuildでは新しいテスト関数の認知的複雑度12と既存入力翻訳の11を拒否した。
code集計と検索キーの翻訳をそれぞれの責務へ分け、閾値・抑制を変更せず通した。
edgeの最初の期待は行単位Endと画素単位末尾を同一視して失敗した。実値は81959からwheelで81999、82000、
82000、82000へ収束し、最終表示行1999/2000を保った。固定回数で停止と最終行を確認するfixtureへ補正し、
スクロール実装は変更していない。途中の失敗記録も保持する。

## 限界と完了証跡

既存の最大256行を間引く概形であり、全行・全セル・pixel縮小や言語別構文色分けを保証しない。
native背景のある行の見た目を写すので、編集時もnativeに存在する書式へ従う。
実clockを使った隔離wheel入力の停止確認であり、物理ホイール・タッチパッド・IME/reader・異なるDPI間移動は別の確認。
最終headの必須CI `eng/check.ps1`、main、通常exe反映/起動、全hash保全/終了整理の確定値は同agentsの
`integration-completion.json`へ収める。通常版を強制終了しない。
