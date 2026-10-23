# #350 — メニューと結果表示の統一（I-C）

2026-10-23〜24。Issue [#350](https://github.com/hideyukiMORI/nene-folio/issues/350)、PR [#360](https://github.com/hideyukiMORI/nene-folio/pull/360)。
基点main `12c2a32`、実装区切り `dbcda520`、独立レビュー是正後 `9bbb7c3`。
設計は[ADR0060決定17〜21](../adr/0060-ui-part-styles-and-state-rule.md)、結果表示の補正はADR0039補正9。
規則: ARC-001/004/011、C-002/008/012/017/018、QLT-008/011/012/013、GIT-001〜004。Waivers: none。
schema・実行時依存・ゲート閾値・除外は変更しない。

## 変更境界

- `core/folio_command`にscope/destructive/needs_input、静的な不透明menu/shortcut descriptorを追加した。
  25 command IDとcatalog順・Ex文法は保ち、captionの「…」は追加入力の属性だけから作る。右列は直接キー、次にコロン付きEx名、最後に面内限定のアクセスキー。
- `application/folio_state`のconst queryは現在文書と明示対象を区別し、既知状態だけから可否を返す。開く時のI/O・確保は追加せず、実行時の検証も維持した。
- `folio_window`の既存CommandLayerへMENUを追加し、操作7項目と右クリック3対象を同じ面へ集めた。
  `drawer_window`のOS popup menuを撤去し、対象付き実行は既存applicationの受け口へ合流する。
  完全な行だけのスクロール、上下反転、無効skip、押下取消、開く打鍵/クリックからの誤実行防止を実装した。
- `ui_part`の行描画と`icon_paint`のシェブロン、既存accessibility host/nodeのMenu/MenuItem・開閉・キー・可否・Scrollを使う。
  providerの第2経路やnative EDITの再実装は追加しない。
- `report_outcome`が現在の面と結果から出し方を選ぶ。入力面内を優先し、ほかではI/O失敗を箱、拒否理由を可視区画の下端へ出す。
  一覧だけでも文字とUIA通知が出る。ノート除去の成功は名前入りUIA通知だけで、確認箱と画面上の成功行は出さない。
- KEY_BINDINGS・SPECIFICATION・ADR・currentを更新した。全ノート検索EDITの過去のName未達は[独立native再診断](2026-10-23-native-edit-uia-probe.md)へ訂正した。

## 対象検証

恒久証跡は `D:/NeNeFolio/agents/350-menu-surface/`。下表のログは同 `archive/logs/`、製品exe・source snapshotは `archive/artifacts/`、診断sourceと手順は `archive/tools/` へ収載した。
完成した製品コードに対する検証だけをまとめ、成功済みの無関係な保存・RTF・検索性能は繰り返していない。

| 対象 | 実行・結果 | 証跡 |
| --- | --- | --- |
| Debug/clang-tidy | 固定toolchainのDebug build成功 | `build-final.log` |
| catalog/shortcut/3言語caption/availability | `build/folio_tests.exe --menu-surface` 成功 | `unit-02.log` |
| 規約 | `python eng/conformance.py`、0違反 | `conformance-final.log` |
| 変更C/Hの整形 | 42ファイルの `clang-format --dry-run --Werror` 成功 | `format-final.log` |
| 空白 | `git diff --check` 成功 | 実装席completion記録 |
| native対象fixture | `compile-native.ps1`でbuild後、専用desktopで164項目/0失敗 | `native-final.log`、`native-menu.c` |
| 起動直後CPU | 実120DPI、入力なし20秒、主thread CPU 0.0156秒（1秒未満） | 実装席completion記録 |
| 独立native UIA client | 製品Debug exeを別PIDで起動。MenuOpened 1 / MenuClosed 1受信、Expand/Collapse状態1→0、関係HRESULT S_OK | `native-event-client-02.log` |

native対象fixtureは製品の描画とprivate経路を使う合成データの確認であり、実データを操作していない。
Menu1/MenuItem7、AccessKey/AcceleratorKey、PositionInSet/SizeOfSet、ScrollPattern、owner thread逸脱0、開閉API28対、閉鎖後の参照失効を確認した。
結果3経路、名前入り除去通知、押下取消、無効skip、閉鎖契機、最小窓と一覧表示を含む。
3言語×120/144/168DPI画像はfixtureでDPIを注入した製品描画であり、3台の実モニタへ移動した試験ではない。

## 独立レビューと初回失敗

読み取り専用レビューは固定`dbcda520`に4件のP2を指摘し、`9bbb7c3`の差分で全件是正を確認した（`review/report.md`）。

1. DRAWERの下端文字が描かれない: noticeを共通blitへ移し、書体/背景も自身で設定。対象確認で実画素540pxと画像を記録。
2. メニューを開くEnterのrepeatが先頭項目を実行する: MENUでも元lParamのrepeatを拒否し、開いた打鍵のUPは実行しない。
3. 検索/置換の旧失敗が残る: 次の入力/結果とCtrl+S成功で旧report状態を解消し、現在の結果へ戻す。
4. UIA Scroll範囲が小さい: padding/群間/矢印を含む実際の完全行geometryから範囲とviewportを導く。

初回147項目中5失敗のうち、反転1件はfixtureのlogical/physical換算漏れ、結果4件は閲覧中の置換が入力面を開く既存動作を誤って下端と期待したものだった。
同時に見つかった操作menuのanchor誤り（検索釦の矩形を参照）は製品を操作釦の共通矩形へ修正した。
期待値を実際の契約と照合して直した最終164項目は0失敗。初回ログは成功根拠にしない。

## UIA実イベントの診断と確認範囲

最初のsource-include event fixtureはAPI送信1/1に対し配送2/1を観測したが、`UIA_HWND*`へ4byteの`int*`を渡す不正があった。
属性取得HRESULTも未確認で、空Nameや0を実値として解釈できない。この版は正しい配送数や属性の成功根拠にしない。

製品sourceをincludeしない独立native COM clientへ切り出し、正しい型で `/W4 /WX` の警告0を確認した。
CacheRequestを使いcallbackではCached属性だけを読む最終観測では、同じ製品の開く/閉じるを各1回受信した。
open側の属性はS_OK、closed側の属性は参照失効による`UIA_E_ELEMENTNOTAVAILABLE`であり、空Name/0を公開値と解釈しない。
レビュー席も最新source/logで型・Cached取得・各1購読を確認した。製品のlifetime契約・待ち時間・通知抑制は変更していない。
これはAPIの各1回送信と、独立clientで各1回受信した限定観測である。旧2/1の全原因解明や全clientでのexactly-once保証とは区別する。

## 見た目と残る確認

設計サナとClaudeCodeのデザインリナが実画像を確認した。デザインリナは `/design:design-critique` を使い、指定6画像をReadで閲覧した。
更新後のanchor、3言語のラベルとキー列、群の区切り、無効行と選択の棒、完全行の表示は採用できると判断した。
一覧320の旧画像は寸法メタデータがなく受理根拠から外した。限定の幾何確認8項目/0失敗で、OS/注入/描画DPIがすべて120、
主窓client400×450（320×360logical）、menu334×52physical、完全行40＋上下余白10＋境界2を記録し、同名画像を更新した（`native-geometry.log`）。
menu幅334はclient縁16ずつを除いた360以内で、anchorの上側へ収まっている。設計サナが新画像と数値を照合した。
`en-cobalt2-disabled-selected-menu.png`も追加し、色が近いテーマで無効なReplace/Discardと、選択中のSave and viewの塗り・棒を確認した。

物理IME・実打鍵・スクリーンリーダーの聴取・全OS/全DPIの一般保証は未測定。独立レビューは製品やテストを再実行していない。
合成fixtureのログにはASanの `interception_win: unhandled instruction` 警告も残る。これを警告なしと扱わず、製品Debug/clang-tidyと独立clientの警告0とは区別する。
最終headの `pwsh -NoProfile -File ./eng/check.ps1` はPRの必須CIで確認し、この対象検証だけで統合済みとはしない。
