# #336 — Release条件の検索・一覧応答

2026-10-18、hideの「やりやすい順番で進めて」に基づく次ベータ準備の最初の単位。
基点main `0d983a7e92143a9e2359a8c87e83279c5862fade`。製品の変更・新しい最適化は行っていない。
Issue: [#336](https://github.com/hideyukiMORI/nene-folio/issues/336)。規則: FR-032 / ADR0024 / ADR0045 / QLT-008・012・013 / GIT-001〜004。
恒久証跡: `D:/NeNeFolio/agents/release-readiness-20261018/`。Waivers: none。schema・依存・ゲート変更なし。

## 条件と対象

Windows 11 Pro 10.0.26300、120 DPI、1100×750物理画素。clang-cl 19.1.5、Release `/O2 /Ob2 /MT /DNDEBUG`。
正典の`eng/package-release.ps1`で生成し、conformance・symbols・版・manifest・許可import・資産・ZIPの照合が成功した。
この時点の版入力は0.1.0のままで、公開済みZIPを置き換えていない。今回のZIPは測定用であり、次版の公開候補ではない。

100カテゴリ、1,000件または10,000件、本文1件10,240 UTF-8 bytesの決定的ASCIIデータ。
外部probeは未変更`folio_window.c`をbyte一致の先頭部分として持つ翻訳単位に観測部を足し、正典のRelease libraryをリンクする。
製品の警告設定を含むcompile引数を維持。read_noteは回数を数えて実adapterへ転送するだけで、読み込み結果を差し替えない。
実EDITへのWM_CHAR → EN_CHANGE → 既存application/filter経路を通し、入力が戻る時点と描画後を同じ試行のQPCで記録した。
描画はメッセージ処理・対象窓のUpdateWindow・GdiFlushまで。専用desktopを使い、入力desktopの切替やglobalキー注入は行わない。

初回はfresh processの3回。後続3語・全件一致時の描画・scrollは準備1回の後に各30回。
空または末尾1文字を除く語を計時外で設定し、最後の1文字を送る。結果の語と一致数、初回read N回・後続read 0回、scroll前進を確認した。
採用rawは316行（準備10行を含む）、統計は306試行。集計器がQPC差、sample順、件数、read数、負荷重複を独立に再計算した。

## 結果

中央値 / nearest-rank p95 / 最大、単位ms。初回n=3のp95は最大値と同じで、安定した裾の推定ではない。

| 区間 | 1,000件 | 10,000件 |
| --- | ---: | ---: |
| 初回入力→描画 z | 78.948 / 82.816 / 82.816 | 719.264 / 726.717 / 726.717 |
| 後続入力→描画 z | 8.698 / 14.838 / 15.999 | 51.820 / 55.415 / 55.418 |
| 後続入力→描画 alpha | 28.946 / 32.101 / 32.803 | 211.946 / 229.180 / 229.734 |
| 後続入力→描画 aaaaab | 10.914 / 12.165 / 12.367 | 84.148 / 85.738 / 87.802 |
| 一覧WM_PAINT | 26.290 / 27.481 / 27.586 | 203.217 / 214.030 / 218.167 |
| 一覧scroll→描画 | 28.014 / 31.312 / 37.084 | 206.007 / 213.231 / 215.441 |

10,000件で全件一致するalphaは、入力処理の中央値2.526ms、描画区間209.406ms、全体211.946msだった。
これらは同じ試行の区間だが、独立した中央値を足した値を全体としない。全体は別に直接集計している。
不一致zでは入力47.649ms、描画3.956ms。初回は本文10,000件を読む経路を含み、全体719.264msだった。
一覧描画そのもの203.217msとscroll後206.007msから、次の性能改善候補は一覧の画面外行の描画処理とする。
`drawer_window.c::draw_rows`は現在、配置の全行にdraw_category/draw_noteを呼び、GDIのclipに任せている。
未表示行の文字変換・描画呼出しを減らせるかが次の焦点であり、この測定だけで改善後の速度や画素一致を主張しない。

## 試行と負荷の扱い

開始前に5回連続の静穏を確認し、計測中は1秒間隔と終了後にToolhelpで記録した。
compiler/build/testはbusy。確認済み.NET serverもPIDと生成時刻が一致し、CPU増分が厳密に0の区間だけidleとする。
busy観測を前後snapshotまで拡張して対象群の全区間と照合した。実行中の他案件を停止・変更していない。

- 最初のinclude形式はWin32 handle typedefに対するheader nullability警告でビルド失敗。同じソースを翻訳単位の先頭に置き、警告を弱めず解消した。
- 最初のworkerは、計測側のEM_SETSEL(-1,-1)が末尾移動になるという誤りで語の一致assertionが失敗。全worker結果を診断用に残し、採用しなかった。
- 明示した文字長で末尾へ移すよう外部probeを修正し、再build後のv2を使用。製品入力は不変。
- v2の1,000件scroll群は末尾のVBCSCompiler CPU活動と重なり、31行全体を不採用。同じprobeでscroll群だけを再測定した。成功済みの検索/描画群を繰り返していない。
- 初回検索の統計は固定したrun 0・1・2を使う。scroll再試行の初期化として行った初回検索は混ぜない。
- 採用群は全てexit 0、入力/結果assertion成功、観測した負荷重複なし。負荷のあったrawと分類も保持する。

16:18:59に全計測終了、16:19にhideへ他案件再開可能と通知。集計後、11,000 mdを含む11,204入力ファイルの全hashが実行前と一致した。
追加ファイルは専用dataのlockだけ。source/build/fixture/probe/rawと採否は恒久証跡へ保存し、archive内のbyteも照合する。

## 実行と限界

- `pwsh -NoProfile -File ./eng/package-release.ps1 -WorkDirectory D:/NeNeFolio/outputs/336-release-response/release-work -OutputDirectory D:/NeNeFolio/outputs/336-release-response/package`: exit 0。
- 外部`compile-response.ps1`: 最終probeのexit 0、警告なし。`run-response.py`の採用workerは全exit 0。
- `summarize-response.py`: 316 raw / 306統計試行をQPC・入力・結果・read・負荷記録から照合し成功。
- `verify-fixtures-after.py`: 11,204ファイル一致、変更/欠損0。
- 文書の差分/参照を確認し、最終PR headの必須CIは通常の`tools/merge-pr.ps1`で統合する。実際のPR/head/CIと整理は同所`completion.json`で記録する。

cold OS cache、実ノートの分布、DWM present、物理キー入力、IME候補窓、混在DPIのモニタ移動、人の聴取は測っていない。
専用desktopのGDI描画完了であり、人の体感の直接測定ではない。監視間隔より短い負荷や全ホストのCPU/IOを排除したとは主張しない。
旧Debug値と構成が異なるため、過去測定との短縮率を出さない。既存core/OOM/UIA/NVDAの成功検証は再実行していない。
次は未確認の実機項目を確認可能な範囲で整理し、次ベータの版・変更履歴・配布候補を正典経路で準備する。公開は別の操作である。
