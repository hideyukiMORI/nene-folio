# ADR 0062 — 同じ文字計測を少ない回数で行い、表示範囲の描画へ絞る

- 状態: 受理（hideの体感速度改善依頼に対する実装前判断）
- 日付: 2026-10-24
- Issue: #368
- 規則: ARC-001/004/005/011、C-008/012/013/016/017、QLT-008/012/013

## 根拠

main 936a1332を実Win32の隔離desktopで測定すると、Releaseでも初回設定541.5ms、テーマへの切替中央値492.9ms、初回検索789.5msだった。
文字列の各prefixをDrawTextWで繰り返し計測し、同じ失敗予約高を設定/テーマ切替で捨てていた。
一覧も描画clip外の文字を変換して描画APIへ渡す。coreの配置/選択/scrollを変えずに削減できる。

NeNeNib main 605f21f9のEditorWindow::follow_look/deliver、Direct2DRendererのstatus_text_layoutを読み、色と文字配置の入力を分け、配置結果を再利用する方針を参考にする。
描画backendを移植する判断ではない。

## 決定

1. `draw_text` → `wrapped_units` → `measured_units` の計測/描画の正典を維持する。`DrawTextW(DT_CALCRECT)`、ASCII字間、元文字列全体によるfont-link判定を変えない。
   明示改行まで全体を先に計測し、収まればそのまま1行にする。あふれる場合はUTF-16のcodepoint境界列で、実測幅から次の候補を見積もり、収まる端とあふれる端を狭めてprefixを選び、その範囲内の既存のword境界へ戻す。
   見積りは探索順だけに使い、幅は毎回同じGDIで実測する。両端の内側へ候補を制限して必ず進む。
   最終的に採用する行も同じ実測で収まることを確認する。文脈依存の字形でprefix幅が単調でない場合にも、最大prefixの数学的保証を要件にせず、採用行の幅・文字の保持・進行を保証する。
   CR/LF/CRLF・空白trim/skip・surrogate pairを保持する。測定不能/最初のcodepointが入らない場合は既存の0返却と面の失敗処理へ渡す。
2. 設定/テーマの失敗予約高は幅/DPI/font/言語が同じなら共用する。両面のfooter高を同じcacheへ保持し、表示面だけでは失効させない。
   fontの再生成は既存の明示無効化を維持する。色・現在候補・結果文字列をcache keyへ足さない。理由全文の固定予約高を省略/近似しない。
3. drawerは既存coreレイアウトのrow矩形とclientのheader以下のclipとの交差だけを描く。上端の部分行・下端の部分行・fade・drop線を維持し、配置/ヒット判定/UIAの正本は変えない。
4. 実測で費用が小さい処理を推測だけで別経路へ移さない。同期単一UI thread・保存の耐久性・native accessibility・theme previewの取消/失敗契約を維持する。

## 検証

同一fixtureと構成でbefore/afterを記録する。文字は3言語/3render DPI・狭幅・CRLF/空白/非BMP/結合文字を測り、採用した行の幅、描画と高さ、全文到達を確認する。
設定/テーマの反復切替、resize/font/言語のcache失効、Help/検索/置換の固定領域を確認する。一覧は1,000/10,000件で時間と描画画素を比較し、scroll両端と部分行/dropを確認する。
対象Debug/tidyと規約検査、最終headの必須CIを通す。計測は外部fixtureだけに置き、製品へ時計や試験口を足さない。

保存schema・実行時依存・ゲート閾値/除外は不変。Waivers: none。
