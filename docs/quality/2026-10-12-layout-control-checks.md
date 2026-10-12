# #292 — 表示3択・直接キー・標準tooltipの確認

- 日付: 2026-10-12。基点main `913bc41`、枝 `feat/292-layout-controls`。
- 設計: [ADR0048](../adr/0048-layout-controls-and-native-tooltips.md)。FR-039、ARC-001/002/004/007/011、C-002/012/018、CNF-010/011、QLT-008/012/013。
- 証跡: `D:/NeNeFolio/agents/292-layout-controls/`。Waivers: none。

## 対象とレビュー

表示3択の共通枠と選択下線、Ctrl+Shift+1/2/3、tooltip/操作メニュー/パレット/F1の説明を対象にした。
ボタンの短い文字とコマンドの説明をui_textの3言語表で区別し、既存layout_buttons/HACCEL/dispatcherへ接続する。
標準Tooltip controlのため、CMakeとarchitecture.jsonの正典経路へOS同梱comctl32を加えた。
保存schema版3、表示状態の寿命、既存の本文保持・幅復元・公開済みReleaseは変えていない。

設計席が全差分を確認し、別席が読み取り専用でnative資源寿命・通知・geometry・入力境界を独立レビューした。
一覧の主窓にIME変換状態の追跡がないP2を検出し、WM_IME_STARTCOMPOSITION/ENDCOMPOSITIONの追跡を新表示キーのguardへ加えた。
DefWindowProcによる通常IME処理と既存の他キー契約は維持した。修正後の独立レビューで未解決の重大/中程度の指摘はない。
320 logical pxではgroup左72、区画左74から54×3、設定左250、閉じる左280。描画/hit/tooltipは同じ矩形を参照する。

## 対象検証

| 確認 | 実コマンド/範囲 | 結果 |
| --- | --- | --- |
| 整形とDebug build | `pwsh -NoProfile -File D:/NeNeFolio/agents/292-layout-controls/check-candidate.ps1`、続いて`check-final.ps1` | 変更C/Hのformat、File API query付きconfigure、`cmake --build D:/NeNeFolio/outputs/292-layout-controls/candidate-20261012-2115/build --target nenefolio folio_tests --verbose`成功。clang-tidyを含む |
| 規約 | `python eng/conformance.py --root D:/NeNeFolio/worktrees/292-layout-controls --build-dir D:/NeNeFolio/outputs/292-layout-controls/candidate-20261012-2115/build --today 2026-10-12` | 0違反 |
| 対象unit | `D:/NeNeFolio/outputs/292-layout-controls/candidate-20261012-2115/build/folio_tests.exe --layout-modes` | 表示状態、コマンド、UI文言の検査成功 |
| 最終UI build | `cmake --build D:/NeNeFolio/outputs/292-layout-controls/candidate-20261012-2115/build --target nenefolio --verbose`（`check-offset-console.log`） | ABI補正後のwindow差分build/tidy、conformance 0違反。変更のないunit結果を再利用 |
| 実OS | `pwsh -NoProfile -File D:/NeNeFolio/agents/292-layout-controls/run-native.ps1` | 最終候補の同一process HACCEL/Tooltipと関係境界を494 assertions、0失敗、exit 0で確認 |
| 差分 | `git diff --check` | 成功 |
| 入力 | `candidate-inputs-final.json` | 新規ファイルを含むsource/tests/eng/CMake/font等のhashを保持。統合時に同じ入力を照合し結果を再利用する |

初回buildはexecute_layout_acceleratorの入れ子が4となりC-012の上限3で失敗した。
ID不一致をcontinueする形へ整理し、root IMEの修正とまとめてwindowの差分build/tidyと未完の規約・unitを実行した。
最初のログを`check-candidate-console.log`、成功した候補を`check-final-console.log`として分離する。
最終候補exe SHA256: `16E5971C2B8B87F52C0FA6941EB2D0E5A5615276CB8EA32C5B35385E26F57097`。
初回の`C672BBAF...`とSDKマクロを直接使った候補は成功したGUI候補と区別して保持する。

## 実OS確認

初回のsame-process native workerはadapter/state/fonts生成後、folio_window_createに失敗した。専用desktopのforegroundは不変で正常に終了した。
最小診断`run-tooltip-size-probe.ps1`で、同manifestのcomctl32 5.82に対しInitCC/CreateTooltip/SetWindowPosの成功を確認した。
TTM_ADDTOOLWはsizeof TOOLINFO=72で0、SDK TTTOOLINFOW_V2_SIZE=64で1となり、どちらもGetLastErrorは0だった。
`tooltip-size-result.json`と`tooltip-size-launch.json`に保存した。ASanなしでも再現し、初回stderrのinterception警告とは分離した。
このため製品の登録と矩形更新をSDKのV2形式へ揃える。manifestや依存の増加、数値の直書きは行わない。
SDKマクロをそのまま評価した候補では、内部のNULLメンバー式をUBSanが検出して終了した（`native-stderr-v2.log`）。
C23 constexprへ直接置いても定数式として拒否された（`sdk-size-constexpr-macro-rejected.log`）。
標準offsetof(TOOLINFOW,lParam)+sizeof(LPARAM)のconstexprは同じsanitizer付きのstrict C23でcompile/実行に成功し、64を得た（`sdk-size-constant.log`）。
この式を単一constexprとして使い、実行時NULLメンバー評価とゲート抑制を避けた。
ASanのinterception警告は窓を作らない最小exeでも現れたがexit 0であり、今回のABI拒否とUBSanの実失敗とは区別する。

補正後は`native-results.json`の494 assertionsがすべて成功した。worker PID14352、exit 0、120 DPI（125%）。
`native-launch.json`で通常desktopのforegroundは67378→67378、所有窓は正常終了を確認した。
CreateDesktop/STARTUPINFO.lpDesktopだけで分離し、SwitchDesktop・SetForegroundWindow・物理キー送信を使っていない。

- 候補の実ライブラリと同じprocess/threadに、実adapter/ports/state/fonts/windowを生成した。
  スレッドのkeyboard stateを一時設定して実folio_window_translate/TranslateAcceleratorへCtrl+Shift+1/2/3を渡し、各表示状態を確認してkeyboard stateを復元した。
  新キーをWM_COMMANDの直接送信だけで代用していない。
- root/本文/絞込/コマンド/置換欄のIME開始で抑止、終了で復帰すること、設定の抑止と実際の名前入力モーダルの無効owner境界を確認した。
  Ex・パレット・検索・絞込・置換から既存コマンドへ合流して入力面が閉じ、可視区画へfocusを戻した。削除確認は静的guard確認の範囲。
- 長い名前付きdirty本文のUTF-16 SHA256、変更印、Undo可否、selection、正規化した可視論理行と、md/json 4ファイルのhash/mtimeを保持した。
  最後に実Undoで元の挿入を取り消した。
- Tooltipの5つのtoolについて同一processでGETTOOLINFO/GETTEXT/HITTESTを使い、矩形・TTF_SUBCLASS・最新言語の説明・対応キー・一覧の縮幅説明を確認した。
  pointerを含むTTMメッセージを別processへ渡していない。query前後のfocus/foregroundは不変。
  物理hoverによるpopupの実表示やpopup色の目視確認は未実施であり、queryの非活性確認とは区別する。
- 日本語/英語/簡体中国語×明暗の320 logical px、通常幅の両方/ノート、日本語の狭幅パレットのPNG9枚を設計席が視認した。
  DPI対応の同一process PrintWindowで400×750または1000×750物理pxのbitmapを作り、窓の矩形と一致する記録を保持した。
  必須3択の文字、枠、選択下線、設定との間隔、パレットの説明/キーが収まっている。

`native-stderr-final.log`には上記の非致命interception警告1行が残る。最終実行にUBSan/ASanの実失敗はなくexit 0だった。
helperの最初のcompiler引数と不足headerの診断は別ログへ保持し、製品のABI/UBSan失敗と混同しない。

## 統合と範囲

最終headのフルゲートは必須CIへ集約する。変更のない#290の最大化/幅復元、設定matrix、カテゴリ永続化は再実行しない。
物理IME実入力、Windowsの個別言語キー割当、別OS/DPI、実複数画面、スクリーンリーダー、hideの目視受理は未測定として区別する。
CI、main、通常exe、成果物保持、作業木と一時出力の整理は紐付くPRと`D:/NeNeFolio/agents/292-layout-controls/completion.json`で確認する。
