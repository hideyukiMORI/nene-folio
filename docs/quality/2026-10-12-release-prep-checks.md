# #282 — 版埋め込みと配布準備の確認

- 日付: 2026-10-12
- 基点: main `7ff9b96`、作業枝`build/282-release-prep`
- 要件: FR-014、[ADR0045](../adr/0045-single-version-and-verified-release-bundle.md)
- 証跡: `D:/NeNeFolio/agents/282-release-prep/`
- Waivers: none

## 対象と実行時点

CMakeの版からexe資源・設定面・配布名への接続、新規Release build、配布一覧/資産/ZIPの照合、
既存出力を壊さない公開経路を対象とする。完成候補の自己レビューと独立レビューをまとめた後に対象検証を行う。
Debugの製品回帰と全体の完了判定は、最終headの必須CIへ集約する（QLT-012・ADR0044）。
公開tag/GitHub Releaseは本作業の範囲外。

## レビュー

Astra設計席が差分と設計を確認し、gpt-6.1-sol / highの別席が配布/資源境界を読み取り専用で独立レビューした。
完成名へコピー途中の出力が残る構成、削除tracked入力のhash失敗、proof作業先のreparse確認より先のmkdirを実行前に修正した。
設定面の非選択末尾行は小さい窓で見えなくなるため、上部の空き領域への固定表示を採用した。
成功した対象検証をレビューの担当交代だけで再実行しない。

## 対象検証

入力はbase `7ff9b96`と未コミットの完成候補。`sourceDirty=true`を明示し、主要入力286ファイルのSHA256を
`source-inputs.json`へ記録した。生成前後の入力集合/hashが一致している。後続の文書変更・commit・統合はこの集合と照合して再利用する。

| 確認 | 実行コマンド/操作 | 結果・証跡 |
| --- | --- | --- |
| Release生成 | `pwsh -NoProfile -File ./eng/package-release.ps1 -WorkDirectory D:/NeNeFolio/outputs/282-release-prep/run-20261012-1642/work -OutputDirectory D:/NeNeFolio/outputs/282-release-prep/run-20261012-1642/package` | exit 0。固定toolchainで新規Release build、conformance 0違反、symbols 2 libraries / 0違反。`package-20261012-1642.log` |
| 検査器proof | 固定toolchain環境で`python eng/test-release.py --artifact D:/NeNeFolio/outputs/282-release-prep/run-20261012-1642/work/stage/NeNeFolio-0.1.0-win-x64/NeNeFolio.exe --metadata D:/NeNeFolio/outputs/282-release-prep/run-20261012-1642/work/build/product-version.json --workspace D:/NeNeFolio/outputs/282-release-prep/run-20261012-1642/proofs` | exit 0、16/16、0.408秒。追加buildなし。`release-proof-20261012-1642.log` |
| 初回起動 | ZIPを検査時に展開したexeをdataなしで起動 | 正常表示、フォント警告なし。PID 55632。終了は既存`post-keys.ps1`から`:q`、exit 0 |
| 設定の版 | `tools/post-keys.ps1 -TargetPid 55632`で設定を開き、`tools/capture-window.ps1 -TargetPid 55632`で撮影 | 日本語「版 0.1.0」、英語「Version 0.1.0」、简体中文「版本 0.1.0」を確認。`gui-results.json` |
| 最小窓/スクロール | Windows build 26300、120 DPI（125%）、論理560×360 / 実画素700×450 | 英語と简体中文で読め、最終選択行へスクロールしても版表示を保持。絵は`D:/NeNeFolio/check/2026-10-12/282/` |

exeはAMD64、FileVersion/固定ProductVersionは`0.1.0.0`、文字列ProductVersionと配布名は`0.1.0`。
manifestはID 1だけで、PerMonitorV2・従来DPI宣言・OS宣言・asInvoker/uiAccess=falseを保持した。
importはarchitecture.jsonから導いたOS DLL 11種だけだった。配布は10ファイル、ZIP内容と展開後の全hashが一致した。

- ZIP: `NeNeFolio-0.1.0-win-x64.zip`、22,716,911 bytes。
- ZIP SHA256: `2bdf6a0120c6576ee0211a8da96a0fbccc12f43addc999384a3e4763f7288106`。
- Release exe SHA256: `e27f3805240e85de47bb31bd8c766f1a58e8bb1660cba348430746cb7b95acc4`。

16件は実exeのidentity、x86 header、固定/文字列版、manifest版/UAC/DPI、不許可import、同梱欠損/余計なdata、
font破損→復帰→欠損、既存/相対/重複path、reparse属性、出力コピー失敗時の未公開・成功時の非上書き、ZIP内容/path、metadataの境界を扱う。
reparseのproofはlstat属性の注入であり、実junctionを作成する試験ではない。
最初のresize呼出しは呼出し側のDPI仮想化で875×563になったため、PerMonitorV2 contextを明示して700×450で再撮影した。
製品側の修正や再buildは無く、初回の絵を最小窓の証明には使っていない。

## 統合と反映

最終headの必須CI、main、通常exe反映、成果物の保持、追加作業木の整理は、紐付くPRと
`D:/NeNeFolio/agents/282-release-prep/completion.json`で確認する。未実施を成功としない。

## 確認の限界

実機表示は実行したOS・DPI・言語条件だけの確認であり、別OS/別DPI、物理IME、過去のhide目視項目まで受理しない。
SHA256は内容の照合に使うもので発行者の認証ではない。署名・インストーラ・自動更新は未実装。
製品の保存schemaとノートの編集/保存経路は変更しない。
