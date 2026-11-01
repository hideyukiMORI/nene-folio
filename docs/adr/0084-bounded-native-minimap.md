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

API根拠: [表示行数](https://learn.microsoft.com/en-us/windows/win32/controls/em-getlinecount)、
[行の先頭](https://learn.microsoft.com/en-us/windows/win32/controls/em-lineindex)、
[行長](https://learn.microsoft.com/en-us/windows/win32/controls/em-linelength)、
[表示行scroll](https://learn.microsoft.com/en-us/windows/win32/controls/em-linescroll)。
