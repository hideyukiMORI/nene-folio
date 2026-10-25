# #381 — メニューを主窓の操作文字へ統合する

日付: 2026-10-25。基点main: `7e1bdb97ecc757eb0a2a1ac6259a9733ca48229c`。
規則: ARC-001/004/008/012、C-008/012/013/016/017、QLT-008/012/013、GIT-001〜004。Waivers: none。

## 判断と変更

#379ではメニュー項目をユーザーの付けた名前と同じ言語別face14へ揃えたが、比較する役割が違っていた。
hideの指定で検索/ヘルプ「操作」「キー」/設定を含む画像14枚を再評価し、同じ動詞を行高32で並べるヘルプ「操作」を基準にした。
Fable利用上限後にhideが指定したOpusのdesign:design-critiqueと親の照合は
`D:/NeNeFolio/agents/ui-typography-review-20261025/review-findings.txt`にある。
hideの「その方向性で対応して」を受け、ADR0060決定17を先に補正して実装した。

`folio_window.c`のMENU専用フォント資源と字間0専用の計測関数を撤去した。
項目名とキー列は、既存の主窓`mono_font`（Consolas11・標準の太さ）と`draw_utf8`/`measure_utf8`を共有する。
ASCII行の字間は既存の1 logical px、非ASCIIを含む行は既存font-link規則の0とする。
日本語/簡体字の実glyph faceをConsolasとは断定しない。同じ共有フォントと描画規則へ統合したという意味である。
下線はラベル全体の字間とフォントの行情報から計測し、末尾キーが省略されるときには描かない。
行高32・余白・群間・色/枠・行のhit/UIA経路は保つ。ノート名/名前入力/本文の書体は別の役割として維持する。
公開API・保存schema・依存・ゲートの変更はない。調査/実装/検証は単体、追加の独立レビューは行わない。

## 対象検証

恒久証跡: `D:/NeNeFolio/agents/menu-operation-style-20261025/`。
作業checkout: `D:/NeNeFolio/worktrees/381-menu-operation-style`。
生成物: `D:/NeNeFolio/outputs/menu-operation-style-20261025/`。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target nenefolio
python eng/conformance.py
git diff --check
pwsh -NoProfile -File D:/NeNeFolio/agents/menu-operation-style-20261025/build-fixture.ps1
# runtimeを作業directoryにして実行し、stdout/stderrを倍率ごとに保存
.\fixture.exe 120
.\fixture.exe 144
.\fixture.exe 168
pwsh -NoProfile -File D:/NeNeFolio/agents/menu-operation-style-20261025/build-comparison.ps1
```

- Debug build/clang-tidy、conformance 0 violations、空白検査が成功。
- 外部fixtureは製品の`folio_window.c`を一つのtranslation unitへ取り込み、完成候補のライブラリと結ぶ。
  製品の検証口や分岐は増やさない。専用desktopで合成1カテゴリ/1ノートを使い、入力desktopへ切替えない。
- 3言語×120/144/168の描画倍率×通常960/最小560/一覧320の幅で、操作/カテゴリ/ノート/空白の99条件を確認した。
  一覧320に存在しない操作ボタンは対象外。OS実DPIは120、144/168は外部fixtureでの描画倍率注入であり、物理DPI移動ではない。
- 計測は独立したGDIの`GetTextExtentPoint32W`/`GetTextExtentExPointW`と照合した。
  メニュー文字と共通操作描画の画素、下線の位置とfocus cueによる変化範囲、選択/無効、全行へのスクロール、
  行高/hit/UIA用矩形、全文Name/AccessKey、計測/描画後の共有フォントと字間の保持を確認した。
- 各processで3回のDPI変更通知後の再openと20回の開閉を追加し、共有フォントの有効性とGDI資源数の不変を確認した。
  計1,649 checks×3 = **4,947 checks / 0 failures**、3 processともexit 0。
- cosmic-mixtapeの13比較画像を保存し、検索/ヘルプ両タブ/設定/日本語・英語メニューを比較した。
  3言語×3倍率の操作メニュー、最小幅/無効/選択/下線も画像で確認した。
- 初回の下線画素検証には27 failuresがあった。透明文字を同じbitmapへ重ね描きしていたfixtureの問題で、
  2回目の描画前に地を初期化して再検証した。初回ログを保存し、成功に読み替えない。製品コードはこの修正で変えていない。
  stderrは既知のinterception_win起動warningのみで、ASan/UBSan/nullabilityのエラー診断は無い。

通常960/560幅は全項目とキー列が収まった。一覧320幅の省略は、英語カテゴリ120/144と英語ノート120/144/168の5条件に残る。
不足幅は順に6/14pxと42/58/36px。#379の保存済み測定と比べ、日本語は収まるようになった一方、
英語ノートは等幅書体と共通字間によって必要幅が増える（120 DPIは不足16→42px）。幅全体の非悪化は主張しない。
固定の最小幅と既存の末尾省略を保ち、全文のUIA情報を維持し、省略時の孤立した下線を解消した。

ソースhashと最終集計は`verification-final.json`、実行終了コードは`fixture-result.json`を参照する。
最終PR headの必須CIは`pwsh -NoProfile -File ./eng/check.ps1`。CI/統合/通常exe/整理は実際のPRと`completion.json`を照合する。

## 限界と整理

実UIA reader、物理IME、物理DPI移動、DWM呈示は未測定。入力/コマンド実行は変更していない。
通常exeが稼働中ならhideが閉じてから反映し、アプリや未保存本文を終了しない。公開Releaseと自動起動はしない。
統合/必要反映/恒久成果物を確認後、このtaskのworktreeと不要出力を整理する。branch/commitは保持する。
以前に削除を拒否された#368/#373/9箇所は再試行しない。
