# #398 — v0.3.0 beta更新の確認

日付: 2026-10-27。hideの実機サンプル受理とベータ版更新依頼に基づく配布準備。
Issue: [#398](https://github.com/hideyukiMORI/nene-folio/issues/398)。基点main `063f5bab`。
規則: FR-014 / ARC-001・002・007・011 / QLT-008・011・012・013 / GIT-001〜004 / ADR0045。Waivers: none。

## 版と変更範囲

runtime版の唯一の入力である`CMakeLists.txt`のproject VERSIONを0.3.0へ変更する。
Windows資源・manifest・設定面・配布名は既存の生成経路へ従い、別の版定数を追加しない。
README、配布README、[リリースノート](../release/0.3.0.md)、[Markdownの対応範囲](../MARKDOWN.md)と現在状態を更新する。
生産C・保存schema・設定・依存・gate・packaging仕様は変更しない。旧Releaseと旧release文書は維持する。

## 根拠と再利用

v0.3.0は#392/#394/#396の成果を配布へ含める。[ホイール](2026-10-26-view-wheel.md)、
[Markdown表示](2026-10-26-markdown-style.md)、[構文](2026-10-26-markdown-syntax.md)の品質記録と
各taskの`completion.json`を根拠とし、入力不変な機能試験は再実行しない。
hideは通常dataの合成サンプル`data/サンプル/Markdown構文サンプル.md`を実機で表示して受理し、ベータ版更新を依頼した。
これは専用desktopの自動検証と区別する。通常アプリPID56180は操作・終了せず、通常dataを配布しない。

## 配布と検証の計画

[workflow§9](../DEVELOPMENT_WORKFLOW.md#9-リリース)と[ADR0045](../adr/0045-single-version-and-verified-release-bundle.md)へ従う。

| 工程 | この候補文書作成時の状態 | 確定情報 |
| --- | --- | --- |
| 版/文書の差分・参照・機密混入の自己確認 | 完了。対象9ファイル、ローカル参照47件と版入力を確認 | `document-validation.json` |
| 最終PR headの必須CIとmain統合 | 未完了 | 実PR/CIと`completion.json` |
| clean mainから正規配布器でRelease生成 | 未実施 | 配布器の検査結果と入力hash |
| AMD64/import/版/manifest/fonts/license/ZIP集合/hash | 未実施 | 正規配布器の検査結果 |
| 新しい展開先でdataなし起動・設定版0.3.0 | 未実施 | 専用smokeのPID/実行パス/画像/結果 |
| v0.2.0の合成dataをcopyして新版で確認 | 未実施 | 旧版と新版の合成data/hash比較 |
| v0.3.0 tag/Pre-release/ZIP/SHA256SUMS公開 | 未実施 | 実Release/tag/添付物 |
| 公開後の再取得照合 | 未実施 | download照合結果 |
| 恒久保管・安全監査・今回一時物の整理 | 未完了 | `completion.json` |

この時点では版/文書の自己確認まで完了し、buildや全件テストは実行していない。配布器でDebugフルゲートを重複実行せず、
最終headの必須CIとRelease成果物検査を区別する。公開前に存在しないAssetsへの候補リンクは公開後の再取得で確定する。

## 確認範囲と限界

今回の機能差分は[リリースノート](../release/0.3.0.md)にまとめた。
list内quoteから親listの継続本文へ戻る組合せは未保証、表は63列まで、リンクは装飾のみである。
HTML/画像埋込み/完全CommonMark互換等は対象外。保存先設定#390、署名、インストーラ、Store、自動更新も未実装/対象外。
専用smokeは全OS/物理IME/実reader聴取/DWM呈示を一括して保証するものではない。

## 証跡と完了追跡

恒久証跡: `D:/NeNeFolio/agents/beta-release-20261027/`。
統合・Release生成・smoke・公開・再取得・保管・整理の最終確定値は同所の`completion.json`と実際のPR/Releaseを照合する。
この文書は計画と候補時点の状態を記し、未実施を成功へ置き換えない。設計サナが公開状態の文章と確定結果を更新する。
通常アプリを操作せず、旧削除拒否済み14箇所も再試行しない。
