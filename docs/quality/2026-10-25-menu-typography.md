# #379 — メニュー項目を既存の14px文字階層へ揃える

日付: 2026-10-25。基点main: `dd96e09e8184403bf136fdf8b9327e528d89813c`。
規則: ARC-001/004/012、C-008/013/016/017、QLT-008/012/013、GIT-001〜004。Waivers: none。

## 問題と判断

hideの指摘を受け、現行の操作メニューと周囲を画像/コード/native実測で比較した。
項目名は言語別face16、周囲とキー列はConsolas11で、両方とも標準の太さだった。
日本語は周囲の字形がfont linkで補完されるため実glyphのfaceは断定せず、サイズ差を主因と判断した。
英語では等幅/比例幅と字間の差も見える。

hideはリナによるdesign skillの批評を依頼したが、Fableは利用上限HTTP429で開始できなかった。
その後hideがOpusへの切替を明示し、ClaudeCode CLIの`claude-opus-5-5`が`design:design-critique`を実呼出し、画像4枚をReadした。
原記録と親の照合は`D:/NeNeFolio/agents/menu-design-review-20261025/`にある。
元レビューの「Consolas11への変更はADR0032違反」と「英語の幅が増える」は確定根拠に採用しない。
ADR0032は主窓の自前描画をConsolasに保つと明記しており、後者も未測定だった。

採用した案は、言語別faceを保ち項目名を16から14 logical pxへ下げ、名前入力/失敗箱/ノート一覧とサイズを揃えること。
hideの「対応して」を受け、ADR0060決定17へ追記してから実装した。

## 変更と範囲

`folio_window.c`の`show_menu_surface`で、既存menu_font生成のサイズだけを14へ変えた。
操作/カテゴリ/ノート/空白の全MENUが同じ生成・実幅計測・描画を使う。
言語別face、標準の太さ、キー列11、行高32、余白・色・枠、入力/UIAと実行経路は不変。
設計文書、現在状態、品質記録、日報/引き継ぎを更新する。公開API・保存schema・依存・品質ゲートは変更しない。
実装/検証は単体。変更前の明示依頼による批評を再利用し、この軽微な変更で独立レビューを繰り返さない。

## 対象検証

恒久証跡: `D:/NeNeFolio/agents/menu-typography-20261025/`。
作業checkout: `D:/NeNeFolio/worktrees/379-menu-typography`。
生成物: `D:/NeNeFolio/outputs/menu-typography-20261025/`。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target nenefolio
python eng/conformance.py
git diff --check
pwsh -NoProfile -File D:/NeNeFolio/agents/menu-typography-20261025/build-fixture.ps1
# 生成先runtimeを作業directoryにして、各DPIで直接実行（標準出力/標準エラーを証跡へ保存）
.\fixture.exe 120
.\fixture.exe 144
.\fixture.exe 168
```

- Debug build/clang-tidy、conformance 0 violations、空白検査が成功。
- fixtureは製品の`folio_window.c`を外部で一つのtranslation unitにし、完成候補のライブラリと結ぶ。製品への検証口は追加しない。
- CreateDesktop/SetThreadDesktopによる専用desktopで合成1カテゴリ/1ノートだけを使い、入力desktopへ切替えない。
- 日本語/英語/簡体字、120/144/168 DPIのフォント/配置、通常960幅/最小560幅/一覧320幅で、操作/カテゴリ/ノート/空白を確認する。
  320の一覧モードに存在しない操作ボタンは対象外。OS実DPIは120、144/168はfolio_window側GetDpiForWindowの外部置換による描画倍率であり、物理DPI移動試験とは区別する。
- フォントのLOGFONT/face、独立したGDI文字幅、行高、全行の可視化、hitとUIA用の行矩形、全文Name/AccessKey、開閉、画像を確認する。
  実UIA reader接続や実操作の追加試験ではない。入力/コマンド実行の実装は変更していない。

- 完成候補は99メニュー条件、各倍率864 checks / 0 failures、合計2,592 checks。各process exit 0。
  通常幅/560幅では項目名とキーが全て収まり、行高と全行の到達、UIA用矩形/Name/AccessKeyも一致した。
  3言語×3倍率の操作メニューと、最小幅/選択/無効項目/アクセスキー下線の画像を目視した。
- 一覧320幅では既存の末尾省略が残る。修正前mainの同じfixtureを3言語×3倍率×カテゴリ/ノートで比較し、
  486 checks / 0 failuresと18条件の幅比較で悪化0を確認した。省略条件は12件から6件へ減った。
  残るのは日本語カテゴリ120、英語カテゴリ120/144、英語ノート120/144/168。120 DPIの不足幅は順に10/11/16pxで、修正前32/33/40pxより小さい。
  省略された末尾のアクセスキー下線が孤立する既存表示も残る。文字幅/省略/下線の別件修正は混ぜない。
- 初回fixtureはStart-Processの出力リダイレクトを伴うrunnerの期限を超え、実行中の合成データ専用processだけを停止した。
  部分ログは保持し、完了へ読み替えない。直接実行＋ファイル出力に切り替えた完成候補は3倍率全体で約9秒で終了した。
  初回の「最小幅も省略ゼロ」という広すぎる判定を、修正前比較に基づく非悪化と全文UIA確認へ分けた。
  規範ゲートや製品コードはこの切分けで変更しない。stderrは既知のinterception_win起動warningだけで、ASan/UBSan/nullabilityの診断は無い。

検証の最終集計と実行終了コードは`fixture-result.json`、ソースhashは`verification-final.json`を参照する。
最終PR headの必須CIは`pwsh -NoProfile -File ./eng/check.ps1`。CI、統合、通常exe反映、整理は実際のPRと`completion.json`を照合する。

## 限界と終了時整理

画像は専用desktopのGDI/PrintWindowによる。物理IME、実reader、DWM呈示、物理DPI跨ぎは未測定。
通常exeが起動中ならhideが保存して閉じてから反映し、稼働アプリを終了しない。公開Releaseと自動起動は行わない。
統合/必要反映/成果物収載後、このtaskの追加worktreeと不要出力を整理し、branch/commitを保持する。
過去に削除が拒否された#368/#373/9箇所は今回の対象外で、再試行しない。