# #226 — IME変換中のmouse置換

Issue: [#226](https://github.com/hideyukiMORI/nene-folio/issues/226)。規則: ADR0016 / ADR0028 決定6、ARC-001 / ARC-011、C-012、QLT-013。Waivers: none。

基点main `81e9901`。製品差分は `apply_replace` 入口の4行だけで、`command_composing` 中は無操作で返す。
click_replace_buttonsのクリック消費を維持し、別欄へfocusを移さない。通知・描画・強制確定・timer・実行予約を足していない。
core / application / ports / API / 保存スキーマ / 依存 / ゲートは変更していない。

## 修正前の制御message再現

成果は `D:/NeNeFolio/agents/ime-replace-repro/` のsource snapshot、probe source/script、4log、JSON/hash/reportに保持する。
両入力欄 × mouse1件/すべての4caseで124 PASS / 0 FAIL（欠陥の挙動を期待）。変換中に旧a→X下見が本文へ適用された。
replacement欄ではpreview3件が2/0件へ変わり、状態行も更新された。実Undo1回で不意の置換を戻してからEND後の新入力による適用も確認した。
同じsourceの成功済み再現は反復せず、snapshotを同SHAで保存して削除済みD220 worktreeへの直接参照を解消した。

## 修正後の隔離 Win32 / RichEdit probe

成果は `D:/NeNeFolio/agents/226-ime-replace/` の `ui_probe.c`、`build-probe.ps1`、`probe-*.log`、`probe-results.json` に保持する。
製品folio_window.cをそのまま取り込み、製品application/実regex adapterとfake persistence portsを結合する。user app/dataへ操作していない。
実EDIT HWNDへSTART→EM_REPLACESELによるEN_CHANGE、実描画layer HWNDへWM_LBUTTONDOWN、実EDIT HWNDへENDを送る。
内部composition flagは直接書かない。mouseを受けてもflagはtrueのままであり、強制確定していないことを観測した。

| 入力欄 / button | PASS / FAIL | 変換中 |
| --- | --- | --- |
| pattern / 1件 | 32 / 0 | 本文・preview3件・状態行・focus不変 |
| pattern / すべて | 32 / 0 | 同上 |
| replacement / 1件 | 32 / 0 | 同上 |
| replacement / すべて | 32 / 0 | 同上 |
| pattern / すべて・非0 anchor/scroll | 33 / 0 | 選択1〜2、正のscrollも保持 |

4case128 PASSに、新しい非0選択/scroll境界1case33 PASSを加えて計161 PASS / 0 FAIL。
全caseで本文・選択・scroll・本文Undo、2欄の文字/選択/native Undo、preview有無/件数/outcome/状態行、focus、全persistence portの呼出し回数を保持した。
END直後も本文不変で、無視したクリックを予約実行していない。ENDで新previewを取得した後、改めて同じbuttonを1回押すと新pattern/replacementが適用され、実Undo1回で戻る。
write/archive/create/renameは0。非0境界以外の成功済み4caseは再実行していない。

## 実行コマンド

- `cmake -S D:/NeNeFolio/worktrees/226-ime-replace -B D:/NeNeFolio/agents/226-ime-replace/product-build -G Ninja -DCMAKE_BUILD_TYPE=Debug` → exit0。
- `cmake --build D:/NeNeFolio/agents/226-ime-replace/product-build --target nenefolio_window nenefolio_adapters` → exit0、対象compile/clang-tidy成功。
- `pwsh -NoProfile -File D:/NeNeFolio/agents/226-ime-replace/build-probe.ps1` → exit0。ASan/UBSanを付けてprobeをcompile/link。
- `ui_probe.exe pattern-one-fixed|pattern-all-fixed|replacement-one-fixed|replacement-all-fixed` → 個別実行で各exit0。
- `ui_probe.exe pattern-all-fixed-offset` → exit0（追加の非0選択/scroll境界のみ）。
- `git diff --check` → exit0。

## 最終ゲート

source checkpointの親静読受理後に正典 `pwsh -NoProfile -File ./eng/check.ps1` を1回実行する。現時点では未実施。
結果・ログ・固有4JSONの退避SHA照合はここへ追記する。

## 未測定と残る範囲

Microsoft IMEが通常クリックより先にENDを届けるかは未測定。制御messageの回帰成功を物理IME再現・目視成功に読み替えない。
検索button / Ex / キーボード別経路 / renameの成功済み試験は今回の変更へ広げていない。
