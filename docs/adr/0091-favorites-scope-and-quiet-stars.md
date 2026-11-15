# ADR 0091 — 星の状態と表示範囲を既存のドロワー操作へ分けて示す

- 状態: 受理（2026-11-15のhideの継続実装指示に基づく。製品編集前）
- 日付: 2026-11-15
- Issue: #817
- 規則: ARC-001/004/008/011/012、C-002/008/012/014/016/017/018、FR-003/006/009/032、QLT-008/010/012/013
- 関連: ADR0056/0057/0058/0060/0063/0090

## 根拠と相談

hideは既存の星の位置・大きさ・未登録の輪郭の多さを指摘し、11月15日にロードマップに沿う継続実装を指示した。
前日の案と現行main667f9a7を、バックグラウンドClaudeの`design:design-critique`で再確認した。
相談は読み取り専用、原応答はD:/NeNeFolio/agents/favorites-ui-20261115/claude-advice.md。
実画面・実測ではなくソースに基づく助言なので、寸法と視認性は完成候補の描画で確認する。

## 決定

1. 星は行右端の補助操作のまま、カテゴリ折畳印と中心w−28 DIPへそろえる。
   字形16 DIPと当たり28×30 DIPを分離する。ノートのcursor印もカテゴリと同じw−52〜w−46へ移す。
   題名右端はw−58へ固定し、星の表示有無で文字を動かさない。既存icon_paintと実効paletteを使う。
2. 登録済みは常時塗り星、未登録はマウスhover・実際に索引focusを持つcursor行・押下中だけ輪郭を描く。
   描画と押下の可否は同じ述語から導き、hoverは行refとindex改訂で結ぶ。本文focusだけで輪郭を残さない。
   pointer探索は既存のUIA配置cacheを使い、cursorは現在のapplication queryを重ねる。hover変化時は旧/新の星領域だけを再描画する。
   タッチ/ペン由来の合成mouse moveはhoverを作らない。未表示星への押下は通常の行操作へ渡す。
   識別は[Microsoftのmouse message署名](https://learn.microsoft.com/en-us/windows/win32/tablet/system-events-and-mouse-messages)をWin32 UI境界で読む。
   見える星、既存行メニュー・パレット・UIA Toggleも同じ星の意図へ渡す。
3. 星は押下/解放の同じref・index改訂・矩形とcaptureを照合し、外への解放・capture喪失・scroll/resize/DPI変更を取消す。
   星の変更後は、別の行が同じ座標へ来てもポインタが実際に移動するまでその星列の連続押下を消費する。
   時計・timerを増やさず、座標変化だけで解除する。通常の本文focusは奪わない。
4. 検索の下へ32 DIPの専用段を設け「すべて／お気に入り」（All/Favorites、全部/收藏）を常設する。
   UI_PART_STYLE_CHOICEの既存配色・focus・選択の棒を使い、見出しは「カテゴリ」に固定する。
   検索語は消さずAND条件を保つ。選択済み項目への再確定は無操作。
   段の高さは既存band_heightへ集約し、配置・clip・hit・scroll・UIAで共用する。
   tooltipも既存のdrawer固定操作の登録へ加え、UIAと同じ名前と矩形を使う。
5. UIAへDRAWER配下のSelection groupと連続する2つのRadioButton/SelectionItemを加える。
   選択値はfolio_stateのnote_scopeから読む。主窓に指定scopeを適用する内部関数を1つ置き、既存反転コマンドも通す。
   pointerは既存accessibility_host_actionへ、鍵は既存主窓のSelectへ接続する。新しい状態所有者や保存経路は作らない。
   Tabは検索→選択中の範囲項目→新規カテゴリ→索引の順。矢印はfocusだけ、Enter/Spaceで確定し、F6の区画は変えない。
6. 0件の索引へfocusが残った場合だけ、選択中の範囲項目へ移す。本文/検索欄のfocusは維持し、空の索引をTabで飛ばす。
   お気に入り総数0と検索との不一致は別の折返し文言を描き、同じ文言をTREE HelpTextへ渡す。検索準備中は出さない。
   総数はapplicationの新しい読取queryが既存note_ledger_favorite_countの和を返す。UIで数えず、派生値を別保存しない。
7. 文字は既存3言語表に置く。最小200 DIPでの表示を実書体で確認し、通常幅では等幅の2項目とする。
   実際の利用可能幅が足りない場合も矩形を逆転させず、必要な文字省略と完全なUIA名を保つ。
   schema/設定版/台帳版/並び順の意味・本文/Undo/履歴/保存成功後の採用はADR0090のまま。

## 検証と限界

既存favorites_nativeで未保存/Redo保有本文と履歴を継続確認し、同じexeのfavorites_ui_nativeで
非表示星・hover・focus・touch署名・同座標連続押下・取消、範囲の選択/UIA/鍵/検索保持と0件を確認する。
両CTestの上限は30秒のまま。描画matrixは診断selectorへ分け、総数queryは対象unitで確認する。
狭幅・3言語・DPI・明暗/HCの実画素と寸法は合成dataの専用desktopで記録する。全試行は品質記録へ残す。
物理タッチ端末・全OS・スクリーンリーダーの実発話と、合成メッセージ/公開UI境界で確認できたことを区別する。
最終head必須CIと正典mergeを維持する。追加waiverなし、既存WVR-0001は今回の範囲外。
