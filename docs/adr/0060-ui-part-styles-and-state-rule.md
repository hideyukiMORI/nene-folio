# ADR 0060 — 操作部品を3様式＋選択群と1つの状態規則へ統一する

- 状態: 受理（2026-10-18、hideの「GUIの統一性」の依頼に対する設計席の判断。実装前に固定）
- 日付: 2026-10-18
- Issue: [#348](https://github.com/hideyukiMORI/nene-folio/issues/348)（I-A: 部品体系と状態）
- 規則: ARC-001/004/011、C-002/008/012/017/018、QLT-008/011/012/013
- 関連: ADR0047/0048/0051/0056/0059（補正）、[docs/design/2026-09-13-gui-first-operations.md](../design/2026-09-13-gui-first-operations.md)

## 文脈

hideが2026-10-18に、共通ヘッダー・ドロワー・本文操作列の14要素の統一性の不足を指摘し、
クリック後の動作と見た目の確認、専門家ペルソナの議論、文献の考察を経た調整を依頼した。
設計席は隔離fixtureで59状態を撮影し（`D:/NeNeFolio/agents/ui-consistency-20261018/captures/`）、
情報設計・視覚デザイン・Windows操作/アクセシビリティの3ペルソナによる2ターンの独立レビュー
（同 `discussion/`）と約30件の文献（同 `research/literature.md`）で統合案を収束させた。実在専門家への取材ではない。

実測の事実: 部品は枠付き絵＋字・塗りセグメント・絵だけ・地だけの字・角丸の橙の札の5様式、高さは32/28/27/24の4種。
hover表現はどこにもなく、押下の見た目は新規とカテゴリ＋だけで、他は`WM_LBUTTONDOWN`の時点で実行される。
focusは`DrawFocusRect`のXOR点線で、橙の札の上では1.38:1。tooltipは6要素だけ。
全127テーマの計算で、文字系トークンは地に対し4.5:1以上あるが、`border`は全テーマ、`selected_background`は112テーマ、
`dialog_border`は47テーマで3:1に届かない。

## 決定

1. **様式を3つ＋選択群に固定する。** 他の様式（枠付き文字・角丸札・地だけの溝）は廃止する。
   - A 中立コマンド: 16pxの絵＋字。絵は「5秒で意味が分かる」もの（新規・保存・検索）に限り、広い窓では字を常に出す。幅が不足するときだけ絵に縮め、消さない。
     単独なら1pxの`dialog_border`枠、群の中なら群が枠を持ち項目間は1pxの`border`区切り。「操作▾」は絵を持たず、シェブロンを絵の位置に置く。
   - B 主操作: `chip_background`で塗り`chip_text`の字。1つの面に最大1つ。本件では用途を定めるだけで、本文での適用（変更ありの保存）はI-Bで行う。
   - C 絵だけ: 窓の慣例と部品内の小操作に限る（設定ギア・窓の×・ヘルプ「?」・欄内の消去×・カテゴリ＋/−・前/次）。当たり28×28、絵20px。tooltip必須。
   - S 選択群: 表示3択と閲覧/編集を同じ部品で描く。群全体に1px枠、項目は等幅、項目間1px区切り。選択中は`selected_background`＋`selected_text`＋下辺2pxの`selected_text`の棒。
2. **寸法を1つの体系にする。** 操作部品の高さ28。ヘッダー44（8+28+8）、本文操作帯36（4+28+4）は維持する。字の釦の左右余白12、絵16、絵と字の間6。
   群内の間隔0、群と群の間16、部品と面の端12。角は直角（GDIの`RoundRect`は縁を平滑化せず125/150/175%で段が出る）。
   論理pxは4の倍数とし、120/144/168 DPIで整数にする。表示3択の区画54→56、群の余白2→4。ダイアログの確定列の高さ32は唯一の例外として明記する。
3. **状態の表現を1つの規則にし、線は文字系トークンで引く。**
   - hover: 地を塗らず、内側1pxの`header_text`枠（地を`border`で塗ると15テーマで字が4.5:1を割る）。
   - pressed: 地`selected_background`、字`selected_text`。selected/on: S群の項目だけ、上記1の表現。
   - focus: 2pxの実線。色は「枠が接する地の上で使う字の色」とする。単独部品は1px外側に`current_text`、群の中の項目は内側にその項目の字色。
     キーボード操作の後だけ描き、`SPI_GETKEYBOARDCUES`が有効なら常に描く。太さはDPIで拡大する。XORの`DrawFocusRect`は廃止する。
   - disabled: 字`border`（dialog_themeの既存規則）。
   - 入力欄: 静止枠は1pxの`header_text`（4つの地すべてで全テーマ3:1以上、最低4.26）、focus時は2pxの`current_text`。
   - `border`／`dialog_border`／`selected_background`は装飾の面とし、それだけで状態を表さない。
   - `theme_tests`に「状態・境界・focusの線のroleと隣接色 ≥ 3:1（全テーマ）」を加える。満たさないテーマでは別の文字系トークンへ機械的に切り替える関数を正典に1つ置く。
4. **押下は離した時に実行する。** 新規の既存capture経路（`folio_window.c`の`SetCapture`→矩形内`UP`で実行）を一般化し、
   押された部品を1つの値で持ち、矩形内で離したときだけ実行し、外で離すかcaptureを失えば取り消す。
   実行はキーボード・UIAと同じ`folio_window_accessible_action`を通し、経路を1本にする。Spaceも離した時に実行し、自動繰り返しは無視する。
   押下で開く例外は「操作▾」だけ（メニューを押下で開くのがOSの慣例）。
5. **表示名・UIA Name・AcceleratorKey・tooltipを1つの元から引く。** 表示名＝UIA名。字付き釦のtooltipはキーだけ、C様式は名前＋キー。
   全操作部品にtooltipを付ける。検索欄のUIA Nameが空である点と、「キー操作を表示」釦のUIA名が「ヘルプ」である点を直す。
   S群はTabで群に1回だけ止まり、矢印はfocusだけを動かし、Enter/Spaceで選ぶ（矢印で選択まで動かすと一覧経由で窓が縮む・閲覧で保存が走る）。
   F6/Tabの区画巡回と、UIAのRadioButton/SelectionItemの構造は維持する。ADR0048の決定3（3択へのTab順の追加なし）とADR0059の決定3をこの形で補正する。
6. **新しい面パス**: 保存・検索・ヘルプ「?」を`icon_paint`へ追加する。既存の語彙（面で塗る・太さ2・直角か45°・曲線は円だけ・evenodd）に従い、字形は使わない。
7. **変えないもの**: theme schema、command catalogの意味と並び、保存モデル（明示保存＋Esc／ノート切替の保存）、vim風キー、3言語、3DPI、最小窓560×360と一覧のみ320、
   新しいノートの位置とヘッダー高44、本文操作帯36と左端36（ADR0059）。新しいトークンは足さない。

本ADRは部品体系と状態の規則（I-A）だけを固定する。本文操作帯の再配置・保存の主操作化・未保存印・ヘルプのヘッダー移動・空状態（I-B）、
操作▾の中身と区切り・View/Readの文言（I-C）は、hideの確認後に別Issueと本ADRの補正節で固定する。
owner-drawメニュー・既定カテゴリ色と`chip_background`の衝突・#32770ダイアログへの暗幕は別案件とする。

## 強制と確認

- active（維持）: enum網羅、文言表の3言語網羅、閉じたコマンド、format/tidy/conformance、`theme_tests`の文字系4.5:1。
- active（本Issueで追加）: `theme_tests`の「線のrole／隣接色 ≥ 3:1」の全テーマ検査。
- 不能: 見た目・実OSのhover/capture/focus・DPIの一般保証。完成候補でDebug/tidy・theme_tests・文言unitと、隔離Win32 fixtureでの
  geometry／hover／press-release-cancel／focus／tooltip／UIAの対象確認、3DPI×3言語×3表示の画像を品質記録へ残す。
  既存の保存・RTF・検索性能の試験は再実行しない。最終headのフルゲートは必須CIに集約する。schema・依存・ゲート変更なし。Waivers: none。

## 結果

14要素が3様式＋選択群のどれかに属し、同じ高さ・同じ状態表現・同じ実行契約（離した時に実行）を持つ。
focusと境界は全テーマで3:1以上を機械で守る。押し間違いは離す前に取り消せる。tooltipとUIA名が表示名と一致する。
部品の高さが揃うぶん、新規（32→28）と札（24→28）の見た目が変わる。

## 却下した選択肢

- 全部を字だけにする: 狭幅で「字を落として絵を残す」規則が作れず、13aのようにノート内検索だけが消える問題が残る。
- 常設のprimaryを置く（新しいノート）: 橙がヘッダー左上に常時灯り、未保存の合図（I-B）と競合する。
- hoverの地を`border`で塗る: 15テーマで字のコントラストが4.5:1を割る。
- focusを外白・内暗の二重枠にする: 1px外側の間隔と合わせて4pxになり28高の部品で太すぎる。単色でも文字系トークンなら全テーマで3:1を満たす。
- 窓タイトルへ未保存の`*`を付ける: `tools/capture-window.ps1`と`post-keys.ps1`が題名の完全一致で窓を探すため、既存の測定道具が壊れる。
- 角丸4px（Windows 11風）: 縁の平滑化を前提にしないと段が出る。採るなら全部品同時の別判断。
- 新トークン`control_border`の追加: `header_text`で全テーマ3:1を満たすため不要。

## 参照

- NN/g「Icon Usability」（2014）、Nielsen「10 Usability Heuristics」#4、Norman『The Design of Everyday Things』のシグニファイア
- Microsoft Fluent 2「Button」（primaryは1レイアウトに1つ）、WinUI「Command bar」「Buttons」、「Keyboard accelerators」
- W3C WCAG 2.1 1.4.3 / 1.4.11 / 2.5.2 / 2.5.3、Microsoft「About Tooltip Controls」
- 文献の全一覧と確認度: `D:/NeNeFolio/agents/ui-consistency-20261018/research/literature.md`

## 2026-10-18 の補正（#348 の実装で判明した4点・設計席の裁定）

1. **一覧のみ（幅320）では表示群と設定ギアの間を8にする。** 群間16を守るとADR0059の移動領域24が取れない（12+28+24+176+16+28+4+28+12 > 320）。
   移動領域を優先し、320幅だけ群間を8へ縮める。I-B（#349）でヘルプ「?」を足す際は表示3択の区画を56→44へ縮め、この例外を再評価する。
2. **「操作▾」のシェブロンは当面 `ui_text` の▾文字のまま。** 決定1の「絵の位置に置く」は維持するが、面パスは決定6に含めない。面パス化と文言は I-C（#350）で判断する。
3. **`SPI_GETKEYBOARDCUES` は `src/ui/win32` が作成時と `WM_SETTINGCHANGE` で読む。** ARC-007 は中核の決定性（時刻・乱数・ロケール・環境変数・ファイル）の規則であり、
   DPI と同じ表示設定の読取は ui/win32 の責務とする。adapters へ寄せない。
4. ADR0047 決定2（共通帯の構成）、ADR0048 決定1（区画54・余白2・表示群と設定の間12）と決定3（3択へのTab順を足さない）、
   ADR0059 決定1（新規高32・ブランドとの間24）と決定4（操作列の構成）を、本ADRの決定1〜5の形で補正する。各ADRの本文に補正節を置く。
## 2026-10-18 の補正（#349 / I-B — 本文操作帯と保存状態）

hideの確定（2026-10-18: 閲覧/編集は右上のまま、新しいノートはヘッダー左のまま、編集の橙は撤去、破壊的操作後の画面上の知らせは出さない）と
第3ターン(a)の3席一致（`D:/NeNeFolio/agents/ui-consistency-20261018/discussion/round3a-conclusion.md`）に基づき、決定1〜7 へ次を足す。

8. **保存の状態を見せる。** 変更あり（dirty）のときだけ「保存」を B 主操作にし、保存済みで A 中立へ戻す。押せる状態は保つ（disabled にしない）。
   無題は開いた時点で dirty。編集モードに入っただけでは灯さない。パンくずのノート名の後ろに 8px の `chip_background` の丸を未保存印として置く。
   UIA はパンくずの Name に「未保存」、保存釦の ItemStatus、保存成功時に `UiaRaiseNotificationEvent`「保存しました」。視覚のトースト・timer・新しい面は足さない。
   窓タイトルは変えない（`tools/capture-window.ps1` と `post-keys.ps1` が題名の完全一致で窓を探す）。
9. **橙の意味を2つに固定する。** `chip_background` は「変更ありの保存（B）」と「ダイアログの確定（B）」だけに使う。閲覧/編集の札は S 選択群（`selected_background`＋棒）にし、橙を使わない。
   位置はパンくず段の右上のまま（hide の判断）。
10. **ヘルプをヘッダーへ移す。** 本文操作列から外し、設定ギアの左に C 様式の「?」として置く。名前は「ヘルプ」、tooltip「ヘルプとコマンド一覧（F1 / Ctrl+P）」。
    Tab 順は 新規→表示3択→ヘルプ→設定→閉じる。UIA 親はヘッダー。F1 / Ctrl+P / `:help` の経路は不変。
    320幅では表示3択の区画を 56→44 に縮め、ブランドと「表示」の見出しを操作より先に落とす。移動領域は ADR0048 の規則のまま。
11. **本文操作帯は [保存｜このノート内を検索｜操作▾] の A 群。** 群内 1px 区切り、左端 36 基準（ADR0059）。狭幅では字を落として絵だけにし、消さない。
    「操作▾」は絵を持たずシェブロンを絵の位置に置く。tooltip は「このノートの操作」。
12. **新しいノートの位置はヘッダー左のまま。** 弱点「押す前に作成先が見えない」は手がかりで補う: tooltip を表示時に組み立て「新しいノート — 〈現在のカテゴリ〉へ（Ctrl+N）」
    （既存の `TTN_GETDISPINFOW` 経路）。UIA Name は「新しいノート」のまま、作成先は HelpText。カテゴリ 0 件では作れない理由を tooltip / HelpText に出す。作成先の規則は Ctrl+N と同じ。
13. **空状態。** ノート未選択では操作帯と閲覧/編集を描かず、案内を出す: 見出し「ノートが選ばれていません」、本文「左の一覧からノートを選ぶか、新しいノートを作ります」、
    キー行 Ctrl+N / Ctrl+Shift+F / F1、作成先カテゴリ。3言語は `ui_text` が所有する。
14. **英語の衝突。** 閲覧の札を Read、表示の見出しは View のまま。

却下: 常設 primary（新しいノート）、編集の橙の維持（未保存の橙と同時に灯り意味が二重になる）、閲覧/編集の操作帯への移動（hide の判断で右上維持）、
ドロワー先頭・カテゴリ＋の隣・ヘッダー右・本文側への新規の移設（3表示すべてで入口が残るのはヘッダーだけ。320幅で右側は満杯）。
ADR0048 決定1（表示群と設定の間 12・区画 54）と ADR0059 決定1（新規高 32・ブランドとの間 24）・決定4（操作列にヘルプ）をこの形で補正する。


### 補正の追加（#349・hide の所感 2026-10-18「マージンとパディングの意識が弱い。入力欄の文字が枠に付いている。ヘッダーの端の距離と枠線を再考したい」）

15. **余白の原則: 窓の縁には16、枠の内側には8以上（4の倍数）。** ヘッダー左右16、新規→ブランド16、表示見出し→S群8、S群→「?」16、「?」→歯車8、歯車→×8、ペイン右端16（閲覧/編集の右端は×の当たりの右端と同じ縦線）、
    ドロワーの検索欄・カテゴリ帯の inset 16。字釦の内側12、絵と字の間6は決定2のまま。
    入力欄は 28 高の枠の内側に左右8（`EM_SETMARGINS`）、縦中央。focus の2pxは内側へ太らせ、字の位置を動かさない。placeholder は窓/本文の地では `header_text`、面の上では `dialog_secondary`。
    320幅の縮退は 群間16→8、区画44→40 の順で、移動領域24以上を条件とする（補正1の「320幅の群間8」はこの規則に吸収）。
16. **枠の意味: 切り替え（S 選択群）は枠を持たない。** 選択は塗り＋下辺2pxの棒、項目間は1pxの区切りだけ。枠を持つのは「押すと実行する」A 群・単独の A 釦・入力欄。
    決定1の S「群全体に1px枠」をこの形へ補正する。hover/pressed/focus・UIA Group は不変。

却下: 端の余白を広げるだけで枠を残す案（枠の列と絵だけの部品が同じ高さで並ぶ違和感が残る）、歯車・×に枠を足す案（絵だけの部品に枠を付けると様式 C の定義と衝突）。

### 実装時の補足（#349）

- 決定8の通知は「md を書いた保存」に限る。変更の無い Ctrl+S（書き込みが省かれる保存）では通知を出さない。無題を開いた時点の未保存化でも出さない。
- 無題の題は「無題」とし、未保存は丸の印と UIA の「（未保存）」で示す。ADR0028 決定11 の「無題（未保存）」の文字表記をこの形へ補正する。
- 320幅の縮退は決定15の順（群間16→8、区画44→40）に従い、結果は区画40・群間8・移動領域32。群間16・区画40（移動領域24）も条件を満たすが、順序の規則を優先した。
- `enum ui_text` は値が256を超えたため `unsigned short`。schema や文言表の意味は変わらない。

### 独立レビュー（#353）後の補足

- 決定3の補足: B 主操作の hover は内側1pxの枠ではなく、下辺に focus と同じ太さの `chip_text` の棒を描く（#352 レビュー L7 の裁定。`chip_text` と `chip_background` の対は既存の 4.5:1 検査の範囲）。
- 決定8の補足: 未保存の合図（B の塗りと丸）の色も決定3の仕組みで選ぶ。役割 `THEME_LINE_UNSAVED`（地は pane / window）を足し、候補は `chip_background`（字 `chip_text`）→ `selected_background`（字 `selected_text`）。
  どちらも 3:1 に届かないテーマでは B を 2px の `current_text` の枠と字で描き、丸も同じ色にする。`theme_tests` が全テーマで塗り 対 地 ≥ 3:1 と塗りと字の対 ≥ 4.5:1 を検査する。
- 決定8の補足: 通知は store の世代が進んだことだけで出し、本文を評価できなかった回は世代を更新しない。未保存の比較は EN_CHANGE の印があるときだけ行う。
- 決定5の確認: 全ノート検索の EDIT の UIA Name は、#348 の注釈後も実デスクトップの native UIA で空のまま（MSAA 名はある）。**未達**として #351 で調査・修正する。この項目を達成済みとして扱わない（ADR0001）。
- 空状態でノートのみ表示のときは focus を F6 の先頭（新規）へ寄せる。枠の描き方は「箱の内側」の1本に統一し、名前入力のカテゴリ一覧も同じ方式。

## 2026-10-23 の補正（#350 / I-C — メニューと結果の表示）

hideの今回の再開・全権委譲と、設計サナ／実装サナ／デザインリナの照合で、2026-10-18の下書きを次の契約として受理する。
根拠は #350、main `12c2a32`、`D:/NeNeFolio/agents/350-menu-surface/` の調査・デザイン議論。
旧下書きの担当、ローカル全ゲート義務、日付、署名は現在の指示に引き継がない。実装と対象検証はSOL、設計・裁定・統合は設計サナが担当する。

17. **操作▾と右クリックは既存 CommandLayer の MENU。** `COMMAND_SURFACE_MENU` は入力欄・題・脚を持たない一覧で、面の所有者、候補の世代、閉鎖、スクロール、UIA hostを既存面と共有する。
    パレット・履歴と異なる位置、キー、閉鎖、UIA型は同じ部品のmode差であり、「差は3つだけ」とした下書きは採らない。
    メニューの行描画は `ui_part` に集約し、OSの `TrackPopupMenuEx` / `TrackPopupMenu` と重複する語・実行経路を撤去する。パレット全体の外枠変更は #351。
    - 地 `dialog_surface`、1px `dialog_border`、直角、影なし。行高32、行の塗りは縁から左右4、字は縁から16、上下の内側4。
      枠内8以上は字までの距離であり、塗りのinset4とは別の寸法。群間は高さ8の領域の中央に1px線を引く。
    - 有効行は通常の文字色、キー列は `dialog_secondary`。無効行は字もキーも `header_text`、選択・押下・実行を受けない。
      一覧の無効行は内容を読めることを優先するvariantとして、決定3の釦のdisabled色から区別する。
      hoverとキー選択は `selected_background` と `selected_text`、左2pxの棒で示す。棒は既存 `THEME_LINE_FOCUS_SELECTED` の色選定に従う。
      選択の棒が接する地との3:1を既存theme検査で守り、色の近い有効／無効行は対象画像で確認する。別の色トークンは足さない。
    - 幅は実フォントで各行の「名前＋アクセスキー表示＋間16＋キーまたはEx名＋左右16」を測り、その最大を240〜360へ丸める。
      client幅−32が上限。キーは各行で右寄せし、全行の最大ラベル幅と最大キー幅を別々に足さない。
      列を隠さず、代替書体などでなお不足するときだけ名前を末尾省略し、UIA Name／HelpTextは全文を保つ。配布対象の3言語×3DPIで省略不要を確認する。
    - 釦の直下、右クリックは指した位置、Shift+F10／アプリキーは索引cursorの行の左下へ付ける。cursorが無ければ空白メニュー。
      主窓clientの縁16を除いた範囲で、下に収まらなければ上下の広い側を使う。横は同じ範囲へ寄せる。
      anchorに重ねる縦ずらしは行わず、広い側にも不足するときは面内をスクロールする。全7項目の同時表示は最小窓で保証しない。
      上下にあふれがあるときは各8pxの合図領域を取り矢印を描く。項目は区切り込みで完全な行だけを見せ、選択の移動は行を表示範囲へ寄せる。
    - マウスで開いた直後は未選択、キーで開いたら先頭の有効行。↑↓は無効行を飛ばして巡回、Home/End、ホイール、アクセスキーを受ける。
      Enter/Spaceと一意のアクセスキーは押下の自動繰り返しを無視し、解放で実行する。Tabと左右は実行しない。
      英字アクセスキーを3言語共通でcatalogに置き、表示は名前の後の `(A)` 形式。幅は常に予約し、下線だけ既存のキーボード合図に従う。`ui_text` に `&` は埋めない。
      同じ面のアクセスキーは重複を検査で拒む。無効行のキーは実行しない。
    - 左DOWNと同じ行の左UPだけで実行し、開いた直後のUP、右ボタン、外での解放、capture喪失では実行しない。
      項目から実行するときは面を閉じてから既存の受け口へ渡す。開く押下からドラッグして実行する経路は追加しない。
      Esc／Alt／F10／外クリック／非アクティブ化で閉じる。操作▾の再押下は閉じるだけ。戻り先は既存 `command_return_focus` と可視focus解決で、操作▾からは同釦へ戻す。
      右クリックは選択文書を変えず、一覧のみの表示モードも変えない。
    - UIAは Menu/MenuItem、IsEnabled、AccessKey、AcceleratorKey、PositionInSet/SizeOfSetを公開し、MenuOpened/Closedを実際の開閉で送る。
      操作▾のExpandCollapseも同じ状態を読む。行名は装飾の下線とアクセスキー表示を除く全文。閉鎖後の参照失効・STA・非同期Invokeの再検査はADR0056/0058を維持する。
      ▾は既存icon_paintの語彙に従う面パスへ揃え、UIA名は引き続き「操作」。

18. **中身と可否の正本。** 操作▾は3群7項目:
    `別名で保存… / 名前を変更… / 保存して閲覧` ｜ `置換… / 履歴から戻す…` ｜ `編集を破棄して読み直す / ノートをごみ箱へ移す`。
    行番号は設定とパレットに残し、操作▾から外す。名前・tooltipの範囲は「このノート」のまま。
    - catalogにscope（note/app/category/layout/quit）、destructive、needs_inputを持つ。needs_inputは引数なしGUI入口で追加入力を待つ意味で、末尾の「…」はこの属性だけから生成する。
      破壊的操作でも確認のないものには「…」を付けない。編集破棄は既存どおりメモリ上の保存済み本文へ戻し、ディスク再読込はしない。
      元の25 command ID・順序・Ex文法は不変。色の選択はADR0039決定12のままコマンドにしない。
    - coreの静的な不透明menu descriptorがcontextごとの項目・群・アクセスキーを返す。通常項目は既存commandを参照し、色だけ閉じた例外variantにする。
      右クリックの並びは、カテゴリ＝`新しいノート` ｜ `色を変える… / カテゴリ名を変更…` ｜ `新しいカテゴリ…` ｜ `カテゴリを削除`、
      ノート＝`名前を変更…` ｜ `ノートをごみ箱へ移す`、空白＝`新しいカテゴリ…`。
      名前はパレットと共通captionから生成する。カテゴリ指定新規も同じNEWで、明示対象とHelpTextが作成先を表す。
      行の対象はUI一時状態が保持し、実行は既存の `*_at` に合流させる。色は既存の色選択、改名は既存の文書切替・保存保護を維持する。
    - 有効可否はapplicationのconst queryが既存のoutcomeで返す。現在の対象と明示のカテゴリ／ノートを区別し、文書種別・モード・絞り込み・既知件数・対象範囲を読む。
      開く時のファイル走査・保存・本文読込・確保を追加しない。未索引filesや実履歴の有無は推測しない。実行時の既存検証も必ず通し、古い可否の結果を実行許可にしない。
      ADR0039決定11の「灰色にしない」を、この読み取りqueryによる無効表示で補正する。

19. **コマンドとdrawer操作の結果は `report_outcome` へ合流する。** 入口の種類で表示方法を選ばず、結果の種類と現在の面で決める。
    READY／CANCELLEDは失敗を表示しない。入力を持ち結果行を描ける面（Ex・パレット・検索・置換・テーマ）では、その1行を優先して入力とfocusを保つ。
    名前入力中の検証は、既存の名前入力面がその結果行を所有したまま扱う。閉じて返った結果は同じreport経路へ渡す。
    メニュー自体は結果行を持たず、実行前に閉じる。その他の場面では、I/O失敗を既存の失敗の箱へ、前提不足／入力の拒否を既存の下端1行へ出す。
    下端の通知は音・絵なし、UIA通知あり。ノート領域が非表示なら一覧の下端に同じ1行を出し、表示モードを変更しない。
    起動不能やUI資源の生成失敗の既存箱は本変更のコマンド結果経路とは分けて維持する。箱そのものの見た目は #351。
    - 未選択時の語は「〈コマンド名〉はノートを開くと使えます。」の型で3言語から引く。名前不正などの既存結果は原因に沿う1文を維持する。
      現在の操作文脈を一時的に渡し、別の操作の名前を次の失敗へ持ち越さない。
    - ノート除去の成功は画面上に知らせない。UIAだけ「〈名前〉を一覧から除きました」と知らせる。
      applicationのREADYは実際のごみ箱移動と既に不在の両方を含むので、「ごみ箱へ移しました」と断言した下書きは採らない。
      復元先の説明は実行前のHelpTextへ置き、ごみ箱へ移せたものがWindowsのごみ箱から戻せることを伝える。結果型・port・保存schemaは増やさない。
      ADR0039補正4の画面上成功行をこの形で置き換える。未編集対応文字の閲覧通知・終了拒否通知は維持する。

20. **キー表記と実行は同じbindingから読む。** coreの静的な型付きshortcut descriptorがkey・modifier・action・routeと表示を所有する。
    UIはHACCEL担当分をそこから変換し、WM_CHAR／WM_KEYDOWN担当分も同じ記述子で解決する。OS担当のAlt+F4は既存OS経路のまま明示する。
    全キーをHACCELへ移さず、IME抑止・入力欄・F6/Tabの既存契約を維持する。tooltip／UIA／menu／helpの重複キー文字列を同じ元へ合流させる。
    既存キー・Ex文法は不変。直接キーが無いときはEx名を `:history` のようにコロン付きで表示し、どちらも無い保存して閲覧／置換は同じmenu descriptorのアクセスキーを「V（メニュー内）」等の限定付きで表示する。
    この2項目のUIA AcceleratorKeyは空、AccessKeyとHelpTextが面内の限定を示す。本文Escは保存して索引へfocusを返すだけで閲覧へ変えないため、VIEWのshortcutと偽らない。
    `:%s` は既存の直接置換文法としてhelpに残し、置換面を開くキーと偽らない。新しいglobal shortcutは作らない。
    単体でbindingの意味・重複・表記を、native対象検証で実アクセラレータと既存の入口を確認する。「すべてHACCELと一致」とした下書きの検証は実在routeに沿って補正する。

21. **付随する既存指摘の境界。** ヘッダーfocusからもF1／Ctrl+Pが効くことを確認し、既存の「?」tooltipを入口にする。`:` の経路は変えない。
    ToolBarの同名問題はmainですでに「本文の操作帯」へ修正済みなので再実装しない。一覧320幅の選択群の区切りは対象画像で確認する。
    全ノート検索EDITのUIA Name、ダイアログ、検索と置換の帯、ヘルプの外枠は #351。

### I-C の受け入れと検証

完成候補でcatalog／文言／availability／shortcutの対象unit、Debug/tidy、3言語×3DPIのメニュー画像、560×360と一覧320幅、
右クリック3対象、反転／スクロール、無効skip、押下取消、全閉鎖契機、UIA型／順序／開閉／参照失効、結果の3表示先、無確認除去の通知を確かめる。
候補は起動直後・入力なし20秒の主スレッドCPUが1秒未満であることを確認する（#356）。
成功済みの無関係な保存・RTF・検索性能は繰り返さない。最終headのフルゲートは必須CIへ集約する（QLT-012）。
保存schema・実行時依存・ゲート閾値・除外は不変、Waivers: none。技術的な未測定条件は品質記録に分け、完了済みと主張しない。

### I-C の検証方法に関する訂正（2026-10-23）

独立レビュー後の補足に記した全ノート検索EDITのUIA Name未達は、[再診断](../quality/2026-10-23-native-edit-uia-probe.md)で検証clientの初期化順によるものと判明した。
独立native clientでは既存製品のName・Edit・ValuePatternは正常。#351に製品のName修正を要求しない。
I-C以降のnative検証はmanaged UIAと別プロセスに分離する。物理IME・実readerの未測定は維持する。

## 2026-10-24 の補正（#351 / I-D — 面・入力・検索置換帯）

hideの2026-10-23の継続・全権委譲と、設計サナ／実装サナ／ClaudeCodeのデザインリナの照合に基づく実装前の裁定。
#350はPR #360でmain `80e2814`へ統合済み。設計資料と実寸・native挙動の診断は `D:/NeNeFolio/agents/351-surface-research/`。
この節は決定2のダイアログ32高例外、I-A時点の暗幕対象外、ADR0035の旧題帯・角丸・固定4行・無効な改名一覧、ADR0052の置換配置対象外を補正する。
Issue下書きの全modal共通AltF4、一致への独自着色、既存対比検査の重複追加は、以下のowner境界・標準選択・既存検査再利用で置き換える。
合成診断は設計可能性の証拠であり、製品実装と完成候補の検証はこれから行う。plannedの検査をactiveとは扱わない。

### 22. 面の所有者と共通の外形

- 名前入力6種（初回保存、別名保存、ノート改名、新規カテゴリ、カテゴリ改名、無題離脱）と改名の再試行、失敗箱は既存native DialogBoxを維持する。
  標準EDIT/LISTBOX/BUTTON、dialog manager、IME、native UIAを維持し、独自入力provider・別の状態modelを作らない。
- 設定/テーマは既存CommandLayerのmodal、ヘルプ/履歴は同じownerの浮き面、検索/置換は同じownerの帯。
  外形の矩形・題・閉じる形・境界描画をui_part/dialog_themeの共通経路へ集める。各面の内容の所有者と実行経路は移さない。
- 面はdialog_surface、1px dialog_border、直角。題の段44、文字は左16、×は右8/上8の28四方、内容の縁16、行32。
  native popupと設定/テーマ/履歴は幅480、ヘルプは460。主窓child面はclient幅−32へ縮める。native popupはowner中央基準でmonitor workareaへ寄せる。
  native popupは一覧320幅のownerからも480幅で開き、ownerの表示モードを変更しない。これは従来400幅popupの位置づけを維持する。
  対応最小BOTH560×360ではpopup高さを328以内にする。monitor workareaの方が狭ければそこへ制限し、ラベルを上積みにする。
  小窓の縮退は開く時の最大結果高さで決め、結果が出てから面や一覧が縮まないようにする。
- native windowは合法なWS_CAPTION|WS_SYSMENUを保持し、WM_NCCALCSIZEで非client領域を除いて題をclientに一つ描く。
  titleのhitはDialogProcのDWLP_MSGRESULT規約でHTCAPTION。OSの移動/終了経路を維持する。題はSTATICを重ねず描画のみで、windowのNameは既存title。
  Alt+SpaceはOSのGetSystemMenuを使い、メニューや移動処理を別実装しない。EDIT focus時はWM_SYSCOMMAND/SC_KEYMENU(lParam=32)が入口であり、WM_SYSKEYDOWNだけでは不足する。
  clientの×はnative BUTTON、Tabstopなし、Nameは閉じる。
  2026-10-23の合成診断ではTab/Enter/Esc/×/AltF4/Move/owner復元/IME guard、native Name/Value/Invokeと余計なTitleBar/Closeが無いことを確認した。
  DWM矩形・属性・画像を製品でも記録する。PrintWindowが窓外の影を含まない限界を明示し、属性成功だけで影や角の見た目を成功としない。
- 暗幕はmodalだけ。既存folio_window所有のbackdropをnative DialogBox実行中はclient全体へ出し、ネスト前の面/regionへ戻す。
  名前入力や失敗箱が別のbackdropやmodal状態を所有しない。作成失敗/取消/正常完了でも対にして戻す。

### 23. 名前入力・確定列・結果

- 題はcatalogのlabelを使う（captionの末尾ellipsisは付けない）。無題離脱だけ「無題のノート」を題にし、保存の問いを本文へ置く。
  retryでは保留中の実kindに対応する題と説明を使う。既存の復旧順序・取消が意図を消さない意味は変えない。
- ラベルは名前/保存先、Name/Save to、名称/保存到。左列80、間8、右欄360、外16で幅480。
  これは同梱Noto Sans JP/SC・14px logical・120/144/168DPIの実測で最長49.6logicalだった。fallback書体では実幅を検査する。
- 名前欄32。改名時は全選択。作成/保存のカテゴリは既存LISTBOXで行32、通常4行、小窓では2行にして全候補へスクロールできる。
  ノート改名のカテゴリはdisabled一覧を除き、readonlyの1行EDIT「〈カテゴリ〉（変更できません）」を使う。全文を選択/コピー/横送りでき、UIAにも全文を公開する。
- 案内はdialog_secondary、検証はcurrent_text＋警告の形。該当する検証1件が案内を置換する。
  最大必要高を実fontのDrawTextで測って最初から予約する。448幅の英語invalid-categoryは3行、最大62.4logicalであり、名目60へ切らない。
  復旧の長い理由はreadonly複数行EDITの固定領域とnative縦scrollで読み切れるようにする。WM_CTLCOLORSTATICでもFIELDの地を使う。
  Enterを文字改行へ奪わず、釦外のEnterは既定承認。readonly欄にいるときもこの規則である。
- 確定列は右寄せ、同じ幅、最小88、高さ28、間8。無題離脱は左から保存/保存しない/キャンセル、native creation/Tab順も同じ。
  主操作Bは承認1つ、中立Aは他。dialog_themeは共通ui_partの状態描画を使い、角丸と旧1px focusを撤去する。
  無題離脱の保存/保存しない/キャンセルはAlt+S/D/Cをnative mnemonicで付け、dialog managerへ任せる。
  既存ui_text captionを正本とし、owner-drawと幅測定のprefix解釈を揃え、ODS_NOACCEL時は下線を隠す。UIA Nameへ&を漏らさずglobal shortcutは足さない。
  Enterはfocusされた釦、その外では承認。Esc/×/AltF4は取消。既存の読み替え1関数に×も含め、×へfocusが来ても承認へ落とさない。
  IME中は確定/取消を実行せず、focus枠の再描画だけは止めない。IME終了時も現focusへ同期する。
- 空入力は長さ0だけをUIの入力不足として「名前を入力してください。」にする。空白のみなどの名前規則は既存の型で判定する。
  既存NAME_REQUIREDは無題の名前付き保存が必要という別意味なので流用しない。
- 完全同名の改名は既存application内の一致判定位置で新しい閉じた結果NAME_UNCHANGEDを返す。
  note/categoryともsynchronize優先を維持し、後続保存/改名を行わない。GUIで重複比較せず「同じ名前です。」を表示して面を残す。
  Exも同じ結果行と入力保持へ合流する。core rename_planのno-op意味は変えず、schema/portは増やさない。

### 24. 失敗の箱

- #350のreport_outcomeと分類を維持。箱の題は製品名、command由来なら本文の先頭にcatalog名、起動失敗などcommandなしなら理由だけ。
  三角はcurrent_text。釦は「閉じる」で中立、OKを使わない。native EVENT_SYSTEM_ALERTを実際の表示時に1回送る。
- 警告音はI/O失敗だけ。前提不足/入力拒否には音を付けない。起動時の資源不足など既存の通知経路を消さず、音の分類だけ明記する。
  色・書体・境界・題・×は名前入力と同じ経路。長文は省略せず折返し/内容領域scrollで読み切れるようにする。

### 25. 検索・置換の上部帯

- SEARCHとREPLACEは既存CommandLayerの共通帯のmode差。本文の操作列の下に置き、本文領域を同じ高さだけ下げて覆わない。
  通常検索はliteral（既存ASCII大小無視）、置換は正規表現・編集本文だけ。語を自動コピーせず、意味/下見/保存/Undo/IMEの既存所有者を保つ。
- SEARCHは検索ラベル/入力/前/次/×/結果。REPLACEは上段に正規表現のパターン、2段目に置換後/1件/すべて、×と結果。
  labelとEnter方向は入力後も残す。対象名は帯の上で常に見える既存pane breadcrumbとそのUIA全文を正本にし、帯内へ重複しない。
  置換結果の帯内は件数だけのSTATUS_REPLACE_COUNT_ONLYを使い、:%sは従来の名前/件数を維持する。幅がある場合の基本行44、狭幅ではlabel/結果を独立した固定高の行へ置く。
  最小pane320の置換欄は144logicalを許し、1件/すべては既存mono11と幅64を使う。native dialogの88下限とは別の帯の部品。
  幅/DPI/fontごとのgeometryと最長の関連理由を実測し、結果変化で本文高さが動かないよう予約する。省略で理由を隠さず、最小窓でも本文の可視行を残す。
- 「1件」は現在の選択開始位置以降でパターンに一致する最初の対象に作用する。現在の選択範囲を置換対象の制限と解釈せず、既存anchor意味を保つ。
  Enter=1件、Ctrl+Enter=すべて、Tab=2入力間、Esc/×=共通閉鎖、Ctrl+H=本文caretの既存契約を維持。:%sは直接置換のまま。
- 一致の着色は標準RichEdit選択を維持する。Issue下書きのselected_background強制は採らない。
  文字書式の変更でUndo/dirty/高コントラストを壊す別経路を足さず、ADR0052決定5を維持する。

### 26. ヘルプ・履歴・設定/テーマ

- ヘルプの表示切替は「操作｜キー」のS2択とUIA SelectionItem、従来Tabの切替を維持する。この面ではS群への通常Tabstop追加をしない明示例外。
  候補のID/catalog順は不変。表示はscopeで群にして、群内の順だけcatalog順を保つ。表示順はノート/カテゴリ/表示/アプリ/終了。
  見出しは非選択Text、↑↓は飛ばす。絞り込みで空の群は表示しない。候補indexから既存commandへの写像を一つにし、別の実行経路を作らない。
  キー説明は折返してページ送りを維持。切替の表示/Hit/UIAは同じ選択値を読む。
- 履歴は題と×、Esc案内は閉じる。履歴採用の未保存/Undo/読込失敗意味は維持する。
- テーマには「‹ 設定」。Escは従来どおり設定へ1段戻る。×は設定全体を閉じる。主窓childなのでAltF4はアプリの通常終了で、#350のOS routeを奪わない。
  native DialogBoxではAltF4取消、というowner境界の差を明記し、全modalに同じAltF4意味を強制した下書きを補正する。
- theme_testsのdialog_secondary/current_text対dialog_surfaceは既存で検査済みなので同じassertを増やさず再利用。追加した状態役割があればその境界だけ増やす。

### 検証と統合

対象Debug/tidy、名前outcomeと3言語文言・themeの単体、各面の3言語×3DPI・最小560×360/一覧320の画像、Tab/Enter/Esc/×/AltF4/IMEメッセージ、
暗幕ネスト/失敗時復元、native UIA Name/Value/Invoke、検索置換のCtrlS/Undo/選択/dirty/矩形復帰を変更境界として確認。
native UIAはmanaged UIAと別clientプロセスにする。全ノート検索EDIT Nameの製品修正は不要（再診断済み）。
物理IME・実reader・DWM窓外影など未測定を明記。成功済みの無関係な検証は繰り返さない。CI最終head check.ps1成功後に統合。
schema/依存/ゲート閾値/除外は不変。Waivers: none。完了後D作業木/不要一時物を規約どおり確認して整理する。

### 2026-10-24 のA/B実装契約

- Aが `ui_part_surface_region.h` の閉じたenum（TITLE/TITLE_TEXT/CLOSE/CONTENT）、`ui_part_surface_rect(RECT,UINT,region)` と `ui_part_draw_surface(HDC,palette,RECT)` を先行実装する。
  boundsは呼出元座標のphysical px、44/16/8/28は明示dpiで拡大、境界はclient1physical px。面の配置・内容・翻訳・fontは各ownerに残す。
  Bは共通API確定後に利用だけを行い、ui_partとui_textを同時編集しない。
- Aはnative name_prompt/failure_box/dialog_theme、NAME_UNCHANGED/application/unit、共通ui_partのdialog_surface隣接地を担当。
  既存PRIMARY/hover/focusのcontrast選定へdialog_surfaceを追加して機械で確認する。既存saveの色が変わる場合は影響themeだけを追加確認する。
  dialog_secondary/current_textの既存assertを複製しない。
- Bはfolio_windowのCommandLayer/帯/各面/暗幕wrapper、drawerの残るreport直呼び、既存UIA面公開を担当。
  name_promptのrequest/resultは不変。failure_box_showは第4引数にnullableの `const enum folio_command*` だけを追加し、既存command_context/reporting_commandを渡す。
  同期呼出し中だけ借り、Aが入口でcopyする。文字列historyや第2のcommand状態を作らない。
- native暗幕はHRGN snapshotの複製を作らず、native_modal_depthと既存command_surfaceからregionを再導出する。
  native中はSetWindowRgn(NULL)で全client、深さ0への復帰で現在の面から穴/可視性/Z順を再構成する。DPI/resize後に古い矩形を復元しない。
  名前入力の準備失敗は業務操作前にOUT_OF_MEMORYで既存reportへ戻す。
  元の失敗を知らせる箱は暗幕準備失敗で理由を捨てず、既存failure_boxのnative/OSfallbackでその理由を表示する。wrapperは再帰reportしない。
  復元失敗は暗幕をhideして入力不能を避け、既存の資源失敗経路へ一度だけ渡す。DialogBox自身のowner disableは保つ。
- native C様式×に短いtooltipを付ける。既存TOOLTIP_CLOSEとshortcut bindingのAlt+F4表示を使い、新文言IDやキーliteralを増やさない。
  tooltipはdialogのowned windowとして寿命を結び、palette更新で作り替わるdialog_themeのbrush状態と分離する。
  TTF_IDISHWND|TTF_SUBCLASSで×を登録し、tooltipのための第2の矩形を持たない。作成/登録失敗は×と共にrollbackする。
  共通native_messageのTTN_GETDISPINFOで固定modal languageから文字を作り、通知のszText等の正しい寿命へ格納する。stack pointerを残さない。
- ui_text.[ch]/ui_text_testsはA単独所有。新IDはFAILURE_NAME_UNCHANGED、PROMPT_NAME_EMPTY、PROMPT_TITLE_UNTITLED、PROMPT_CATEGORY_READONLY、FAILURE_COMMAND_REASON、
  STATUS_REPLACE_COUNT_ONLY、HELP_TAB_COMMANDS/HELP_TAB_KEYS、HELP_GROUP_NOTE/CATEGORY/LAYOUT/APP/QUIT。THEME_BACKとPROMPT_LABEL_NAME/CATEGORYは既存IDを更新する。
  SEARCH/REPLACEラベルは既存COMMAND_FIND/ACCESSIBLE_PATTERN/REPLACEMENTを表示とUIAで共用し、不要なPROMPT_LABEL_CATEGORY_NAMEは共通Nameへ統合する。
- ADR・quality/current/report/handoffと必要なCMake登録は親が所有する。#350main統合→#351新作業木→ADR補正→A共通宣言→A/B排他的実装の順で進む。
