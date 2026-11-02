# WVR-0001: WinHTTP callbackの固定ABI

- Status: active
- Rule: C-012
- Issue: #520
- Owner: NeNe Folioサナ
- Created: 2026-11-02
- Expires: 2026-12-02
- Scope: `src/adapters/win32/image_download.c#status_callback`

## 正確な範囲

`WINHTTP_STATUS_CALLBACK`の固定5引数に対する`readability-function-size`だけを単関数で抑制する。
同checkerの行数/ネスト診断も抑制されるため、callbackを60行/ネスト3以内で通知の複写とevent通知だけに限定する。
認知的複雑度10は別checkerで引き続き強制する。閾値・除外設定は変更しない。

## 理由

非同期WinHTTPの完了と最終handle closingを安全に受け取るには、OS指定の5引数ABIが必要。
4引数関数へのcastはABI違反になる。同期受信は少量ずつ届くheadersの累積期限/取消を保証しない。
ADR0085の通信資源所有に限定した例外である。

## リスクと封じ込め

callbackはUI/application/coreに触れず、requestの操作はworkerだけが行う。
最終closingまでcontext・buffer・eventを保持し、通信成功/失敗/取消/期限をnative検証する。

## 解除条件

2026-12-02までに、OS指定ABIの引数だけを正確に検査対象外にできるlint設定/検査方法を評価し、
同等の行数/ネスト検査を保って置換するか、当該callbackを廃止する。追跡: #520。

## 却下した代替案

別threadから同期requestをcloseする方法はMicrosoftが競合を警告している。
UIでの同期取得や、任意の通信の背景化は採らない。

## 参照

- Issue: #520
- ADR: [0085](../adr/0085-lazy-markdown-images.md)
- [OS callback ABI](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nc-winhttp-winhttp_status_callback)
