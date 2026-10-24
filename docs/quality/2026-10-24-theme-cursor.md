# #364 — テーマ候補カーソルの表示回帰

2026-10-24。hideの通常試遊で、テーマ一覧のホイール/カーソル操作が反映されず固まったように見えるとの報告を受けた。
対象規則はARC-001/004/011、C-008/012、QLT-008/012/013。Waivers: none。schema・依存・gateの変更なし。

## 原因と修正

main `1567439` を使った隔離Win32 fixtureでは、検索EDITにfocusを保ったまま候補indexが0→1→2→3と動いていた。
しかし4回の製品描画のpixel hashはすべて `8140155158738144501` だった。候補枠がkeyboard cueと
`GetFocus()==command_layer`の両方を要求し、検索EDITからの操作では見えなくなっていた。
同じ描画境界の所要時間は約2〜4msであり、今回の再現は処理停止ではなく表示回帰だった。別条件の遅延を否定する測定ではない。

`folio_window.c`で候補枠を既存`command_selection`だけから描く。`theme_row.h`の表示値を`focused`から`cursor`へ改名し、
native focusと混同しない。共通`ui_part`の内側2px枠と既存contrast色を使う。採用印は既存の採用済みテーマにだけ付ける。
ADR0060決定26へこの区別を明記した。検索EDITのfocus/IME、UIAの選択と真のfocus、テーマ適用経路は変更していない。

## 対象検証

証跡と再現用の機械検査は `D:/NeNeFolio/agents/theme-navigation-20261024/`。
`run-probe.ps1`が現在の製品`folio_window.c`とfixtureを一つの翻訳単位へ連結し、製品のlibrary/font/通常Win32手続きを使う。
製品へテスト用入口は追加しない。専用desktopで動かし、通常試遊のプロセス・dataは操作しない。

| 実行 | 結果 |
| --- | --- |
| `run-probe.ps1 -Label regression-before`（main/通常build） | 140 checks、24 failures、probe exit 1。候補枠の画素と操作前後の画像不変が失敗。原ログを保持 |
| `run-probe.ps1 -Repo D:/NeNeFolio/worktrees/364-theme-cursor -Build D:/NeNeFolio/outputs/364-theme-cursor/build -Label after` | 同じfixtureの140 checks、0 failures、exit 0 |
| 固定toolchain `cmake --build D:/NeNeFolio/outputs/364-theme-cursor/build --target nenefolio -j 4` | Debug compile/tidy/link、68/68、exit 0 |
| `python eng/conformance.py --build-dir D:/NeNeFolio/outputs/364-theme-cursor/build` | 0 violations、exit 0 |
| `clang-format --dry-run --Werror --style=file src/ui/win32/folio_window.c src/ui/win32/theme_row.h`、`git diff --check` | exit 0 |

画素検査は現在候補の内枠位置に製品の`focus_item_line`が描かれることを確かめ、内部indexの一致だけでは成功としない。
EDIT宛/一覧宛のwheel、↓、PgDn/PgUp、部分検索、0件、Enter採用、Escで設定へ戻る操作を確認。
IME合成メッセージ中は移動を抑止し、候補UIA viewはselectedでもEDIT focusをcandidate focusedへ偽装しない。
3言語、通常960幅とDRAWER320幅で候補移動前後を描画し、明/暗配色を確認。実OS DPIは120で、物理DPI移動試験ではない。
候補はviewportへrevealされ、ページ送りでスクロールする。採用済み値はEnterまで不変。

修正後の先頭4枚は各操作で異なるpixel hash、所要時間は2.1〜2.6ms。通常/狭幅の画像を目視し、
採用済みの印と移動中の輪郭の区別を確認した。全配色のcontrast選定と高contrast描画経路は変更しておらず、#351の成功証跡を再利用する。
実OS高contrast設定を切り替えた確認、物理IME、fresh COM client/実readerのイベント確認は今回実行していない。

入力source/fixture/exeのSHA256は`probe-*-inputs.json`、各実行ログとBMPは対応するラベル名で保持する。
初回のsource-includeコンパイルはheader扱いによるnullability completeness警告で失敗し、製品と同じ翻訳単位への連結で解消した。抑制は追加していない。
probe stderrにはASanの`interception_win: unhandled instruction`起動警告がある。実行は完走したが、すべてのinterception成功を主張しない。

## 統合と再発確認

候補カーソルを変更する際は、上記機械検査の「EDIT focus中のwheel/↓で候補枠の画素が変わる」を再実行する。
このnative fixtureは外部の対象検証であり、CIへ追加した自動gateとは扱わない。内部状態だけを調べるunitで見た目を代替しない。
最終PR headの`pwsh -NoProfile -File ./eng/check.ps1`必須CIが統合条件。実行結果・main反映・起動・整理は
`D:/NeNeFolio/agents/theme-navigation-20261024/completion.json`と[#364](https://github.com/hideyukiMORI/nene-folio/issues/364)に結び付くPR Checksで確認する。
