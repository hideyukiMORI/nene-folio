# UIの同期計測と一覧描画の性能 — 2026-10-25（#368）

Issue: [#368](https://github.com/hideyukiMORI/nene-folio/issues/368)。基点mainは`936a1332c180502277c4d4552f8de1da68e5b883`。
2026-10-24 23:23 JSTのhideの依頼から開始し、日付をまたいで完成候補を確認した。
規則: ARC-001/004/005/011、C-008/012/013/016/017、QLT-008/012/013、GIT-001〜004。判断は[ADR0062](../adr/0062-ui-measurement-and-visible-paint.md)。

## 変更と原因

`folio_window.c`の折返しは、文字列を1文字ずつ伸ばした全prefixをGDIで測っていた。設定初回の約517ms中、約488msをwrapped_textが占めた。
設定/テーマは同じ失敗理由の全件予約高を、面を切り替えるたびに捨てていた。Help/検索帯にも同じprefix計測があった。

- 同じ`DrawTextW(DT_CALCRECT)`経路で、まず明示改行までの全体を測る。あふれたときだけ実測幅から次の候補位置を見積もり、実測済みのfit/fail境界を狭める。
  予測は測定順だけに使う。最終的なword境界・末尾空白trim後の行も実測し、文字を失わず進む。幅0以下は既存の表示不能結果へ返す。
- `command_surface_metrics.h`の一つの派生cacheが、共通noticeと両面のfooterを保持する。幅/DPI/font/言語、font再生成時の明示失効は維持する。
- `drawer_window.c`は既存row矩形が描画clipと交差する行だけを描く。上端/下端の部分行、coreの配置/scroll/hit/drop、独立したdrop線とfadeは維持する。

保存schema、core/application/adapters、実行時依存、ゲート閾値/除外は変更しない。Waivers: none。

## 測定方法

恒久証跡は`D:/NeNeFolio/agents/performance-20261024/`。`verification-inputs.json`がsource/exe hash、CSVが生標本、
`ui-performance-results.json`と`drawer-performance-results.json`が集計を持つ。開始時の不完全ログと中間の二分探索版も分けて保持した。

外部fixtureが製品UI sourceと製品libraryを使い、専用desktopのWin32へ同じ入力を渡す。時計/計装/試験口を製品へ加えていない。
UIは29関数の透過wrapperでinclusive/exclusive時間を取り、入力関数からメッセージ排出とGdiFlushまでを計る。
物理クリックやDWMの実display呈示時間ではない。実OS DPIは120、Debugは/Od・sanitizers、Releaseは/O2。
UI fixtureは空状態と、約500行/14KiBの合成MarkdownをVIEW/EDITで使用する。各操作5回、初回と中央値を分ける。
計時中に本件のcompilerや別native fixtureを走らせていない。単一マシンでの診断であり一般的な性能保証ではない。

一覧は100カテゴリ×10/100ノート、各本文10,240bytes。先頭/127px scroll/末尾の各10回、計30回の中央値。
core配置の作成とWM_PAINTを別々に測る。新規作成したファイルの初読と、既に読み込まれたファイルの値をI/O改善として比較しない。

## Releaseの比較

| 操作 | 変更前ms | 変更後ms | 条件 |
| --- | ---: | ---: | --- |
| 設定を初めて開く | 541.5 | 97.2 | 空状態、各1標本 |
| テーマへ切替 | 492.9 | 20.8 | 空状態、5回中央値 |
| テーマから設定へ戻る | 507.9 | 22.9 | 空状態、5回中央値 |
| ノート内検索を初めて開く | 789.5 | 173.4 | VIEW、各1標本 |
| Helpのキー説明へ切替 | 204.9 | 31.6 | 各1標本 |
| Helpのキー説明を再描画 | 87.3 | 18.0 | 各1標本 |
| VIEWでテーマ候補を1つ進める | 43.0 | 39.3 | 約500行、5回中央値 |
| EDITでテーマ候補を1つ進める | 18.9 | 16.0 | 同本文、5回中央値 |
| 1,000件一覧の描画 | 29.7 | 1.47 | 30回中央値 |
| 10,000件一覧の描画 | 235.9 | 1.52 | 30回中央値 |

Debugでも初回設定516.8→91.7ms、テーマ切替515.7→25.8ms、初回検索1005.1→193.1ms。
一覧のcore配置は10,000件で0.177→0.180ms。全件配置は残っており、一覧処理全体が可視件数だけになるとは主張しない。

## アプリ全体の監査と残る費用

| 領域 | 確認したこと・判断 |
| --- | --- |
| 設定/テーマ/Help/検索・置換帯 | 共有の折返し計測が主因。今回の変更で削減。結果文や入力だけでは固定予約高を変えない |
| 一覧 | clip外へも全行の文字変換/GDI描画を発行していた。今回の可視行制限で描画の件数依存を大きく削減 |
| テーマのVIEW本文 | 約500行でRichEditへの再投入が残る。Releaseの候補移動は約39ms。書式/選択/失敗時の復元と別の再着色経路を増やす変更は今回行わない |
| EDIT本文・未保存判定 | 既存revision/body_touched cacheで未変更時の全文比較を避ける。`note_pane.c`とapplicationは不変。Undo/dirty/保存モデルを高速化のために弱めない |
| 全ノート検索のwarm計算 | 10,000件のalphaは約2.2ms、非一致zは約51ms。#328/#331の二分探索/部分一致改善が現在も同一sourceで存在する |
| 全ノート検索の初回本文読込 | 同期全件読込が残る。新規fixtureの最初は1,000件15.08秒/10,000件136.96秒、同ファイルを読んだ後の別processは86/847ms。OS cacheの強制cold測定ではなく、差の原因は未確定。I/O/core/adaptersは不変であり今回の短縮効果とはしない。[#369](https://github.com/hideyukiMORI/nene-folio/issues/369)へ分離 |
| 起動/書体登録 | UI構築、state読込、font登録を別に記録。書体登録は33〜490msと初回条件で変動し、同梱書体の検証/登録を省略しない |
| 行番号・巨大本文 | 既存の論理行index再構築は残る。4MiBのキー入力を今回再計測したとはしない。過去の[測定](2026-10-15-external-review-performance.md)は時点付き参考に限る |
| UIA/保存/履歴/ファイル操作 | UIA通知/名前更新や保存のdurabilityを無効化しない。所有者/閉じた結果/保存後修復を読み、今回の描画差分と無関係な全件回帰は追加しない |

NeNeNibはmain `605f21f968bba7a2827a47c257952c12444f2edd`を読み取り専用で参照した。
`src/ui/win32/EditorWindow.cpp`のfollow_look/deliverは色とfont/配置の変更を分け、repaint要求をまとめる。
`Direct2DRenderer.cpp`のstatus_text_layoutはtext/format/areaで配置を再利用し、色をkeyに含めない。
`EditorController.cpp`のshown_settingsは既存候補からpreviewを導く。Folioでは同じ方針をGDIの既存経路へ適用し、Direct2D移植や新しい設定正本を作っていない。

## 対象検証

| コマンド/fixture | 結果 |
| --- | --- |
| `cmake --build .../after-debug/build --target nenefolio -j 4` | Debug compile/tidy/link exit 0。`after-final3-debug-build.log` |
| `cmake --build .../after-release/build --target nenefolio -j 4` | Release compile/tidy/link exit 0。`after-final3-release-build.log` |
| `run-commandlayer.ps1 -Mode wrap -SkipBuild` | 15,968 checks/0 failures。全UI文言×3言語×3render DPI、幅-1/0/24/80/288/568、CR/LF/CRLF、空白/タブ、非BMP/結合文字。採用行幅、文字進行、surrogate境界、高さを確認 |
| `shared-metrics` と補正 `shared-width` | 3言語×3render DPI×2幅の共用/通知高/font失効は成功。初回98件中1件はfixtureがBOTH最小幅制限を見落としたwidth期待。97成功は再利用し、実際に狭くするDrawer320条件を3 checks/0 failuresで追加確認 |
| `help-final` | 101 checks/0 failures。UIA SelectionItem/Tab、ページ全体到達、項目がviewportより長い場合、cache失効/取得失敗 |
| `band-geometry` | 95 checks/0 failures。3言語/3render DPI、最小本文高、通知だけでgeometry不変、resizeの非減少 |
| `settings-scroll` | 48 checks/0 failures。選択可能な項目を完全な行としてreveal |
| `representatives` | 8 checks/0 failures。最小SEARCH/REPLACE・Help/設定/Theme画像を保存し、5枚を実際に目視 |
| 一覧のbefore/after | 1,000/10,000件×先頭/途中/末尾の6枚、RGB差0。使用しないDIB alphaは比較対象外 |
| `build-parity.ps1` のnative fixture | 12 checks/0 failures、exit 0。選択/cursor、3scroll位置、drag挿入線有無、高コントラストで基点の全行描画と現描画の画素差0。hit/dropは同じcore配置から取得。物理drag操作の試験ではない |
| `python eng/conformance.py --build-dir .../after-debug/build` | CMake File API query/reconfigure後に0 violations。最初のreply欠落は設定不足としてログを保持 |
| 変更C/Hのclang-format、`git diff --check` | 成功 |

途中の補間helperはC-012の複雑度11/引数5で拒否された。候補を区間内へ制限する関数へ分け、4引数以下/複雑度10以下に直した。閾値や抑制を変えていない。
ASan起動時の既知interception診断はraw stderrに保持し、対象workerにsanitizer違反はない。

read-only独立レビューは設計と最終3製品差分を確認し、追加の確定指摘0件。親が実装/検証を直接担当した。
最終`folio_window.c` SHA256は`75051322fcf5ce9abc791b47c0b4d508e673d89f696ac89ded6d757da79519b4`。
PR最終headの必須`eng/check.ps1`成功、統合、通常試遊への反映、整理はPR Checksと`completion.json`を正本とする。
物理IME・実reader・物理DPIをまたぐ移動・DWM窓外影は今回未測定。公開Releaseは作成しない。
