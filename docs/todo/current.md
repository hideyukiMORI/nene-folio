# いまのタスク — NeNe Folio

記録日: 2026-10-17。#311開始mainは1e799d1e16eb09aa82c6458161c20e9868fe5d0d、公開tag v0.1.0。
hideの現在の継続依頼で#310を完了し、続けて#311のサイズ計算を扱った。
今回のAstra設計判断・SOL/LUNA実装/調査/検証分担は、この継続依頼の範囲に限る。次回へ自動継承しない。

## 今回の焦点と最終照合

- [#311](https://github.com/hideyukiMORI/nene-folio/issues/311)の4つのcore経路で、サイズを加算/乗算する前に検査する。
  既存OOM/TOO_LARGE、本文保持、置換3scopeを維持。保存16MiB・置換4000000単位の上限は変更しない。
- 対象build/tidy、通常/測定版の--size-overflow、format/conformance/diffが成功。
  実製品の自然入力でのoverflow再現は確認していない。core宣言長のredと受理根拠は[品質記録](../quality/2026-10-17-size-overflow-checks.md)。
- 最終head必須CI・統合・通常exe反映・専用worktree/outputs整理は、#311の関連PRと
  D:/NeNeFolio/agents/311-size-overflow/completion.jsonで照合する。
  MERGED/CLOSEDと反映・整理成功が揃えば#311は完了として再開しない。未了なら残る工程を継続する。

## 完了済みと次の入口

- #310は[PR #317](https://github.com/hideyukiMORI/nene-folio/pull/317)で統合、CI成功、通常exe反映、専用作業木整理まで完了。
  D:/NeNeFolio/agents/310-stream-failure/completion.jsonを参照。#309/PR #314、#315/PR #316、#306/#308も完了済み。
- 経緯は[日報](../reports/2026-10-17.md)、再開条件は[引き継ぎ](../handoffs/2026-10-17.md)。
- 次の設計候補は[a11y/OS contrast #312](https://github.com/hideyukiMORI/nene-folio/issues/312)。
  性能は[既存実測](../quality/2026-10-15-external-review-performance.md)済み・最適化未実装で、別Issueの焦点を定める。
- portable/installer、UI分割、定期draft退避、起動temp回収は別の範囲設計へ。
  公開v0.1.0差替え、#243、旧作業木整理も別件。

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe。最終反映・hashはcompletionで確認し、更新だけで起動/実機受理を主張しない。
#311の検証はx64のcore算術と直接呼出境界。巨大実確保・GUI/OS終了の再測定はしていない。入力不変の成功結果は再利用する（QLT-012）。Waivers: none。
