# #520 — 原文を保つ画像表示と持ち出し

対象: #520 / FR-043。規則: ARC-001〜012、C-001〜018、QLT-008/009/012/013、GIT-001〜004。
決定: [ADR0085](../adr/0085-lazy-markdown-images.md)。実装/対象検証時のbranchは`feat/520-markdown-images`。

## 変更と正典経路

既存block/inline/table parserの一回の走査からRTFと画像参照を作り、同じRichEditへ代替画像を先に表示する。
applicationの必須image port、adapter単一worker、WinHTTP/WIC、native画像差替えへ接続した。
本文と画像の原本を変更せず、元パスも自動修正しない。加工中の動くメーターは完了率を表さない。
ZIPは同じparser/portから参照するimages/内画像を収集し、外部ローカル画像は明示選択時だけ同梱する。
書換えは出力コピーのdestination範囲だけで、HTTPSはURLを保持する。既存ZIP writerと新規公開経路を使用する。

設定/schema/製品版/配布依存は変更しない。OS同梱windowscodecs/winhttpを宣言済みplatformLibrariesに追加した。
操作説明はMARKDOWN/TRANSFER、規範はSPECIFICATION/PROJECT_LAYOUT/CODING_RULES/GLOSSARYを更新した。

## 実行した対象検証

原本は`D:/NeNeFolio/outputs/520-markdown-images/`の各新規run directory。
`tools/record-run.py`がargv、raw出力、終了値、明示input/artifactのsnapshot/hashを保存する。
build入力は`build-inputs-NN.json`と同runのsnapshotで識別する。

| 記録 | コマンド/対象 | 結果 |
| --- | --- | --- |
| build-30、build-31 | `pwsh -NoProfile -File .../build-images.ps1`。正典CMake Debug、ASan/UBSan/nullability・clang-tidy | 成功。31はnative試験から時計読込を除いた変更 |
| unit-images-06 | `build/folio_tests.exe --images` | 成功。画像構文/code除外/escape/リンク、原文span/ZIP、移動/改名取消、UI文言と既存関連境界 |
| native-display-09 | `build/image_display_tests.exe` | 53checks成功。実Msftedit、literal marker衝突、原文/選択/dirty/Undo/リンク/検索、画像寸法/表/幅変更/scroll |
| native-adapter-01 | `build/image_adapter_tests.exe` | 1353checks成功。PNG/JPEG、4096×1024→2048×512、再利用/原bytes更新、破損/欠損/容量超過、取消 |
| native-status-02 | `build/verification-images/image_status_tests.exe` | 39checks成功。専用desktop/dataの実主窓で本文先行、読込/加工表示、編集取消/再試行、カテゴリ移動後の新しい基準と選択保持 |
| native-export-02、zip-readback-01 | adapterの`--export`と独立Python zipfile読取 | 成功。3entries、原画像2件の全bytes一致、先頭FEFF/CRLF/title/HTTPS保持、出力コピーだけ調整 |
| native-https-03 | GitHub公開PNG（query/fragment付き）を実WinHTTP/WICで取得 | 成功。非同期通信への変更後 |
| native-https-deadline-01 | httpbingoの10秒dripを`--download-case`で取得 | 10.016秒で失敗として完了。累積10秒で取消開始する境界 |
| native-https-cancel-02、native-https-shutdown-01 | 同dripの取消/adapter破棄を`observe-network.py`が外側で計測 | 成功。取消2秒未満、終了4秒未満。詳細時間はraw出力 |
| native-https-cert-01、native-https-redirect-01 | 期限切れ証明書、HTTPS→HTTP redirect | いずれも拒否。証明書検証を無効化していない |
| native-https-cap-02 | Hetzner公開100MB fileの先頭を上限まで取得 | `IMAGE_TOO_LARGE`で拒否。16MiBを超えたら停止 |

製品編集前の成立性診断`native-probe-09`は23checks成功、`wic-probe-01`は8192×8192の縮小125ms。
成功した関連入力が不変の検証は再実行しない。構文/ZIPの確保失敗注入は正典Debugではinactiveと明示され、
全分岐の90%条件と注入実行は最終CIの既存`eng/coverage.py`で判定する。

## 途中結果と修正

全build/試行のraw記録を保持する。compile/静的解析指摘は修正し、閾値/除外を変えていない。
native表示の診断ではRichEditのreadonly・hidden marker位置・表のquery位置を修正した。
ZIP最初の試行はfixtureのFEFF表現期待を修正し、成功原本と独立読取を別記録へ保存した。
最終規約確認で試験コードの時計読込と文書中の架空相対リンクが拒否され、相対timerと外側の計測へ変更した。

native-https-02のローカルTLS試験は証明書登録が完了しなかったため中止した。
同試験だけのprocessを終了し、秘密鍵fileを削除。`https-loopback-01/cleanup-recheck.json`で
対象thumbprintの信頼登録0、秘密鍵fileなし、processなしを確認した。
native-https-slow-01は相手側の20秒指定拒否（HTTP400）で、期限の証拠には使用しない。
native-https-cap-01は相手の応答拒否で、容量上限の証拠には使用しない。

必要な読み取り専用の独立レビューを実施。カテゴリ間移動・カテゴリ改名の旧結果失効、改名未完了中の
新規要求拒否を追加し、再確認で重大な未解決指摘なし。カテゴリ改名では他カテゴリからの相対/絶対参照も
影響し得るため現在画像全体を失効する。画像/ノートの移動を増やす処理ではない。

## 統合と残る範囲

完了条件は最終PR headの`pwsh -NoProfile -File ./eng/check.ps1`と必須check成功。
CI/head/main、通常exe反映、恒久収載と整理の確定原本は
`D:/NeNeFolio/agents/markdown-images-20261102/integration-completion.json`へ記録する。
ここでは未実行のCI成功/通常反映を主張しない。

WinHTTPの10秒は取消開始までの予算で、OSの最終closing通知までの厳密な時間保証ではない。
WICの一回の同期復号は途中中断できず、上限内で完了後に旧結果を破棄する。
RichEdit内部copy/WIC作業領域はcacheの32MiB・pane保持64MiBとは別に存在する。
SVG/GIF/WebP・EXIF回転の特別処理・外部ファイル監視・ネットワークドライブ/reparse pointは対象外。
専用desktopの合成操作は物理IME・全DPI/OS・実readerの保証ではない。実Store配布も今回の範囲外。

Waiver: [WVR-0001](../waivers/WVR-0001-winhttp-callback-abi.md)。固定5引数のOS callbackだけ。
行数60/ネスト3の小さな通知関数であることをレビューし、認知的複雑度の機械検査は維持する。
