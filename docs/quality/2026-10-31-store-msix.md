# Store MSIX生成 — #484

## 範囲と正典経路

hideが開発者登録/製品予約を完了し、Store identityを提供した。版1.0.0と紙/ノートの図案作成もhideが指定。
単体で実装。`eng/package-release.ps1 -Format Msix`から既存Release buildと配布検査へ合流し、
`eng/msix.json`のidentity/画像情報を使って未署名MSIXを作る。版入力はCMake一箇所。
ZIPとMSIXのREADMEだけを配布形式に合わせ、exe/書体/licenseの正本と検証は共有する。
保存schema、製品のC実装、runtime依存は不変。Waivers: none。

## 検証と環境

Windows build26300.9818、既存固定clang-cl/clang-tidy/Python、MakeAppx10.0.26100.7705
（SDK directory10.0.26100.0）。作業先は`D:/NeNeFolio/outputs/484-store-msix/`。
対象実行は`tools/record-run.py`で入力snapshot、raw出力、artifact、子/記録の終了値を保全する。

| 記録 | 内容 | 結果 |
| --- | --- | --- |
| `icons-01` | Windows GDI+で独自図案をPNG化。44/150/50pxと512px確認画像 | 成功。候補を目視し、3同梱PNGの寸法/hashを固定 |
| `proofs-01` | `eng/conformance.py`、`eng/test-release.py`、`git diff --check` | 0違反、23tests成功、差分正常 |
| `package-01` | 正典Release/tidy・MakeAppx pack・追加照合 | build/pack成功、旧block map想定で追加照合が失敗。完成品出力なし |
| `unpack-probe-01` | 既存MSIXの全payload/64KiB block/whole-file hash、SDK unpack | 成功。検査修正の境界診断で、C再ビルドなし |
| `proofs-02` | SDK whole-file hash正例/反例を加えた同じ対象検証 | 0違反、23tests成功。474入力の事後hash不変、子/記録0 |
| `package-02` | `pwsh -NoProfile -File eng/package-release.ps1 -Format Msix`、明示した新work/output | Release/tidy・pack/unpack・15同梱ファイルの全照合成功。471入力不変、5artifact保全、子/記録0 |
| `zip-probe-01` | 検証済みReleaseを使う既定ZIPの補助driver | toolchainの変数名との衝突でdestination拒否。生成前停止、製品変更なし |
| `zip-probe-02` | 補助変数を固有名へ変更し、同じexe/metadataと既存`release.py assemble`を使用 | 既定ZIP作成・全内容・展開・版/import/font照合成功。追加Cビルドなし |

`package-01`の失敗はSDKが2021 namespaceの末尾`FileHash`を追加するため。
全体hashも実ファイルから算出して一致を要求し、未知の要素・重複・誤hashを許す除外は追加していない。
SDK unpackは`AppxBlockMap.xml`も取り出すため、検証済みarchiveの同ファイルとbytesを照合する。
MSIX内の集合はpayload+manifest+SDK管理2ファイル、展開後はpayload+manifest+block mapに固定する。

最終生成・ZIP互換確認・PR最終headの必須CI・main統合・成果物収載と整理は
`D:/NeNeFolio/agents/msix-20261031/integration-completion.json`を確定原本とする。
成功済みの対象確認は入力不変なら再利用し、単にpush/mergeするために繰り返さない。

枝のMSIX候補SHA256は`67bb54cbabae762a2ed89a520a417dbf3dd4ceb4f4ac1c0272483cc9faf26a07`。
提出用の保管候補はclean mainから同じ正典コマンドで生成し、そのsource/hashは上記確定原本へ残す。

## 未確認と次段階

実MSIXの署名/インストール/初回保存先/更新/アンインストール/WACK、Storeアップロード/審査/公開はこの生成確認に含まない。
開発用manifest登録も実MSIXのインストールと同一視しない。Storeページの最終アイコン受理は候補の作成と分ける。
AppDataに任意指定したノートrootは仮想化の影響があり得るため、外部Documentsと同じ保持を保証しない。
通常の実ノート/clipboard/focusを試験に使わず、起動中の通常exeを終了・上書きしない。
公開済みv0.3.0 betaはそのまま、#479/#466/停止済み継続改善は再開しない。

規則: FR-014、ARC-001/002/011/012、QLT-007/008/010/011/012/013、GIT-001〜004。
設計は[ADR0080](../adr/0080-store-msix-through-release-pipeline.md)、操作は[MSIX手順](../release/MSIX.md)。
