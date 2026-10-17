# OS高コントラストの実装と対象検証（#320）

契約: FR-016、ARC-001/003/004/007/008/010/011、C-002/003/012/016/018、QLT-008/009/012/013、ADR0049/0053/0054/0055 B/0057。
基点はmain `2f037e9`。専用枝は`feat/320-system-high-contrast`。

## 変更

appearance_adapterだけがSPI_GETHIGHCONTRAST/GetSysColorを読み、結果enumとsnapshotを既存portへ渡す。
applicationが最後の有効snapshotと実効paletteを所有し、coreの同じ解決をUI/RTFが共有する。
127固定配色と保存選択/カテゴリ色を保持し、HC ONはOSの前景・背景対、OFFは保存選択へ戻す。
同じ明暗enumの色変更、取得/RTF準備失敗、本文投入/終了再入、開いた設定・名前/失敗面を扱う。

RichEditの再着色は取得/停止/書式/再開の失敗を区別して通知し、同じsnapshotでも再試行する。
再開失敗中は本文を保持して編集だけ停止。別対象を採用した後の投入拒否はUNAVAILABLEへ渡し、
旧本文を新対象へ保存させない。設定フォーカスは内枠でも示し、選択行のキー別名はhighlight対で描く。

## 実行した検証

証跡ルートは`D:/NeNeFolio/agents/320-system-high-contrast/`。

- 固定toolchainで`cmake --build D:/NeNeFolio/outputs/320-system-high-contrast/build --target nenefolio folio_tests`。
  製品build/tidy成功。最後の製品差分は`build-10.log`で成功。
- `folio_tests.exe --system-contrast`成功（`unit-02.log`）。任意明暗色、選択保持、取得失敗、RTF準備失敗の分岐、三言語文言。
  確保失敗注入は測定ビルドで有効になり、最終CIでも同じ試験を実行する。
- `compile-native.ps1`で実製品libraryとfolio_window.cを使うC fixtureをASan/UBSan付きで構築。
  `NativeDesktop320.Run`の別desktopで対象fixtureだけを実行した。入力は合成dataで、通常dataは使わない。
- `native-results-04.log`: 270/270成功。system/固定、dirty EDIT/VIEW、明暗2組、同じenum内の色変更、OFF、
  本文UTF-16完全一致、変更印、選択、表示論理行、実Undo、OS通知の書込0、設定面、TOM/書式/Undo停止・再開/stream故障と再試行。
- `native-results-visual.log`: 22/22成功。HC内枠の有無の画素、選択キー別名の前景、見出し/リンク/コード、テーマ候補、開いた名前/失敗面の追随。
  テーマ色見本の下余白を調整した最終実装で、この22項目だけ再確認し、内枠と色見本の画像も確認した。
- `native-results-real.log`: 35/35成功。実SPIによるHC ON/OFF、実adapterの値、保存固定選択への復帰、dirty本文・選択・表示行・Undo保持。
- `native-results-recovery2.log`: 20/20成功。Undo再開故障中の通常A→Beta移動、旧本文取得/保存/OS終了拒否、書込0、回復後のBeta再投入と混入なし。
  実装を変えていない成功境界は再利用し、対象採用後の投入拒否だけを追加検証した。

合計347項目。`injected-*.png`、`name-dark.png`、`failure-dark.png`、`real-os-before/on/restored.png`を目視確認した。
実OS画像では行番号もOS色で描かれた。自作fixtureの直接失敗通知は記録へ置換し、名前/失敗ダイアログ自体は別の実表示試験で確認した。

## 実OS試験と失敗の記録

Windows 11 Pro build 26300の同一ホストで、別desktopの合成fixtureと復元controllerを使った。
SPI_SETHIGHCONTRASTはSPIF_UPDATEINIFILEを指定せず、before/on/restoredのflags・scheme・31色と画像を保存した。
最終`real-os-run-04.log`と`os-restoration-result.json`は成功。flags 126→127→126、31色、通常CurrentTheme、保存配色名の復元を確認。
独立watchdogは不要で終了した。通常アプリを閉じたり通常ノートを書き換えたりしていない。

初回は公式HIGHCONTRASTW文書のLocalFree記述に従った補助コードと製品の一時追加でheap corruptionが起きた。
その解放を撤回し、未使用の既定配色名ポインタを製品で所有しない元の読み取りへ戻した。失敗は`attempt01-*`に保存。
HC OFFと31色を手動復元し、Windowsが初回切替で初期化した保存配色名を空へ戻した。
SPIが返す既定名のキャッシュは空から「ハイコントラスト 黒」へ初期化されたままである。
これは保存値/現在配色の復元と区別して記録する。再試験ではそのcache値も前後一致、通常テーマ/保存値も一致した。
初回の受理失敗を成功試験へ数えていない。続く準備2回の失敗は旧stderrファイルの保持で、OSは切り替わっていない。

TOMの特殊Countは実MsfteditでS_FALSEも成功復帰する。S_OK限定で誤拒否した初回fixtureを残し、
FAILED/SUCCEEDEDへ修正後にE_FAIL注入と実Undoで確認した。単体の初回失敗は新文言IDの期待表不足で、補完後に成功。

## 独立レビューと完了の境界

サナが単体で調査・設計・実装・検証。必要な読み取り専用レビューのみcontrast_reviewへ依頼した。
focus/選択色、再着色失敗、Undo再開保持、次対象への旧本文混入の指摘を修正し、最終差分の追加blocker/highなし。
保存schema、外部runtime、ゲート閾値/除外、公開Releaseは変更しない。appearance_adapterのuser32依存だけ宣言を追加。
全reader/全OSテーマ/全UI公開の保証はしない。補助面のUIA公開は#321。

最終PR headの必須`pwsh -NoProfile -File ./eng/check.ps1`成功、main統合、通常exe反映、恒久証跡、作業物整理は
#320関連PRと`D:/NeNeFolio/agents/320-system-high-contrast/completion.json`で確定する。ローカル対象検証だけを統合完了とはしない。
Waivers: none。
