# #294 — NeNeNibの125テーマ・検索選択・設定版4

- 日付: 2026-10-12。基点main `e992d7e`、枝 `feat/294-theme-catalog`。
- 契約: [ADR0049](../adr/0049-builtin-theme-catalog.md)、FR-016、ARC-001/003/004/008/009/011、C-002/003/012/014/016/018、CNF-009/010/011、QLT-008/009/012/013。
- 恒久証跡: `D:/NeNeFolio/agents/294-theme-catalog/`。Waivers: none。

## 実装と確認範囲

NeNeNib `5b7dbb2791785e508384fd3a7e82cb01f40c8ebb` の実builtin tableから125種類を取り込んだ。
元の本文背景/前景を保持し、Folioで必要な役割へ固定RGBを割り当てる。既存light/darkを含む127配色をcoreの1表へ集めた。
systemを加えた128候補はsettings.jsonとExが同じ名前解析を使い、設定面で部分検索・色見本・クリック/Enter採用ができる。
Nibの実行時参照・C++依存・生成工程は製品へ追加していない。テーマIDはデータ、説明はui_textの3言語表に置く。

settings.jsonは版4へ進めた。旧版1〜3を読み、次の設定変更で版4を書く。起動時の書き換えはしない。
旧版2/3で新しいIDを受けないこと、未知語・埋め込みNUL・未来版を拒否する境界と、設定を書いてから採用する原子性を維持した。
出典は`docs/theme-sources.json`、Nibのnotice/ライセンス全文は`THIRD_PARTY_NOTICES.txt`へ保持し、既存release layoutへ加えた。

## 実行した対象検証

全て作業treeをcwdに実行した。以下のbuildは固定toolchain/clang-tidy/sanitizer付きDebug。

| 確認 | 実コマンドと証跡 | 結果 |
| --- | --- | --- |
| 整形・configure・build | `check-candidate.ps1` → `check-corrected.ps1`、続く `cmake --build D:/NeNeFolio/outputs/294-theme-catalog/build --target folio_tests` / `--target nenefolio` | 最終差分build成功。`build-unit-final.log` / `build-picker-final.log` |
| 対象unit | `D:/NeNeFolio/outputs/294-theme-catalog/build/folio_tests.exe --themes` | catalog/settings/state/Ex/ui_text成功。`unit-final.log` |
| 実buildグラフを含む規約 | `python eng/conformance.py --build-dir D:/NeNeFolio/outputs/294-theme-catalog/build` | 0違反。`conformance-final.log` |
| 中核の外部依存 | `python eng/symbols.py --build-dir D:/NeNeFolio/outputs/294-theme-catalog/build --require core application` | 2 library、0違反。`symbols-final.log` |
| 配布境界 | `python eng/test-release.py --artifact D:/NeNeFolio/outputs/294-theme-catalog/build/NeNeFolio.exe --metadata D:/NeNeFolio/outputs/294-theme-catalog/build/product-version.json --workspace D:/NeNeFolio/outputs/294-theme-catalog/release-proofs` | 16件成功。`release-proofs.log` |
| 固定表とnotice | `python -X utf8 D:/NeNeFolio/agents/294-theme-catalog/verify-catalog-boundaries.py` | 旧配色54役割完全一致、CNF-009欠落/重複拒否、noticeの配布宣言/hash成功。`catalog-boundary-proof.json` |
| Win32実操作 | `pwsh -NoProfile -File D:/NeNeFolio/agents/294-theme-catalog/run-native.ps1` | 148 assertions、0失敗。`native-results.json`、PID43688、exit 0 |
| 最小窓の変更境界 | `pwsh -NoProfile -File D:/NeNeFolio/agents/294-theme-catalog/run-native-general.ps1` | 54 assertions、0失敗。`native-general-results.json`、PID62668、exit 0 |
| 差分 | `git diff --check` | 成功 |

unitでは125の名前と元RGB、127配色×19組の可読文字コントラスト4.5:1以上、128値の保存往復/Ex解決、
検索の大小無視/0件、旧設定の移行、未知名・NUL拒否、書込失敗時のstate/RTF不変、起動時非書込を確認した。
成功後の変更はUI空行クリックのみで、変更のないunit結果を再利用した。

初回buildはtheme_navigateの複雑さ14とclick_command_surfaceの11が閾値10を超えて停止した。
移動量計算とパレットクリックを意味ごとの関数へ分け、抑制/閾値変更をせず成功した。
初回unitは語彙検証が旧parse_optionだけを呼ぶため停止し、themeを同じ実Ex経路へ照合するよう修正した。
`build-initial.log`、`unit-corrected.log`に失敗を残す。
独立読み取りレビューで、既存labelと不一致なテスト、新ID文言期待表の漏れ、settings名のstrlenによるNUL切詰め、
候補のない余白クリックで不正な選択位置を作るP2を修正した。最終レビューに未解決のP1/P2はない。

## 実ウィンドウと実画像

実adapter/ports/state/fonts/windowを候補libraryと同一process/threadへ生成し、専用dataと非表示の別desktopで実行した。
通常desktopのforegroundは両実行とも67378→67378。SwitchDesktop・SetForegroundWindow・物理キー送信は使っていない。

- 検索/0件/矢印/ページ/ホイール/クリック/Enter、末尾候補への到達、単一候補の余白クリック、Esc/戻る/×、元editorへのfocus復帰を確認した。
- 合成WM_IME_STARTCOMPOSITION/ENDCOMPOSITIONでEnterとクリックの採用抑止/復帰を確認し、実HACCELで背後layout切替が遮られることを確認した。
- dirty編集のUTF-16本文SHA256、変更印、Undo可否、選択、表示論理行を保持し、最後に実Undoで元の挿入が消えることを確認した。
- 実settingsを共有モードで保持して置換を失敗させ、採用テーマとsettings bytesの不変、解放後の再試行を確認した。
  md/categories/indexのhash/mtimeは不変。新しいapplication stateが版4のdracula/jaを再読込した。
- 日本語/英語/簡体中国語の320 logical px、最小320×360の行番号到達と失敗理由、通常幅の明暗/0件/editorをPNG16枚で視認した。
  120 DPI、400×650/400×450/1000×750物理pxを同一process PrintWindowで採取し、窓矩形とbitmapの一致を記録した。
  狭幅一般設定の下部はTab/スクロールで到達し、候補・案内・保存失敗の理由は最小サイズでも読めた。

初回nativeの141件も成功したが、最終148件で空行の反例と実320幅を追加したため、受理対象は後者である。
最終runnerの最初の起動はworker生成前に停止し、原因は特定していない。ログ名/desktop名を分離した再実行は成功し、結果を保存した。
`native-stderr-picker-final.log` / `native-general-stderr.log`にASan interceptionの非致命警告1行がある。
同環境の既知挙動（#292品質記録）と同じで、今回の両workerは全assertion成功、sanitizer実失敗なし、exit 0だった。

候補exe SHA256: `6E37D04C1CCA34E8B5907C54FE7FAE3F5111EF14DB28C31806E68F1FC39732FD`。
`candidate-inputs-final.json`に入力のblob/hashを保持し、統合時は同じ入力であることを照合して結果を再利用する。
最終headの必須CI・main・通常exe反映/起動・終了整理は紐付くPRと同証跡ディレクトリの`completion.json`を正本にする。

## 限界

全125配色それぞれの実画面目視、物理IME実入力、別OS/DPI、実スクリーンリーダー、hideの実機受理は未測定。
外部の利用者テーマファイルと公開済みReleaseの更新は今回の範囲へ含めない。
最終headの正典フルゲートは必須CIで実行し、対象検証済みの機能を工程が変わるだけで再測定しない。
