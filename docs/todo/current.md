# いまのタスク — NeNe Folio

記録日: 2026-10-17。開始main `9b6c34d318817ecb83460bca202c607c71ed8ebd`、公開tag `v0.1.0`。
hideの現在の継続依頼で#310を再開。今回の分担はAstra設計判断・SOL/LUNA実装/調査/検証であり、次回へ自動継承しない。

## 今回の焦点と最終照合

- [#310](https://github.com/hideyukiMORI/nene-folio/issues/310)の本文投入保護を実装・対象受理した。
  候補`a1b6ba3`で最新main/P0を取込済み。native130件成功、最小幅3言語画像確認、独立レビュー指摘解決。
  契約は[ADR0054](../adr/0054-checked-rich-edit-load.md)、実測は[品質記録](../quality/2026-10-17-pane-load-checks.md)。
- 最終head必須CI・統合・通常exe反映・専用worktree/outputs整理は、#310の関連PRと
  `D:/NeNeFolio/agents/310-stream-failure/completion.json`で照合する。
  MERGED/CLOSEDと反映・整理成功が揃えば#310は完了として再開しない。未了なら残る工程を継続する。
- #309/PR #314、#315/PR #316は統合・必要反映・専用作業木整理済み。#306/#308も完了済み。
  今回の経緯は[日報](../reports/2026-10-17.md)、場所・再開条件は[引き継ぎ](../handoffs/2026-10-17.md)。

## 次の判断候補

- サイズ計算 [#311](https://github.com/hideyukiMORI/nene-folio/issues/311)は未着手。
- 性能は[1万本/数MiBの実測](../quality/2026-10-15-external-review-performance.md)済み、最適化未実装。
- a11y/OS contrast [#312](https://github.com/hideyukiMORI/nene-folio/issues/312)、portable/installer、UI分割、
  定期draft退避、起動temp回収は別の範囲設計へ。[判断表](../reports/2026-10-15.md)を参照。
  公開v0.1.0差替え、#243、旧作業木整理は別件。

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。最終反映・hashはcompletionで確認し、更新だけで起動/実機受理を主張しない。
実ログオフ/物理IME/電断/実資源枯渇/別OS/DPIは未測定。成功済検証は関係入力不変なら再利用（QLT-012）。Waivers: none。
