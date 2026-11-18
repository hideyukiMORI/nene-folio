# ADR 0096 — 検索の装飾だけを有限frameで動かし、操作を待たせない

- 状態: 受理（production変更前の独立読取レビュー1点を反映）
- 日付: 2026-11-15
- Issue: #827
- 関連: ADR0025/0031/0032/0057/0064/0068/0084/0090/0094/0095、ロードマップ段階4
- 規則: ARC-001/002/003/004/007/010/011/012、C-002/003/005/008/012/013/014/016/017/018、QLT-008/009/010/012/013、GIT-001〜004

## 境界

検索面の入力・最終geometry・選択・結果・preview・確定・focusは従来の経路で即時に更新する。
装飾は同じ線の上だけに置き、文字、窓寸法、当たり判定、選択位置を補間しない。閉じる処理は即時のまま。
背景Claudeのdesign相談を参照し、光を行間へ走らせる案、閉鎖fade、previewのpulseは採らない。
新しい検索、本文投入、UIA要素/property/event/revisionをframeから発生させない。
全アプリへの演出拡張、時計port、About、版上げ、公開/Storeは範囲外。

## 保存選択とOSの観測

`folio_settings`の唯一のparser/writerへboolの`motion`を追加し、版7の固定末尾keyとする。
既定はtrue。旧版1〜6は従来の意味を保ってmotion=trueで読み、起動だけでは書き換えない。
版7で欠損/重複/異型/未知key/順序違いはMALFORMED。未知版はUNSUPPORTED_VERSION。
変更は不変のwith_motion、共通の設定保存、成功後採用の順。拒否/OOMで旧選択と実効値を保つ。
旧アプリは新設定を読めないことを記録する。台帳schemaと製品版は変えない。

appearance_adapterだけがSPI_GETCLIENTAREAANIMATIONを読み、appearance_snapshotへ
閉じた`appearance_motion`（UNKNOWN=0 / DISABLED / ALLOWED）を加える。
motion単独の取得失敗はUNKNOWNで、成功した配色観測を破棄しない。SPI_GETHIGHCONTRASTの
失敗は従来どおりsnapshot全体をUNAVAILABLEとし、outを変更しない。OSのSETは呼ばない。

applicationの唯一の実効判定は「保存motion=true、appearance_notice=READY、
snapshot.motion=ALLOWED、snapshot.contrast=STANDARD」の積とする。
このためOSの取得失敗、配色準備OOMで旧snapshotを保持している間も静止する。
motionの観測だけが変わる場合、既存palette比較に従ってRTFの再生成/再投入をしない。
保存値はOS通知や取得失敗から書き換えない。UNKNOWNをDISABLEDの観測成功とは記録しない。

設定面の項目名は「動きの演出 / Motion effects / 动态效果」。現在の対象が「ノートを探す」であること、
Windowsの無効/コントラスト設定または取得不能では静止になることを三言語の説明とUIA HelpTextへ置く。
実効OFFでも保存選択のtoggleは操作可能。キーやExの別toggle経路は増やさない。

## UIだけが持つframeとtimer

`folio_window`が検索装飾のframe、利用者選択の対象世代/行、単一のWM_TIMER資源を所有する。
既存の画像/minimap timerとはIDを区別する。SetTimerの失敗は演出を静止へ戻し、操作を失敗させない。
時計・乱数を読まず、16ms指定の論理tickを使う。名目時間は保証ではなく、遅延時にframeをまとめて消費しない。
有限演出に完了した後は保留queueを残さず、そのまま静止する。

既存main loopは通常messageが無い時だけ検索idleを進める。装飾timerがこれを奪わないよう、
検索work pending中のtickは最初にtimerを破棄し、そのtickでは再armしない。
既存continue_discoveryが一単位進んだ後だけ待ちを解除して再armする。
timerから検索を直接実行したり、第二のpumpや検索driverを作ったりしない。
中断/閉鎖ではこの再arm待ちも破棄する。検索workが無い有限演出だけは通常のtickを続けてよい。

| 演出 | 領域と有限性 | 起点 |
| --- | --- | --- |
| 開く | 外枠上を左上から2方向へ16frame（#928で8から2倍）で進み、最終で既存の静止枠へ戻る | 面の明示開始で一度。復帰・再描画で再開しない |
| 選択 | 新しい選択行の下線内で24 DIP以下の区間が12frame（#928で6から2倍）で一巡 | 利用者の鍵/クリック/UIAの共通選択。次の選択が旧演出を置換 |
| 処理中 | 入力欄直下の4 DIP間隔内の2 DIP線上で約1/5幅を64tick周期で巡回 | PREPARING/MATCHINGかつ進行可能が2tick続いた後 |

色は既存paletteのcurrent_text一色、太さは2 DIP以下。新しい色・点滅・ぼかし・alpha資産は持たない。
選択行の下線自体は最初から正しい位置に描く。自動的な結果世代更新/先頭選択、0件、
狭幅previewで行が不可視のときは選択演出を開始しない。旧世代の行番号を新集合へ流用しない。
最終frameは演出OFFの静止画とpixel一致する。

「進行可能」は既存のdiscovery idle実行条件を共有し、表示中/非最小化も要求する。
IME未確定、native modal、非表示/最小化、終了/本文投入の抑止中は全演出を静止へ戻してtimerを破棄する。
処理中の線は再開時に0から遅延する。文字の処理中表示は既存状態から残す。
READY/FAILED/INPUT、閉じる、破棄はその場で処理中の線を消し、不要timerを破棄する。
resize/DPI/テーマ/言語/WM_SETTINGCHANGE/アプリ設定変更では全演出を静止へ戻す。
処理中なら新geometryで走査光だけを再開できる。開く/選択の演出は再生しない。
timerが動いていない状態を監視する別timerは追加せず、既存の操作・通知・idle入口で再評価する。

## 描画経路

帯の旧位置と新位置だけをInvalidateRectへ渡す。外枠は4辺を別々に無効化する。
既存command layerのbuffered paintを共有し、別snapshot/合成window/重複rendererを作らない。
同じdraw_discovery内でDCのclipと交差しない文字/行/preview/gutterを処理しない。
paint用memory DCにも実update clipを保持し、帯だけのtickからnative previewへの問い合わせや本文生成を起こさない。
input/previewの子窓は動かさず、frameからarrange/EM_SETTEXT/EM_STREAMINを呼ばない。

## 検証と限界

schema7/旧1〜6/不正形、with_*による保持、保存拒否/確保失敗、OS三値/高コントラスト/
外観失敗/配色準備OOMの実効値を対象unitで確認する。
nativeで開く/選択の有限性、急な連続選択、世代更新、全停止入口、SetTimer終了後、
UIA意味/検索結果/文書/UndoのON/OFF同一性、最終静止pixel一致を確認する。
描画/tick配送が16msを超える条件でも既存検索idleが進み、演出だけが回らないことを確認する。
明暗/高コントラスト、三言語、最小/広幅の設定と検索面を目視する。
外側からwarm検索ON/OFF、実message pumpで静止20秒の主スレッドCPUを測る。
論理frameの画像と実配送時間を区別し、同期I/Oや低優先WM_TIMERの実時間上限は保証しない。
最終PR headの必須CI、main反映、実data保全、全試行保全、追加領域整理までを完了単位とする。
新waiver none。既存WVR-0001の範囲は変えない。

## 参照

- [SystemParametersInfoW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfow)
- [Client-area animation](https://learn.microsoft.com/en-us/windows/win32/winauto/client-area-animation)
- [WM_TIMER](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer)
- [About timers](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-timers)

先行調査と背景相談原文はD:/NeNeFolio/agents/discovery-motion-design-20261115/に保持する。

## 補足（#928・2026-11-18）: 演出の長さを2倍にする

hideの実機の感想「速くて見えない」（開く約0.13秒・選ぶ約0.1秒）と、選択のページでの判断（案B、約2倍）。
開くは8→16frame（名目約0.27秒）、選ぶは6→12frame（名目約0.2秒）。開くの進み方は元の8段の間に中間の点を足した16段。
入力・確定・閉じるは即時のまま、最終frameが静止画と一致すること、OSの設定・高コントラスト・設定OFFで静止することは変えない。
