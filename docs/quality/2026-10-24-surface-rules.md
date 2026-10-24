# #351 — 面の規則の対象検証

2026-10-24。対象は `feat/351-surface-rules`。設計はADR0035/0052/0058/0060、規則はARC-001/004/011、C-002/008/012/017/018、CNF-009、QLT-008/012/013。
途中状態は[同日のチェックポイント](2026-10-24-surface-checkpoint.md)を参照する。本記録の対象検証と最終PR headの必須CIは区別する。
schema・実行時依存・gate閾値/除外の変更なし。音分類表だけを既存CNF-009の検査対象へ登録した。Waivers: none。

## 検証入力と再利用

- native名前入力/失敗箱/dialog_theme/ui_partの入力はチェックポイント `f1ec547` から不変。入力source・lib・object・font・fixture・SHA256を `D:/NeNeFolio/agents/351-surface-resume/native-validation/` に固定した。
- 失敗箱のCRLF修正 `127ba50` 後はcoreを再linkして該当9面だけを再確認した。テーマの英中Esc案内修正は `a37c123`。
- 既成功の `folio_tests --surface-rules`（theme contrast・文言・NAME_UNCHANGED）と音分類63値/20件・CNF-009の欠落拒否は、関係入力が不変な範囲で再利用する。
  push・担当交代・レビュー・統合だけを理由に再実行しない。
- CommandLayerの初期完成候補は `f59bdb8`。その後に最小本文高、tooltip、Helpの描画費用・S2の配置・UIA順/Name・脚釦幅を修正したため、それらの境界を候補へ更新して確認する。

## nativeの配置・入力・UIA・音

詳細と実コマンドは `native-validation/report.md`、入力は `input-manifest.json`、集計は `observation-validation.json`。

| 対象 | 実測と範囲 |
| --- | --- |
| 名前/失敗面 | 名前7mode（6種＋retry）×3言語×render DPI120/144/168×owner3寸法の189面と失敗27面。216面の子包含・外形・合法caption/sysmenu・NC除去を確認。実OSは120DPIで、物理モニタを移動した試験ではない |
| 名前の検証 | 空名、ノート/カテゴリの同名拒否で面と入力を保持。readonlyカテゴリの全文・全選択・横送り、カテゴリ一覧後方への到達を確認 |
| 標準キー | IsDialogMessage経由のTab・Enter・Esc・×・AltF4、無題のSave/Don't save/Cancel順と3言語Alt+S/D/C。mnemonics 246 checks、validation 207 checks、各exit 0 |
| OS/IME | Alt+Space→OS Moveの1往復と移動を確認。native IME contextと合成WM_IME_START/ENDCOMPOSITION中の承認/取消抑止を確認。物理IME操作は含まない |
| 保留改名のretry | typed persistence portで実applicationの保留intentを作り、実retry入口から再試行31 checks、exit 0。実filesystem上の改名回復は既存機能で本試験の対象外 |
| fresh native UIA | managed UIAを読み込まない別のnative COM client4個、各exit 0。Name/Value/readonly/Invoke、mnemonic除去、余計なTitleBarなしを確認。Englishでの観測で、実readerやイベント配送の保証ではない |
| 失敗文 | 操作名と理由の間をCRLFへ修正後、3言語×3render DPIでEM_LINEFROMCHARが0/1。長い理由22行もnative EDITのscrollで到達。OS fallbackへ同じ理由を渡す |
| alert | 製品のEVENT_SYSTEM_ALERT発行1回、再表示通知で増えない。OS MessageBox内部やreaderへの配送回数を測ったものではない |
| 画像/DWM | 25組のBMP/同画素PNGを保存。DWM extended frameのHRESULT 0、bounds、corner=1を記録。PrintWindowは窓外の影を含まない |

元のmatrix実行は4509 checks中243 failuresで**exit 1**。216件はDefWindowProcだけの合成ownerに「元childへ必ずfocus復帰」を求めたfixture期待、27件はreadonly retry EDITへIME contextを必須とした期待である。
配置等の4266観測とこの失敗を分けて保存し、matrix全体をexit 0と書き換えない。製品owner/callerの復帰は別の統合検証で確認する。
fresh UIA Close後に合成ownerのGetActiveWindow/GetFocusがNULLになる1条件も、この統合境界へ引き継いだ。

WASAPI default-render loopback（48kHz/2ch/float32）で基準区間と7caseを各2.5秒採取した。native/clientは各exit 0。
基準・前提拒否・OOMはpeak/energy 0。I/Oのnative、OS fallback、native→後段fallbackは同じ波形で、peak 0.063903496、energy 19.3292800206。
非zero区間122666 samplesのSHA256は3caseとも `b0bcf1b32e8453d8cb414244be0779f17f1ab3538899c74fee522e46c499b310`。
この環境では後段fallbackの追加音は無かった。物理音量や別のOSサウンド設定の一般保証ではない。

元active desktopのforegroundはbefore/afterとも659006。専用desktopと合成dataだけを使い、SetForegroundWindow/SendInput/SendKeys/SwitchDesktopは使っていない。
初期fixture自身の寿命誤り・OS fallbackのwatchdog timeout・compile/link失敗は原ログに保持し、製品の成功記録と混同しない。

## 設計診断で見つかった欠陥

- 失敗箱のLF区切りでは操作名と理由が連結して見えた。唯一の文言表をCRLFへ直し、unit期待値とnativeの実表示行で確認した。
- 最小BOTH560×360の本文予算に共通ヘッダー44が入っておらず、REPLACE本文が0高になった。ADR0060の短窓規則で必要分だけ余白を減らし、題・操作・理由全文・文字周囲の8を保持する。
  実OS120DPIのC案診断ではH1のTOM top/baseline/bottomが0/74/93、実inkはy44..82。本文95pxに文字とdescentが収まる。元の行送り103には段落間の空白が含まれていた。
  3言語のNoto Sans JP/SC・22pt太字のGDI metricとWM_PRINTCLIENTのinkを照合、14 checks/0 failures。これは完成候補の帯契約確認と分けた設計診断である。
- Helpは各行の位置を照会するたびに先頭から折返しを測り直していた。同じ描画境界のQPC単発測定は326.024msから13.694msへ短縮。
  幅/DPI/font/言語によるoffset・総高・最大行高をarrangeで一度導出し、描画・UIA・ページ送りで共用する。一般的な性能保証や無関係な一覧性能の測定ではない。
  初期単発の原ログが後続runで上書きされたため、固定git sourceの旧/新を3sampleで比較した原ログを保持する。中央値は342.2795ms→12.0012ms。
  比較するfolio_window以外のheader/libを共用した制御実験であり、過去の製品全体を再buildした比較ではない。
- 最小幅のHelp前後釦44では文言が省略されたため各88へ変更。S2の文字を中央へ揃え、視覚順にUIAのTABS/List/Statusを並べ、面のNameを描画と同じHelpへ揃えた。
- 共通×/前後釦のtooltipを既存の単一ownerへ登録。標準Tooltip API/通知/矩形/activationの35 checksは成功。実カーソルでpopupを表示した試験ではない。
- 実製品の設定保存失敗では箱をEscで閉じるとroot、UIAの×ではNULLへfocusが移り、本文保存失敗でもUIA×後にNULLとなった。owner有効化と暗幕復元は成功しており、アプリ内の入力先復帰が欠けていた。
  `2b612d8`の5行修正で名前/失敗wrapperが元focusをlocalに捕捉し、追加の資源失敗通知まで終了後に既存`restore_valid_focus`へ渡す。個々の設定の修復処理や第2の永続focus状態は足していない。
- Help cache導入後の実WM_PAINT順を外部subclassで調べ、幅resizeは新cacheで描画する一方、font再生成→arrangeはcache更新後のpaintが0回になる条件を確認した。
  配置/計測の最後に既存の再描画予約へ合流し、描画時に計測を戻す修正や別の更新loopを作らない。

## 独立レビューとデザイン確認

read-onlyの独立レビューは `f59bdb8` 対 `80e2814` を対象とし、英中テーマ案内のEsc Close/关闭と実動作の不一致1件を確定した。`a37c123`でBack/返回へ修正した。
報告は `D:/NeNeFolio/agents/351-surface-resume/review/review-f59bdb8.md`。
後続 `b7c246e` のcache・垂直予算・tooltip・UIA差分、および `2b612d8` のfocus復帰5行をそれぞれ独立に読み取り、確定指摘0件。
`review-b7c246e.md` と `review-2b612d8.md` に、コード上の結論と実GUI/最終CIを実行していない範囲を分けて残した。

hideの現在の明示分担に従い、設計判断は対話席、実装/対象検証/独立レビューはSOL、検証資産の読み取り調査はLUNA、見た目の議論はClaudeCodeのデザインリナが担当した。
リナは `/design:design-critique` で設計と実画像を読み取り専用で確認。設定の日本語行が部分的に見える画像は、末尾の行番号設定へcursorを動かした撮影条件で、現在言語の印とcursorを区別する。
議論は `design-rina-result.json`、`design-rina-discussion.json`、`design-rina-band.json`、`design-rina-final.json`。
最後の5枚（最小SEARCH/REPLACEのroot全体、狭幅Help/設定、通常幅Theme）で前回の重大懸念の解消を確認し、見た目のブロッカーなしと判定した。
未測定の影・reader・DPIを画像レビューの成功に含めない。

## 完成候補の確認

| コマンド・対象 | 結果と範囲 |
| --- | --- |
| 固定toolchain `cmake --build D:/NeNeFolio/outputs/351-surface-rules/a/build --target nenefolio folio_tests -j 4` | `b7c246e`のDebug compile/tidy/link、6/6、exit 0。`build-completion.log` |
| 同buildの `folio_tests.exe --commands-ui` | exit 0、`final-text-unit-evidence.json`にexe/source/test SHA256を記録。後のfocus5行はunit入力に影響せず再利用 |
| 同buildの `--target nenefolio -j 4` | `2b612d8`のfocus5行後、Debug compile/tidy/link exit 0。`build-native-focus.log` |
| 帯の操作契約 | 24 checks/0 failures。literal検索・regex置換、anchor以後の1件・全件、dirty、1回Undoで原文/clean、CtrlSで欄維持/保存/旧失敗消去、Esc/×で本文矩形/選択復帰、本文CtrlH caret、論理行gutter |
| 帯のgeometry | 95 checks/0 failures。3言語×3render DPIの最小SEARCH本文95/114/133px、REPLACE40/48/56px。結果だけでgeometry不変、通常高で16/32余白へ復帰、360→480の連続resizeで本文高は非減少 |
| 設定の選択reveal | 48 checks/0 failures。可選択の5項目×3言語×3render DPIで操作中の行が全高で見える。現在言語の印とは区別 |
| Help操作の到達 | 146 checks/0 failures。wheelで現在利用可能な23候補すべてへ到達、UIA Scroll 100%と各SelectionItem.Selectで末尾を含む全候補をreveal。catalogの25ID自体は不変 |
| Help cache/ページ | `fixture-help-final-run.log`で101 checks/0 failures。S2・ページ境界・viewportより長い項目の全文・cache失効・測定失敗時の閉鎖を確認。旧11 checks/2 failuresと黒PNGは、language変更が面を閉じる既存仕様をfixtureが見落とした条件として保持/分離 |
| native深さ/失敗注入 | 更新wrapperで17 checks/0 failures。外部source-including fixtureのsignature一致OS注入でdepth2→1→0、全client→現面の穴/Z順、準備失敗の業務前OOM、元理由保持、復元失敗hide/非再帰/通知1回/業務結果保持、追加通知後のfocusを確認。実exe blackbox成功とは分ける |
| 計測後のpaint予約 | `c27aca4`の1行修正後、実WM_PAINT順の5 checks/0 failures。幅変更とfont再生成の両方で更新後cacheによる最終paintを確認。nenefolioだけの対象Debug compile/tidy/linkもexit 0 |
| 規約/format | C/H変更箇所のformat成功、実CMake File APIを含むConformance 0 violations。最終PR headの全ゲートを代替しない |

製品候補のexe SHA256は `b7c246e` が `46ed024a7eaa0f76cdb72dce9298b73d2e94faed4b787997a6560c7433c1b459`、
focus補正後 `2b612d8` が `d28f59f4f0070899556b77a6a742ae1261988378425cee528a5c69c7e956cab1`。
最終製品source `c27aca478428036c0d2645f6dbe09b4a59586db4` のexeは `2cab53515d0ee855dd86c14d2a42742f4d6326948a5f2f5155bc690d3ad22453`。
最後の1行は既存InvalidateRectの予約だけで、native wrapper/focus/region状態・geometry・文言は不変。直前のowner142 assertionsと画像はその範囲で再利用した。
この差分は対話席が既存helperまで読み、焦点・状態の変更がないことを自己レビューした。軽微な1行に別の独立レビューを一律追加していない。

後者の実製品owner/caller検証は、settings失敗UIA 29、settings失敗keyboard 28、本文保存失敗UIA 22、F2名前入力18、Ex名前入力23、Ex新カテゴリ確定22の計142 assertions、6caseすべてdriver/product exit 0。
native後は元のCommandLayer、本文からの名前入力は本文へ戻る。設定Esc/Save Invoke後のroot復帰は、同じUIA入口でnative箱を挟まないbaselineと一致し、NULLを解消した。
UIA Invoke前の本文を無条件の復帰先とした初期fixture期待を、baseline観測と分けて記録した。Ex新カテゴリ確定は既存どおり面を閉じ、新しい索引行へfocusする。
実製品のowner disable/enable、全client暗幕、現surfaceのregion/visibilityへの復元を確認し、内部depthの値をexe外から読んだという主張はしない。
最終6case中5caseは元desktopのforegroundが同一。1caseとcase間で変化があり、不変とは書かない。所有製品は専用desktopで、SwitchDesktop/SetForegroundWindowは使っていない。

外部fixtureの再現手順・入力manifest・画像監査・process0の記録は `implementation/report.md` と `manifest.json`、
実exeのowner検証は `native-validation/owner-integration/report.md` と `after-2b/completion.json` に収載した。
PrintWindowではlayered childやdisabledなRichEditの背面が実displayと異なる場合があり、暗幕のalpha合成や窓外影の一般保証にはしない。

統合は [PR #363](https://github.com/hideyukiMORI/nene-folio/pull/363) の最終headに対する `pwsh -NoProfile -File ./eng/check.ps1` 必須CIを条件とする。
CI/統合状態はPR Checksを正本とし、対象検証だけからReady・統合・配布反映の成功を主張しない。

物理IME、実reader、物理DPI120→144→168の移動、DWM窓外影は未測定。既存の10,000件一覧性能も本件では測定/改善していない。
