# ADR 0045 — CMakeの版からWindows資源と検証済み配布ZIPを作る

- 状態: 受理（完成候補のRelease生成・対象proof・設定表示を確認。設計: AstraのNeNeFolioサナ）
- 日付: 2026-10-12
- Issue: #282
- 影響する規則: FR-014 / ARC-001 / ARC-002 / ARC-007 / ARC-011 / C-018 / QLT-007 / QLT-010 / QLT-011 / QLT-012 / QLT-013
- 関連: [ADR0003](0003-c23-clang-cl-foundation-and-measured-limits.md)、[ADR0036](0036-bundled-fonts.md)、[ADR0044](0044-tests-at-completion-and-integration.md)

## 文脈

main `7ff9b96`では、`project(NeNeFolio VERSION 0.1.0)`が唯一の版入力として宣言されているが、
exeのVERSIONINFO、manifestのidentity、設定面に結ばれていない。ワークフロー§9の配布コマンドも未実装である。
hideの「配布準備を進める」を受け、版の一元化と実際に展開・起動できるZIPを一つの完成単位とする。
公開tagとGitHub Release、版番号の引上げ、インストーラ、署名、自動更新は本Issueの対象外である。

appの入力は`nenefolio_target`で所有を確認している。生成C/RCを後から`target_sources`で足すと、この経路を外れる。
現在のC用警告・sanitizer設定は言語条件が無く、RC導入時にCだけへ限定する必要がある。
書体は`fonts/manifest.json`がサイズ・hash・ライセンスの正本で、CNF-012が既に検査している。
この一覧を配布用に再度手入力すると、追加や変更時に不一致を生む。

## 決定

**CMakeの版入力を静的RC・設定面・配布metadataへ供給し、一つの配布コマンドが新しい作業先で作成・照合したZIPだけを出力する。**

1. `project(... VERSION)`以外に製品版を手入力しない。表示と配布名は`PROJECT_VERSION`、Windows固定版とmanifestは
   不足成分を0で補った4成分を使う（例: `0.1.0`と`0.1.0.0`）。Windowsの各成分は0〜65535とし、範囲外をconfigure時に拒否する。
   CMakeからcoreの版参照とRCへcompile definitionを供給し、配布スクリプト向けmetadataも同じ値から生成する。
2. 追跡済みの`src/app/NeNeFolio.rc`と`NeNeFolio.manifest.in`を正典app targetのsourceへ宣言する。
   templateはコンパイルしない資源入力で、configureしたmanifestへのRC依存を`OBJECT_DEPENDS`に明示する。
   生成C/header/RCや所有検査の例外は追加しない。全ての既存Cオプションを維持したまま`COMPILE_LANGUAGE:C`へ限定する。
3. RCがVERSIONINFOとresource ID 1のmanifestを埋め込み、`/MANIFEST:NO`でリンカによる別のmanifest生成を止める。
   manifestには版付きidentity、既存のPerMonitorV2とDPI aware、Windows対応OS宣言を含め、
   `requestedExecutionLevel=asInvoker` / `uiAccess=false`を明示して従来の非昇格実行を保持する。
4. 設定面の上部の既存の空き領域に読み取り専用の版を表示する。最後の非選択行には足さず、小さい窓や一覧のスクロール中も表示を保つ。
   利用者向けのラベル・書式は既存`ui_text`の3言語表に置き、
   値だけを版参照から渡す。settings.jsonへ保存せず、設定schemaを変更しない。
5. 配布生成入口は`eng/package-release.ps1`とする。固定toolchainで、単一ゲートと別の新しい作業先にReleaseを構成・ビルドする。
   WorkDirectoryとOutputDirectoryは絶対パスで明示し、既存パス、ソース配下、互いに重なるパス、reparse pointを通るパスを作成前に拒否する。
   コマンドは既存ディレクトリの削除やアプリ終了を行わない。失敗時の作業先は診断用に残し、出力先への完成品公開は全照合の後に行う。
6. 配布の固定ファイルと命名規則は`eng/release-layout.json`一か所に宣言する。書体・ライセンスは既存のfont manifestから展開する。
   ZIP内の一つの製品フォルダにexe、配布用README、MIT LICENSE、`fonts/`を入れる。`data/`、PDB、開発用ファイルを入れない。
   初回はdataなしで起動でき、利用者の保存に応じて既存の保存経路がdataを作る。
7. 配布検査は同じ一覧を使い、実ファイル集合、AMD64、実exeの固定/文字列版、埋め込みmanifest、宣言されたOS DLLだけのimport、
   書体のサイズ/hash/licenseを確認する。OS DLLの許可集合は`architecture.json`から導出し、配布専用の別allowlistを作らない。
   ZIPのエントリと展開結果をstageと照合した後、ZIPと`SHA256SUMS`を完成品として公開する。
8. 完成候補で配布生成、検査器の反例、dataなしの起動と設定版表示をまとめて確認する。
   反例は余計なファイル、欠損・破損した資産、不一致版・不許可importなどの拒否境界を狙い、反例ごとに製品を再ビルドしない。
   `eng/check.ps1`には配布検査器の対象proofを結び、最終headの必須CIを維持する。配布コマンド自体でフルゲートを重複実行しない。
   公開候補を作る際は統合済みclean mainから同じコマンドを使う。本Issueは準備用成果物までとし、公開は別の依頼で行う。

## 強制

- Cの品質設定とsource所有: **active**。既存`nenefolio_target`、File API検査、clang-cl/clang-tidyを維持する。
- 版の供給と配布検査: **active**。CMakeのcompile definition/資源生成、`eng/package-release.ps1`と`eng/release.py`。
  配布一覧は`eng/release-layout.json`、font検査は既存`conformance.bundled_asset_checks`を再利用する。
- 検査器の反例: **active**。`eng/test-release.py`の16件を`eng/check.ps1`へ結線する。追加ビルドせず既存exeを使い、
  Debugは版・manifest・AMD64の同一性、Release配布は加えてimport許可集合を検査する。
  対象実測は[品質記録](../quality/2026-10-12-release-prep-checks.md)。最終headの必須CI条件は別途維持する。
- 設定面の可読性と配布物の実OS起動: **不能**（対象環境での実測）。結果を品質記録へ残し、未測定DPI/OSまで証明したと扱わない。

## 結果

版と資産の二重管理を増やさず、自己完結した配布候補を作れる。配布作業で既存の成果物やノートを削除しない。
生成入力の種類をmanifest資源と配布metadataに限り、汎用の生成コード所有管理は導入しない。
対象はWindows x64と固定toolchainで、署名済みインストーラや自動更新は提供しない。
archive・hash・起動の確認は発行者の認証、全OSでの可動性、hideの目視受理まで保証しない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| RC、C header、ZIP名へそれぞれ版を書く | FR-014の単一入力を失い、更新漏れを生む |
| generated sourceを後付けし、所有検査を除外する | ARC-002の正典経路を迂回する |
| この変更のために汎用生成コード管理を新設する | 静的RCと生成資源入力で足り、無関係な所有検査の変更を増やす |
| build全体をZIPへ詰める | dataやデバッグ資産の混入を防げず、配布一覧を保証できない |
| 既存出力先を削除して再利用する | 別の配布物や利用者データを壊す余地がある |
| Release生成のたびに全Debugゲートも実行する | 同じ最終headの必須CIと重複し、ADR0044の実行集約に反する |

## 参照

- [Microsoft VERSIONINFO](https://learn.microsoft.com/en-us/windows/win32/menurc/versioninfo-resource)
- [Microsoft /MANIFEST](https://learn.microsoft.com/en-us/cpp/build/reference/manifest-create-side-by-side-assembly-manifest?view=msvc-170)
- [CMake 3.31 OBJECT_DEPENDS](https://cmake.org/cmake/help/v3.31/prop_sf/OBJECT_DEPENDS.html)
