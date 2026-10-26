# #398 — v0.3.0 beta公開の確認

日付: 2026-10-27。hideの実機サンプル受理とベータ版更新依頼に基づき、
[v0.3.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.3.0)を01:14:32 JSTに公開した。
`isDraft=false`、`isPrerelease=true`。Issue: [#398](https://github.com/hideyukiMORI/nene-folio/issues/398)。
規則: FR-014 / ARC-001・002・007・011 / QLT-008・011・012・013 / GIT-001〜004 / ADR0045。Waivers: none。

## 公開物と由来

[PR #399](https://github.com/hideyukiMORI/nene-folio/pull/399)の最終head
`f50a25b24b2438c107ae96b18c654ea628d5febf`で
[必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/40354478601)が成功した。
統合後のclean main `a0df9932620e3575d692c71134465eb704ec5e01`から正規配布器で生成した。
版は唯一のCMake入力から0.3.0、Windows資源/manifestは0.3.0.0へ導出した。

| 添付物 | bytes | SHA256 |
| --- | ---: | --- |
| NeNeFolio-0.3.0-win-x64.zip | 22,839,058 | `0641f956f4e5fe0931107a895e006b6b52417104edc43ea2c7aafb98439034ce` |
| SHA256SUMS | 95 | `9927fc544da73cc648f3f9ac4a70263e9ff2fa12ea79699b532250bb49819215` |

Release exe SHA256: `2018b6dba3436ec99e55664acb39b9897a996bd592bdbf08badd6567950777fe`。
`release-evidence.json`にsourceDirty=false、AMD64、宣言されたOS DLL 14種、版/manifest、
同梱11ファイルと書体hash/licenseの検査成功を記録した。ZIPに通常dataを含めていない。
公開後のZIPとSHA256SUMSは生成物とbytes/hashが一致し、再取得したZIPの11 membersも一致した
（`published-release.json`、`download-verification.json`）。

## 配布exeの起動・保存・旧版からの更新

専用展開先で実配布exeを用い、`legacy`（v0.2.0）、`fresh`、`upgrade`の全processがexit0だった。
日本語を含む合成ノートは36 bytes、SHA256
`32635988552dfebe122ff9114a26daa390ab7db6c6adaaf15c954185b88b387d`で全条件一致した。
旧v0.2.0が生成したdata 5ファイルをコピーした更新では、ファイル集合と全hashを保持し、
新版から再読込したnative本文も一致した（`smoke-verification.json`）。

`fresh-settings-version.png`で同じRelease exeの設定版0.3.0を目視確認し、
更新後本文は`upgrade-reopened-redraw.png`で受理した。
`upgrade-settings-version.png`には所有modalが映っておらず、設定版の表示証拠として採用していない。
同一exeのhashとfresh側の成功証拠を再利用し、「すべての画像で成功」とは扱わない
（`smoke-visual-review.json`）。専用desktopの確認は物理IME・実reader聴取・DWM実画面呈示の保証とは区別する。

## 機能の再利用と通常exe

v0.3.0は#392/#394/#396の成果を配布へ含めた。[ホイール](2026-10-26-view-wheel.md)、
[Markdown表示](2026-10-26-markdown-style.md)、[構文](2026-10-26-markdown-syntax.md)の品質記録と各taskの完了証跡を根拠とし、
入力不変な機能試験を繰り返していない。hideは通常`data/サンプル/Markdown構文サンプル.md`の合成サンプルを
実機で表示して受理した。通常サンプルと専用smokeの合成dataは配布物へ混ぜていない。

通常の`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `063f5bab`のMarkdown機能追加済みビルドで、
表示版は0.2.0だった。開始時PID56180へ操作・終了・上書きをせず、01:15:53 JSTに通常process0件を確認してから正規ツールで反映した。
`tools/prepare-real-machine.ps1`はexit0、01:16:51 JSTにsource `a0df993`・表示版0.3.0/Windows版0.3.0.0へ更新した。
通常exeは5,367,808 bytes、SHA256 `024AB87C7F0C2A79E08A0C8EEC49FCD2E6DB0A3A3A8369ADCC54CAE8EF4A1270`。
更新後は起動せず、通常dataを操作していない。受理済みサンプルは残存し、hashも不変だった（`normal-build-verification.json`）。

## 範囲と限界

runtime版と利用者文書を更新したが、生産C・保存schema・設定・依存・gate・packaging仕様は変更していない。
[リリースノート](../release/0.3.0.md)に更新手順と制限を記載した。
list内quoteから親list継続へ戻る組合せは未保証、表は63列まで、リンクは装飾のみ。
HTML/画像埋込み/完全CommonMark互換、保存先設定#390、署名・インストーラ・Store・自動更新は対象外である。

## 保管・整理と文書確定の追跡

恒久証跡: `D:/NeNeFolio/agents/beta-release-20261027/`。
上記のPR/CI/Release/配布・smoke・再取得は各確定JSONへ照合した。
`frozen-fixtures-and-builds.zip`へ200ファイルを保管し、全entryのhashが一致した。
archiveは206,617,101 bytes、SHA256 `cb35fd391d4f03fc944cf541b27d95c55aa29930ff4cd16b667a59d916a1078a`。
01:18:08 JSTに安全監査後の`cleanup-task.ps1`がexit0で完了し、#398のworktreeと
`398-beta-work`/`398-beta-package`/`398-beta-smoke`/`398-beta-download`の4出力を整理した。
branch/commitと公開物・恒久証跡を保持し、同所の`completion.json`へ`release-complete`を記録した。
旧削除拒否済み14箇所は今回も再試行しない。

公開後の文書確定は[#400](https://github.com/hideyukiMORI/nene-folio/issues/400)。
本稿の更新では文書参照・数値・時点とdiffcheckだけを確認し、製品build/test・配布生成・smokeを再実行していない。
#400自身の最終head CI・main統合・追加worktree整理の確定値は、実Issue/PRと`docs-completion.json`を照合する。
