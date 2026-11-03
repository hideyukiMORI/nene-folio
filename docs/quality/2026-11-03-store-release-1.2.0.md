# Store更新候補1.2.0 — #536

2026-11-03。hideは#532の言語ラベルと#533のアイコンを受理し、Microsoft Store更新を求め、
Partner Centerのパッケージアップロード画面へ進んだ。
今回はStore提出原本と掲載用更新説明を作る。GitHub Releaseは更新しない。

規則: FR-014、ARC-001/002/007/011/012、C-018、QLT-008/011/012/013、GIT-001〜004、ADR0045/0080/0088。
追加waiver: none。既存WVR-0001は継続する。

## 変更と正典経路

唯一の版入力CMakeLists.txtのproject VERSIONを1.2.0へ上げる。
Windows版1.2.0.0、設定表示、manifest、配布名は既存の一経路から導出する。
README、MSIX README、[変更一覧](../release/1.2.0.md)、[3言語の更新説明](../release/1.2.0-store-notes.txt)、
現在状態と引き継ぎを更新する。生産C・保存schema・依存・品質gateは変更しない。

#520/#522/#524/#526/#530/#532/#533の機能検証とhideの受理は再利用する。
再実行の対象は版・配布metadata・実配布payloadの起動/設定表示であり、無関係なローカル全件回帰は行わない。

## 検証と確定原本

差分/参照とpython -B eng/conformance.pyを確認し、最終PR headで必須CIの
pwsh -NoProfile -File ./eng/check.ps1を成功させてから既存統合ツールでmainへ取り込む。
統合済みclean mainでeng/package-release.ps1 -Format Msixを実行し、固定toolchainのRelease build、
PE/版/manifest/import、アイコン全資源、書体/license、57候補PRI、MakeAppx pack/unpackと全payload/hashを検証する。
完成候補のpayloadをdataなしで短く起動し、設定のVersion 1.2.0を確認する。

入力snapshot・raw出力・終了値・成果物はtools/record-run.pyで記録する。
実行コマンド・最終head/CI/main・MSIXとSHA256・版表示・恒久収載・保全整理の確定原本は
`D:/NeNeFolio/agents/release-1.2.0-20261103/integration-completion.json`とする。
提出原本の予定先は`D:/NeNeFolio/releases/1.2.0/store-candidate-20261103/`。
この本文の予定だけで実行成功・アップロード・認定・公開を主張しない。

## 境界と残るリスク

hideが開いたアップロード画面へ渡すファイルを準備する。候補生成はStoreへの提出ではない。
Storeの現在の認定/公開状態は、hideの新しい報告か実画面を得た時点で別途記録する。
以前の1.1.0認定中という報告を、今回の1.2.0の状態に流用しない。
通常版が稼働中なら終了・上書きせず、Store提出用成果物の起動確認を分離する。
既存MSIXの導入・WACKと保存先に関する限界は[11月1日の記録](2026-11-01-msix-installation.md)を参照する。
