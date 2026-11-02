# 画像の高さ・ホイール・ミニマップ — #522

## 問題と変更

hideの通常版確認で画像付近のwheelに違和感があり、ミニマップの画像ブロックが求められた。
実RichEditの600×240本文で、同じwheel一回が画像の前/途中では240px、通常段落では93pxになることを再現した。
nativeだけの位置指定は全高709pxまで進み、末尾が空になることも確認した（`probe-01/02`）。

`note_pane_geometry`が同じTOM範囲とnative scrollから文書pixel座標を観測する。
wheelは既定本文の文字行高×OS行数で移動量を決め、全高−可視高で止まる。画像一行だけでも移動できる。
240ms減速・細かなdelta・OSページ設定・取消は既存の経路を使う。非同期画像の差替えはnativeの自動移動も含めて
先頭可視文字の位置差を一回補正し、進行中のwheel・選択・Undo・dirtyを保つ。

ミニマップは文字最大256sampleと画像最大256矩形を、同じidleで一回16件ずつ構築する。
画像位置は#520のmetadataから読み、parserや画像bytesを複製しない。実際の横位置/表示高のブロックを描き、
現在範囲・click/drag・矢印/Page/Home/End・UIAも同じpixel尺度と上限を使う。
本文/幅/倍率/画像差替えの既存失効経路、入力優先、検索との交互実行を保つ。

規則: ARC-001/004/005/009/011/012、C-002/008/012/013/016/017、QLT-008/012/013、GIT-001〜004。
ADR0026/0084を実装前に補正し、PROJECT_LAYOUT・憲章・用語・画像案内・仕様を合わせた。
保存schema・版・依存・Store提出物は変更しない。
追加waiverはnone。既存WVR-0001はWinHTTP callback ABIだけで、今回の変更には使わない。

## 対象検証

Windowsの隔離desktop、実RichEdit、正規Debug targetのASan/UBSan/nullability・clang-tidy付きで実行。
原本は`D:/NeNeFolio/outputs/522-image-geometry/`、確定/保全記録は
`D:/NeNeFolio/agents/image-geometry-20261103/`。`tools/record-run.py`で入力/成果物のsnapshot・hashとraw出力を保存した。

| コマンド・原本 | 結果 |
| --- | --- |
| `cmake --build build --target image_display_tests minimap_structure_tests minimap_scheduling_tests nenefolio`（`target-02`） | 成功。最終製品入力のコンパイル/静的解析。後続の変更はimage_displayの追加ケースと文書のみ |
| `ctest --test-dir build --output-on-failure --no-tests=error -R '^(image_display\|minimap_structure\|minimap_scheduling)$'`（`target-02`の個別結果） | minimap_structure 95checks、minimap_scheduling 298checks成功。後者はUIA/矢印/Page/Home/Endのpixel位置一致も確認。image_displayは下記fixture修正前の失敗で、run全体の成功とは扱わない |
| `cmake --build build --target image_display_tests` / `ctest --test-dir build --output-on-failure --no-tests=error -R '^image_display$'`（`target-04`） | 173checks成功。画像の前/途中/後で59〜60px、末尾停止、画像一行、幅/倍率、表セル、image block画素、thumb/drag位置、非同期anchor、wheel継続、text/選択/Undo/dirty、70,000px、256文字sample＋20画像を18単位で完了 |

初回の静的解析は文字行高取得の複雑度11を拒否し、GDI資源の取得/解放を一つの関数へ分けて解消した。
`target-01`は非表示fixtureでwheelを試したため失敗し、成果物指定にも配置先の誤りがあった。
`target-02`の画像試験はhidden marker内の無効な選択範囲を期待して失敗した。
fixtureを隔離desktop上で表示し、可視本文を選ぶ形へ修正した`target-03`が成功。
長文・画像batch・表矩形を追加した最終`target-04`も成功した。製品の不具合を期待値変更で隠していない。
minimap2件は関係入力が不変のため成功を再利用し、画像試験だけを再実行した。

最終PR headの必須`eng/check.ps1` CI、main統合、通常exe反映、data保持、追加worktree/一時出力の整理は
同agentsの`integration-completion.json`で確定する。CI成功前に完了扱いにしない。

## 限界

今回の観測OS/RichEditで70,000pxを含む座標を確認した。全Windows build/全DPIや物理wheel全機種の保証ではない。
画像ブロックは実寸に対応する概形で、縮小写真ではない。非常に長い文書では1pixelへ縮約する。
通常データは変更せず、hideの実機での感触は統合後に確認する。
