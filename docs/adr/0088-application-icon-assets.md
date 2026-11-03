# ADR 0088 — アプリアイコンを一つのSVGからWindows資産へ派生させる

- 状態: 受理
- 日付: 2026-11-03
- Issue: #533
- 規則: ARC-001/002/004/009/012、C-008/016/017、QLT-007/008/010/011/012/013、GIT-001〜004
- 関連: ADR0045/0080

## 根拠

hideはコード言語ラベルのデザイン検討後、Claudeにアプリアイコン制作を依頼し、角丸の外に見える背景とClock/Loupeとの不統一を直すよう指定した。
Claudeのdesign:design-critique/design-handoffと[canvas](https://claude.ai/artifact/BcA7CRegGBHzcQrZco58Lc)で3案を比較し、アンバー地・クリームのノート・チャコールの背表紙/罫線/分類タブのB案を採用する。
現状はMSIX用のPNG3枚だけで、native ICOとtargetsize/unplated画像・PRI索引がない。

## 決定

1. 正本はpackaging/windows/app-icon.svg一つ。図形・色を別の手描き実装へ複製しない。
   外側余白をなくし、角の外はalpha 0、中央は不透明とする。16〜256pxでもノートを識別できる図案を使う。
2. 既存packaging/windows/render-icons.ps1をSVG派生の唯一の入口へ置き換える。
   eng/windows-icons.jsonが原本/ICOのpath、resource名、ICOサイズ、MSIXのbase/scale/targetsize、生成用Chromiumの版/hashを所有する。
   指定した未作成のD側出力へ、固定rendererで各寸法のSVGを一枚のsheetに描画し、Windows GDI+で等寸切出しする。
   PNG・ICO・更新用eng/msix.json候補と生成証跡を出す。通常build/CIはrendererを起動せず、追跡した資産とhashを検証する。
   新たな製品実行時依存やPython package依存は増やさない。
3. ICOは16/20/24/28/32/36/40/48/56/64/72/80/96/112/128/256pxの32bit alpha。256はPNG、他はDIBと透明AND mask。
   native exeへ同じICOを一つの名前付きgroup resourceとして埋め込み、実主窓が大小のHICONを所有する。
   LoadImageWでサイズごとの専有handleを取得し、両方成功してからWM_SETICONで渡す。
   Windows標準の100/125/150/175/200/225/250/300/350/400%で大小とも実寸frameを供給する。
   既存manifestのCommon Controls版を変更せず、画像の拡縮によるalpha丸めと表示のぼけを避ける。
   任意のcustom DPIではOSによる補間があり得る。元画像の不透明部分はalpha255、角は部分/完全透過を保つ。
   DPI変更では両方取得できたときだけ置換し、旧handleを解放する。破棄は窓を壊した後にDestroyIconを行う。
   LR_SHAREDのresource名cacheで大小が同じhandleになる経路は使わない。class/globalの可変所有状態は追加しない。
4. MSIXは既存3baseのscale100/125/150/200/400と、Square44のtarget16/20/24/30/32/36/40/48/60/64/72/80/96/256を用意する。
   targetごとに既定/unplated/lightunplatedを同じ原本から供給する。既存の無修飾PNG3枚は置き換える。
   eng/msix.jsonが派生ファイル集合・寸法・hashと原本/ICOのhashを保持し、集合はwindows-icons.jsonから導出して照合する。
5. ADR0080決定3の「qualifier/PRIを使わない」を更新する。固定Windows SDKのMakePRIを既存MSIX生成にだけ追加する。
   resource packageへ分割せず一つのresources.priを生成する。dumpで資産pathとqualifierを検査し、生成bytesをstage/pack/unpackで照合する。
   未知の追加ファイルを許す除外は設けない。MakeAppxの検証とblock map照合、入力snapshot、未作成path、最終公開renameは維持する。
6. 製品版1.1.0、保存schema、Store identity、機能、runtime DLL依存を変えない。Store申請・署名・公開・既存インストール変更は行わない。

## 受理と限界

SVG→PNG/ICO生成、全サイズ/alpha/hash、exeのGROUP_ICONとICON bytes、実窓の大小handle/サイズとDPI置換、
MSIX資産集合/PRI候補/pack/unpackを対象確認する。検査器の欠落・改変・寸法・不透明角・resource差異・PRI差異等の反例を既存release proofへ結ぶ。
最終headの必須CI後にmainへ統合し、#532の言語ラベルと一緒に通常版へ反映・data照合・保全・整理する。
全shell cache状態/OSテーマ/DPIの保証はしない。Store公開版の画像は次の配布更新まで変わらない。
追加waiverはnone。既存WVR-0001は変更しない。

## 一次資料

- [Windowsアイコンの寸法とunplated画像](https://learn.microsoft.com/ja-jp/windows/apps/design/iconography/app-icon-construction)
- [アイコンresourceと窓](https://learn.microsoft.com/en-us/windows/win32/menurc/about-icons)
- [LoadImageWのcacheと所有](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadimagew)
- [LoadIconWithScaleDownのサイズ選択](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-loadiconwithscaledown)
- [MakePRIコマンド](https://learn.microsoft.com/en-us/windows/uwp/app-resources/makepri-exe-command-options)
