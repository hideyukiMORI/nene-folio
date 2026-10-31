# ADR 0080 — Store MSIXを既存のRelease生成経路で作る

- 状態: 受理（2026-10-31、実装前。hideのStore準備継続とidentity提供による）
- 日付: 2026-10-31
- Issue: #484
- 規則: FR-014、ARC-001/002/011/012、QLT-007/008/010/011/012/013、GIT-001〜004
- 関連: [ADR0045](0045-single-version-and-verified-release-bundle.md)、[ADR0077](0077-note-storage-location.md)

## 決定

1. 配布入口は引き続き `eng/package-release.ps1` 一つ。既定ZIPに `-Format Msix` を加える。
   固定toolchainのRelease build、版・PE・manifest・import・書体・license検証とpayload一覧を共有し、
   形式ごとに製品を作り直す第2のスクリプトを置かない。
2. hideが提供したStore Name/Publisher/PublisherDisplayNameはMSIX設定一箇所に置く。
   秘密鍵・証明書・不要なPackage SIDは保存しない。製品版はhide指定の1.0.0へCMakeで変更し、
   exe表示/Windows版/MSIXへ同じ値を供給する。Store候補は先頭1以上、4成分目0を要求する。
3. MSIXはx64・Windows.Desktop（最小10.0.19041.0）・packagedClassicApp/mediumIL。
   runFullTrustだけを宣言し、管理者昇格・AppContainer・独自自動更新・file associationを追加しない。
   3言語の製品UIに対応するja-JP/en-US/zh-CNを列挙する。設定の説明文とPNGロゴは直接指定し、
   名前付き/qualifier付きresourceを使わないためPRI生成を導入しない。
4. MSIXのpayloadは既存ZIPと同じ検証済みexe/書体/license。MSIX用の説明READMEだけは配布形式に合う内容を選ぶ。
   package manifestと必要なPNGロゴを追加する。ロゴはhideが作成を依頼した紙/ノートの候補で、
   ソース図案とサイズ/hashを追跡し、Storeページ素材の受理と区別する。
5. Windows SDK MakeAppxを固定SDK版から解決して記録する。packの検証を無効化せず、SHA256で作る。
   ZIPとしての全エントリ名/集合/hash、manifest、SDKのblock mapを確認し、MakeAppx unpack後もpayloadを照合する。
   SDK管理のfootprintと製品payloadを区別する。未知の余分なファイルを認める一般除外は置かない。
6. 既存の未作成絶対path・重複/包含/reparse拒否・入力snapshot・部分出力保全・最終renameを使う。
   成功時は未署名MSIXとSHA256SUMSを出力する。生成中に入力が変わったら公開しない。
7. パッケージ生成はStore申請・公開・証明書の信頼登録を実行しない。
   Store署名とローカル試験署名を分ける。実インストール、初回保存先、更新、アンインストール、WACKは
   対象環境の別段階として記録し、MakeAppx成功だけで成功を主張しない。
8. 検査器の正例/反例を既存 `eng/test-release.py` と単一ゲートへ加える。
   実MSIX生成は完成候補で一回にまとめ、変更がないC機能のローカル回帰を追加しない。
   最終headの必須CI、統合、成果物の保全と作業先整理は既存手順を維持する。

## 結果と未確認事項

portable ZIPを維持したまま、Store identityを持つ候補を同じ版・payloadから作れる。
保存schemaは変更しない。実MSIXの保存先挙動、とくにAppDataに利用者が任意指定したrootの仮想化と
アンインストール時の保持は実インストール段階で確認が必要。未検証の保持を保証しない。
Waivers: none。

## 一次資料

- [パッケージ要件と版](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements)
- [手動MSIX構成](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion)
- [MakeAppx](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool)
- [packaged desktopの動作と仮想化](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes)
