# native表の余り幅を等分する — 2026-10-29

Issue: [#405](https://github.com/hideyukiMORI/nene-folio/issues/405)。基点main
`5c1240edf06842681e5ae773ab202c8f23dadd6e`。
規則: ARC-001/004/011/012、C-002/011/014、QLT-008/012/013、GIT-001〜004。
[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)の「各列は等幅」を回復する不具合修正。
Waivers: none。

## 不具合と変更

`src/ui/win32/note_pane_table.c`の`layout_row`は、割算の余りを最後の列へ全部加えていた。
63列・1624twipでは25twip×62列＋74twipとなり、最後の列だけ幅が大きくなる。

同じnative配置処理の1行を変更し、先頭の余り個の列へ1twipずつ加える。
列数をn、幅をW、商をq、余りをrとすると、r列がq+1、残りがqになり、合計nq+r=W、最大差1を保つ。
余り0では全列qのままで、従来の割切れる入力と一致する。
native queryが列数1〜63を保証するため、除数は0にならず、列indexのLONG変換も範囲内に収まる。

列の構造・本文索引・描画経路は増やさない。container indent、水平移動、最小可読幅、
公開API・保存schema・依存・品質ゲートは変更しない。

## 実製品関数を使うnative回帰

成果物は`D:/NeNeFolio/agents/405-equal-table-columns/`、作業木は
`D:/NeNeFolio/worktrees/405-equal-table-columns`、build・obj・exeは
`D:/NeNeFolio/outputs/405-equal-table-columns/`へ置く。

`native-fixture.c`は専用の非入力desktopにMSFTEDITを作り、固定した4つの手書きRTFを読み込む。
実製品の`note_pane_table.c`をcanonical CMake targetのobjectとしてbuildし、fixtureへ直接リンクする。
計算式の写しや代替のlayout関数をテスト対象にしない。
TOM Undo停止/再開とmodified復元は、既存`note_pane`の呼出側と同じ手順で行う。

QLT-008/012の修正前redとして、まず63列・1624twipだけを実行した。
native readbackは25×62＋74、合計1624、最大差49で、幅差と明示した分布の期待が失敗した。
22 checks / 2 failures、native exeはexit 1だった。行indent920・列数63と本文状態は保持されていた。

完成候補の実native readbackは次のとおり。79 checks / 0 failures、native exeはexit 0。

| 入力 | native readback | 合計 | 最大差 | 保持した行indent / 列数 |
| --- | --- | --- | --- | --- |
| 63列・1624twip | 先頭49列が26、残り14列が25 | 1624 | 1 | 920 / 63 |
| 2列・1625twip | 813、812 | 1625 | 1 | 440 / 2 |
| 2列・1624twip | 812、812 | 1624 | 0 | 0 / 2 |
| 63列・1638twip | 全63列が26 | 1638 | 0 | 480 / 63 |

各入力でnative rowのgap80、raw UTF-16の全位置、選択範囲、dirty=-1、
非空のUndo=1・Redo=1を保持した。専用窓の破棄、thread desktop復帰、専用desktop解放も成功した。

## 実行経路と証跡

`run-native.ps1`は固定toolchainを読み、次の正典object targetをbuildする。
`eng/targets.cmake`のDebug警告集合・ASan/UBSan/nullabilityと、CMakeに結線したclang-tidyを適用する。

```powershell
cmake -S D:/NeNeFolio/worktrees/405-equal-table-columns -B D:/NeNeFolio/outputs/405-equal-table-columns/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build D:/NeNeFolio/outputs/405-equal-table-columns/build --target CMakeFiles/nenefolio_window.dir/src/ui/win32/note_pane_table.c.obj
pwsh -NoProfile -File D:/NeNeFolio/agents/405-equal-table-columns/run-native.ps1 -Phase red -PrimaryOnly
pwsh -NoProfile -File D:/NeNeFolio/agents/405-equal-table-columns/run-native.ps1 -Phase green
```

configure、red/greenの対象object build/tidy、fixture compile/linkはすべてexit 0。
wrapperのPowerShell終了値はred/greenとも0で、native exeの終了値は各`native-*-result.json`へ別に記録する。
redのexit 1を成功と扱わない。原本の`source-red.c`、`source-green.c`、入力RTF、全生値と初回失敗は保持する。

完成候補の製品source SHA-256は`7DFC97D97362F7797E4BAF70BFDE70E707CD8E446DE7EBB427887F99852E79BF`、
同じred/green fixtureは`23C760D3934973CCCA13AECDEC59E697D6048DCB95DA245307E8FF53943188B8`。
入力は`test-inputs.json`、環境は`environment.json`、静的確認は`run-static.ps1`と
`static-result.json`、全成果物の照合は`completion.json`と`hashes.json`へ集約する。
関係入力が変わらない担当交代・commit・push・mergeで、この対象成功を再実行しない。

## 残る範囲

Windows 10.0.26300.0、MSFTEDIT 10.0.26100.9949、120dpiの専用desktopによる実OS境界の回帰である。
通常画面・通常data・物理入力・Nibは操作していない。Debug実行にはASan runtimeの
`interception_win: unhandled instruction`警告があり、原本logに保持した。今回の成功を全環境の保証へ広げない。

狭幅の63列表で文字が読めない既知制限は未解消であり、今回の幅差のoracleとは分ける。
container所属、字下げと残幅、水平到達、検索、coreの成功済みテストは今回再実行していない。

これは対象実装・回帰の記録であり、最終PR headの必須CI `pwsh -NoProfile -File ./eng/check.ps1`、
main統合・必要反映の代わりではない。commit/push/PRと最終headのCIは親担当へ引き継ぐ。
統合反映後の整理も親担当が絶対パス・取込・未保存/未追跡/ignored・唯一成果物・稼働参照・リンクを監査して行う。
現時点では追加worktreeと出力を保持する。
