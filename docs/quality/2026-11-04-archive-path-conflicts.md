# ZIP内のファイルとディレクトリの衝突を拒否する — #593

`images/_nenefolio`という画像ファイルと外部ローカル画像を同梱すると、ZIPに
`A/images/_nenefolio`と`A/images/_nenefolio/1.png`が入り、同じパスがファイルとフォルダーを兼ねていた。
修正前は参照の両順序で書き出しが成功したが、実際の展開はFileNotFoundError/PermissionErrorになった。
`archive_assets`の既存追加経路で祖先/子孫の衝突を拒否し、画像書き出しをZIP公開前に中止する。

## 再現と正典経路

合成PNG17×11を専用fixtureの`A/images/_nenefolio`、`A/images/a.png`、`outside.png`へ配置した。
実`image_adapter_port`→`image_export`のALL_LOCAL→`note_archive`で、内側先/外側先/正常対照の3件を生成した。
現mainとの製品入力不変と既存Release3libのSHAを照合して再利用。修正前は全3件READY、原文/画像不変。
Python zipfileの独立読取で2件のfile祖先衝突と展開失敗、正常対照の展開成功を確認した。
初期原本は`D:/NeNeFolio/agents/export-path-conflicts-20261104/observation.json`。

Issue593とD側作業木をmain`1368ca4`から作り、製品編集前に既存unitへ対象例を追加しredを確認した。
その後ADR0085へ方針を追記し、製品は`src/core/archive_assets.c`だけを変更した。
既存ASCII大小文字同一視の走査を共有し、公開containsの完全同名という意味を維持する。
appendは完全同名、または一方の終端が他方の`/`境界に一致する祖先/子孫をgrow/copy前に拒否する。
既存の`NOTE_ARCHIVE_INVALID`→`FOLIO_STATE_EXPORT_IMAGE_UNREADABLE`へ返し、元Markdownを変えない。
自動改名/別配置/第2のZIP実装は加えず、owner/公開API/上限/schema/版/依存/gateを維持した。

ARC-001/002/003/010/012、C-002〜008/011/012/016、QLT-008/009/010/012、GIT-001〜004を自己レビューした。
小さなcore内の変更であり追加の独立レビューは行わない。追加waiverはnone、既存WVR-0001はWinHTTP callbackだけ。

## 対象試験と結果

既存`tests/unit/image_tests.c`へ14path対（衝突8/対照6）とapplicationの参照順序/大小文字3入力を追加した。
祖先/子孫の両順序、ASCII大小文字、深い子、完全同名、UTF-8を拒否し、単なるprefix/兄弟/別カテゴリを受理する。
拒否後の件数/path/元bytesを保持し、その後の正常追加ができることも確認する。既定CIにも含まれる。

| run | コマンド・観測 |
| --- | --- |
| 初期link-01 | 既存Release libraryで外部観測をlink、子/記録側0 |
| 初期observe-01 | link完了前に未生成exeをinputへ指定したため子未起動・記録125。失敗を保持 |
| 初期observe-02 | 実画像から3ZIPを生成、子/記録側0。衝突2件もREADYとなる反例 |
| 初期inspect-01 | 展開のFileNotFoundErrorが想定catch外で子1/記録125。rawを保持 |
| 初期inspect-02 | OSErrorを収集し別directoryへ展開。衝突2件の失敗と対照成功、子/記録側0 |
| build-red-01 | 固定Debug/clang-tidy、folio_testsとnenefolio_adapters、parallel2。子/記録側0 |
| red-01 | `folio_tests.exe --images`、file親子衝突の受理で子/記録側1、記録エラーなし |
| build-green-01 / green-01 | 同じbuildと`--images`、子/記録側0、stderr/記録エラーなし |
| verify-01 | 整形/conformance違反0/symbols2lib違反0/diff成功。子/記録側0、stderrはCRLF案内のみ |
| link-native-01 / native-01 | 正規lld/ASan/UBSan条件のDebug3libで同じ実fixture、子/記録側0、stderr/記録エラーなし |
| native-check-01 | Python独立読取、拒否ZIPなし/原文画像不変/対照ZIP全bytes一致/3file展開成功。子/記録側0 |

対象runは`tools/record-run.py`で入力snapshot・raw stdout/stderr・終了値を記録した。
修正後nativeの衝突2件は画像書き出し失敗となり、ZIP構築を呼ばずarchive変数はINVALID初期値のまま、ZIPを生成しない。
正常対照はREADY、修正前と全bytes/SHA256が一致（`27d188f82f5c2be56b633c98131bd24e900ddc1962f3ad83f272115fbe4d352a`）。
元Markdown3件/合成画像3件の全bytesまたはSHAを照合し、不変を確認した。
unit selectorは既存画像/Markdown/link/transfer/UI text境界を通る。allocation injection inactive表示を保持し、確保失敗成功とは呼ばない。

## 証跡・文書・限界

製品1file/unit1file、ADR/TRANSFER/品質記録/gate-proofs/日報/引き継ぎ/currentの7文書を変更する。
恒久原本は`D:/NeNeFolio/agents/593-archive-path-conflicts`。初期観測も含む3rootを全保全/全hash照合後に整理する。
最終head必須CI/正典統合/通常exe反映/data照合/保全整理の確定値は`integration-completion.json`へ収載する。
過去exec IDの参照不能と推測native.ps1読取の失敗もjournalへ残し、再実行や成功結果へ数えない。

任意ZIP reader/全Windows版/Unicode大小文字の全同一性/全heap/性能/実UIを保証しない。
ASCII比較とパス検証の既存規約を維持する修正であり、Unicode正規化や一般ZIP importerへ範囲を広げない。
実app/実ノートを試験へ使わず、Store再提出/公開は行わない。
