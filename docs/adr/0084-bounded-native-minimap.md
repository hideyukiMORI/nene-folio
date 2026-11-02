# ADR 0084 — ミニマップはnative表示行の有限sampleと既存scrollで作る

- 状態: 受理（2026-11-01、hideの簡易ミニマップと設定/キー切替の依頼）
- 日付: 2026-11-01
- Issue: #502
- 規則: ARC-001/004/005/009/011/012、C-002/008/012/013/016/017/018、QLT-008/012/013
- 関連: ADR0025/0026/0056/0058/0060/0068/0070/0081

## 決定

1. 閲覧と編集の右端へ64 logical pxの簡易ミニマップを置く。行の分量を短い水平線、現在の
   表示範囲を枠と面で示す。本文の縮小像やMarkdownの意味解析を作らない。既存の主窓backbuffer、
   実効palette、DPI、矩形を描画/hit/keyboard/UIAで共有する。本文用の幅が320 logical px未満なら
   自動で隠し、設定値は保持する。ノート無し・一覧表示でも隠す。
2. RichEditだけが本文・表示行・選択・scrollを所有する。新しい`note_minimap`は最大256個の
   行長sampleと構築位置だけを持つ派生cacheで、全文・RTF・AST・文書IDを複製しない。
   `EM_GETLINECOUNT`、`EM_LINEINDEX`、`EM_LINELENGTH`で表示行を等間隔に参照する。
   長い本文では間の行を省略する概形であり、見出し/空行の完全な再現やpixel単位の縮小を保証しない。
3. EN_CHANGEは失効だけを行う。主窓が所有する一つの120ms timerで連続入力をまとめ、通常loopの
   queueが空のとき一回16sampleまで進める。timer引数以外の時計読取・背景thread・入れ子pumpを
   足さない。既存index filterのidle入口を用途共通の名前へ変え、filterを優先する。
   完成snapshotだけを公開し、途中で本文/幅/倍率が変われば構築を捨てる。同じ本文の編集中は
   直前の完成像を表示できるが、本文投入時は必ず消す。非表示/IME/modal/stream中は構築しない。
   問い合わせ失敗は今回の構築を終了し、次の失効で再試行する。無限idleを作らない。
4. 現在範囲はnativeの先頭表示行とclient下端の表示行から読む。最小18 logical pxの操作範囲を
   設け、短い文書は3 logical px/行を上限に縦へ伸ばす。clickはその位置を中央付近へ、dragは
   掴んだoffsetを保って移動する。↑↓・PgUp/PgDn・Home/EndとUIA RangeValueも同じ表示行scrollへ
   渡す。実移動前に既存wheelの余韻を止め、本文・選択・Undo・dirtyを変更しない。
5. 既存自前要素のfocus/hover/capture/入力guardへ加える。ミニマップは縦Sliderとして公開し、
   本文操作群のTabから到達できる。RangeValueのwidth専用名称をrangeへ一般化し、一覧幅も同じ
   providerを使う。数値は表示行の先頭位置、要求範囲は0から最終表示行まで。nativeが末尾でclampする。
   可変行高でも一度の末尾要求で到達でき、現在値は実際の先頭表示行を返す。
   読取専用本文でもscroll可、IME/modal/stream/終了処理中は操作不可。capture喪失でdragを終了する。
6. 表示設定は`folio_settings`版5の最後のbool `minimap`。既定はtrue。版1〜4は既存値を保持して
   trueを補い、次の設定変更で版5を保存する。未知/不正/欠落/重複/型違いは拒否し、読取失敗時の
   設定変更拒否、書込み成功後だけの採用は既存application経路を共有する。ノート/台帳schemaは不変。
7. 設定に「ミニマップを表示」を追加し、Ctrl+Shift+M、操作メニュー、パレットを単一の
   `FOLIO_COMMAND_TOGGLE_MINIMAP`へ結ぶ。キー記述子が表示と実行の正本。auto-repeatを消費し、
   設定面内のクリック以外のglobalキーは既存modal/IME guardを守る。3言語のヘルプも同じ記述子を使う。

## 検証と限界

設定移行/破損/保存失敗、catalogとキー、実native scrollと選択/Undo/dirty、上限sample/失効/非表示/
連続入力/文書切替/狭幅/可変行高/表/倍率/IME/modal/資源寿命/UIAを対象に確認する。
明暗/高コントラストと狭幅は画像で確認し、最終PR headの必須CIを統合条件にする。
256sample/16workは問い合わせ数の上限で、RichEdit内部処理の実時間上限ではない。
版/依存/ゲート/申請済みMSIXを変えない。Waivers none。

## #506補正 — 右余白とnative書式の概形

hideの2026-11-01の実機指摘に合わせ、表示中の右余白は56から12 logical pxへ、
本文との間隔は12から8 logical pxへ詰める。ミニマップ幅64と本文最小幅320を保ち、
非表示時は従来の本文右余白を使う。描画・hit・UIA・本文配置は同じ矩形から導出する。

行長だけだった派生cacheへ、nativeの文字サイズ・色・段落の字下げ・行頭の印・水平線・表の列数を加える。
閲覧のh1〜4は既存RTFの文字サイズ/色/段落間隔、リストはnativeのhanging indentと行頭、
水平線は空段落の書式、表は既存`note_pane_table`の行構造取得を使う。別のMarkdown parserや
文書全体の縮小描画を追加しない。編集時は編集中のnative表示に従う。

`note_pane`の既存ITextDocument取得から一回16行用の独立したTOM rangeを作り、選択を動かさずに読む。
文字列読取は各sampleの行頭1 UTF-16単位だけに限定し、全文/AST/RTFを保持しない。
表の行頭はnativeのU+FFF9と既存EM_GETTABLEPARMSで確認する。長い表と折り返しの全セル再現は保証しない。
256sample/16work/120msの制限、非表示・IME・modal中の停止、失効と完成像の公開、失敗時の有限終了を維持する。
native書式の問い合わせも含めて対象検証し、本文・選択・Undo・dirty・scrollが変わらないことを確認する。

根拠: 実RichEdit診断でh1〜4の22/17/14/11pt、水平線の1ptと背景、リストのhanging indent、
表行頭U+FFF9と既存行構造の列数取得を確認した。ITextRange2::GetRowは対象環境では表でもE_FAILだったため採用しない。
TOM rangeと書式の参照は呼出し内で解放し、cacheへCOM参照を保持しない。
リストの先頭はStartOfの移動量ではなく段落開始位置との一致で決め、折り返しの継続行で印を重ねない。
高コントラスト時はOSの実効配色、現在範囲内は選択面の文字色を使う。
[対象検証](../quality/2026-11-01-minimap-structure.md)に実RichEdit/主窓/画像と限界を記録した。

API根拠: [表示行数](https://learn.microsoft.com/en-us/windows/win32/controls/em-getlinecount)、
[行の先頭](https://learn.microsoft.com/en-us/windows/win32/controls/em-lineindex)、
[行長](https://learn.microsoft.com/en-us/windows/win32/controls/em-linelength)、
[表示行scroll](https://learn.microsoft.com/en-us/windows/win32/controls/em-linescroll)。


## #508補正 — レーンのホイールとコードブロック

レーン上の上下wheelは、本文と現在位置枠を一緒に動かす。ミニマップ独立の位置や減速状態を増やさず、
`note_pane`の既存wheel要求・OS行数/ページ設定・240ms減速・native位置反映/通知へ合流する。
root/本文/他の自前childに配送されたqueue入力も、レーンの共通矩形から同じ入口へ送る。
本文から親へ戻る入力で余韻を先に消さず、親の配送先が本文/ミニマップ以外と決まった時点で取り消す。
focus・選択・Undo・dirtyを変えない。レーン上ではCtrl併用でも倍率を変えず、本文上のCtrl倍率は従来通り。
IME・modal・hidden・未準備のguardと、drag中のwheel取消を維持する。

コードブロックは既にsampleへ取得しているnative背景の付いた通常サイズ行を、面と左辺で示す。
文字列の追加読取・言語分類・別parserは作らない。inline codeには背景が無く、hrは小さい空段落、表は
既存の行構造を優先する。空行も背景があれば帯を保つ。明暗はnativeの背景/文字色を対で使い、
高コントラストでは実効paletteの地/文字色と左辺で区別する。現在位置枠の外周/左右余白は残す。
最大256sample・16/idle・120ms、編集のnative平文、保存schema/版/依存を維持する。

## #510補正 — 文書投入後はidleへ直行し、stream中の予約をまとめる

hideの表示待ちの指摘を隔離した合成文書65回で調べた。通常文書の本文投入後の待ちは中央値128〜139msで、
120msの入力集約timerが大半だった。2,000行の取得は約5.6ms、描画約0.6ms。
3,000行の表ではstream中の更新予約が3,003回あり、予約だけで中央値42.579msかかった。
原本は`D:/NeNeFolio/agents/minimap-latency-20261101/findings.json`。hideは後に体感の上限を0.6秒と訂正した。
この体感値は実測値ではなく、実ノートの同条件再現は行っていない。

1. 文書投入・閲覧/編集の投入成功後は120ms timerを経ず、同じidle構築を開始可能にする。
   同期呼出し中にはsampleを取得しない。通常loopの入力/paint優先、16/idle・最大256、guardと完成像公開は維持する。
   入力集約と投入完了を閉じた更新種別で区別し、timer所有・失効・開始は一つの予約関数へ集約する。
2. 投入前に旧timerと旧文書の像を消す。STREAMING中のEN_CHANGEでは行cache/本文更新の印と像を失効させるだけで、
   timerの再予約・描画/UIA通知を繰り返さない。投入後の既存loaded_paneが全体を無効化し、成功時に一度開始する。
   失敗時は旧像や未完了構築を残さず、既存の本文未準備と再試行へ戻す。
3. 投入後の行番号幅の変化は既存のpane_inset判定とarrangeへ合流し、構築を開始する前に配置する。
   遅れたpaintの幅調整で新文書の構築が再び120ms待ちにならないようにする。
   同じ判定を通常paint時の再配置通知にも使い、配置/幅の別経路を作らない。
4. 編集の連続入力、通常resize/倍率/表示復帰などは120ms集約を保つ。編集中の直前完成像を保ち、
   本文・選択・Undo・dirty・focus・native scroll、IME/modal/非表示/未準備の抑止を変更しない。

アタリや段階表示、本文読込全体の高速化、別parser/cache/threadは追加しない。
公開UI/idle境界でtimer無しの開始と編集待機を固定し、同じ合成条件で開始待機・予約数・実処理を再測定する。
保存schema・版・依存・申請済みMSIXは不変。Waivers none。

## #512補正 — 検索とミニマップのidleを交互に進める

検索pending中は決定3の検索優先によりミニマップを最後まで待たせていた。
同じ主窓のidle入口に次の優先対象だけを閉じたenumで持ち、両方が実行可能なら交互に一単位ずつ進める。
初期優先はミニマップ。片方だけ実行可能ならその処理を進め、次の優先は他方へ移す。
検索の本文/一致計算進捗とミニマップのsample進捗は既存ownerに残し、複製しない。
毎単位の前には従来の通常queueへ戻り、入力/paint先行、検索一単位・16sample/最大256の上限を保つ。
timer/時計/threadを増やさず、IME/modal/stream/非表示/最小化など各処理の抑止と有限終了を維持する。

ミニマップ完成時の部分描画は既存の共通頭・caption・操作帯矩形で更新範囲と照合し、
交わらない領域の描画計算を行わない。操作帯の外枠は配置と共有し、字幅の参照も省く。描画cacheや描画経路を増やさず、同じbackbufferへ同じ描画を使う。
全体/部分の画素一致と、検索・ミニマップ双方の進行/取消/失敗/非表示を確認する。Waivers none。

## 2026-11-03 の補正（#522 — 画像を含む実表示高へ対応する）

表示行を同じ高さで並べる決定2/4/5の位置尺度を、`note_pane_geometry`が同じRichEditから導く
文書のpixel座標へ置き換える。文字sampleもnativeの上下端を使う。画像は#520の既存位置metadataから
一文字範囲のnative矩形を読み、横位置と表示高に対応する枠/面を描く。Markdownを再解析しない。

文字は従来の最大256sample、画像は表示対象の最大256矩形。どちらも一回16件までの同じidleで
順に構築し、両方完成してから公開する。画像差替え/幅/倍率/本文変更で失効し、旧文書の像は残さない。
画像が文字sampleの間にあっても省略しない。画像bytesや縮小bitmapをミニマップへ複製しない。

現在位置枠・click/drag・矢印/Page/Home/End・UIA RangeValueはpixel座標と同じ移動上限を使う。
矢印/小変更は既定本文の文字行高、Page/大変更は可視高。最小18logical pxの操作枠と短文の縮小表示、
入力優先/既存timer/検索との交互実行/未準備guardを維持する。保存schema・新しいthread・時計は増やさない。
