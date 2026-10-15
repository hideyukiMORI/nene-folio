# #309 — OS終了への保存同意と取消の確認

2026-10-15、外部講評P0-1を[ADR0053](../adr/0053-session-end-and-exit-contract.md)で設計し、
修正前redと候補の対象検証を行った。実Win32/Windows 11/PMv2/120 DPI、専用desktop、合成fixtureを使用。
通常のdataや利用中アプリ、実ログオフは操作していない。hideの今回指定によりAstraが設計・受理、
SOLが実装・native検証・必要な独立静読を担当した。Waivers: none。

## 入力と修正前の再現

恒久証跡は`D:/NeNeFolio/agents/309-session-end/`、build/runtimeは同outputs配下。
基点main 55f08641で名前付き本文を変更し、WM_QUERYENDSESSION(LOGOFF)→WM_ENDSESSION(TRUE)を注入。
QUERYはTRUEを返すが保存先は両通知後とも変更前のまま。期待した保護を満たさずexit 1。
`red-results.json`、入力hash、source、fixture前後を保持する。

## 対象検証のコマンドと結果

作業木`D:/NeNeFolio/worktrees/309-session-end`で`eng/toolchain.ps1`の固定環境を使用。

- `cmake -S . -B D:/NeNeFolio/outputs/309-session-end/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`成功。
- `cmake --build D:/NeNeFolio/outputs/309-session-end/build --target nenefolio folio_tests`成功。
  初回はtranslateの認知複雑度11で停止。同じ入力窓所有の判定を関数へまとめ、上限10を維持して修正。
  notice更新から失敗モーダルへ到達しない共通配置の整理、短い英語理由への変更後も対象build成功。
  `build.log`、`build-fix.log`、`build-notice-fix.log`、`build-short-notice.log`を区別して保持。
- `folio_tests.exe --commands-ui`成功。文言変更後の`unit-short-notice.log`が最終結果。
- 変更C/headerの`clang-format --dry-run --Werror`、`python eng/conformance.py --build-dir D:/NeNeFolio/outputs/309-session-end/build`、
  `git diff --check`が成功（conformance 0違反）。`format-root-final.log`、`conformance-root-final.log`、`diff-root-final.log`へ収載。
  最終head必須CIは別に実PRで確定する。
- 外部`prepare-green.ps1`、`compile-green.ps1`で実製品source/libsを使い、`run-green.ps1`で専用desktopへ起動。
  初回は機能168 assertionが成功した後、試験側の子process kill待ちがtimeout。worker全体をexit 0とは書かない。
  probe親の待機をmessage pumpへ修正し、`run-green.ps1 -Case kill -RunTag green-kill`だけ再実行して4/4成功、exit 0。
  両runの製品入力同一をhashで確認し、`aggregate-green.py`で172件、製品failureなしと集計。

## 確認した振る舞い

- 名前付きQUERYが保存してからTRUE。承認後の入力を止め、再QUERYは追加保存せず、END(TRUE)は共通最終破棄。
- END(FALSE)は本文・選択・Undo・dirty・表示論理行・検索surface/input/Undo/focusを保持。成功した保存を巻き戻さない。
- 無題、root/pane/filter/commandの合成IME、既に無効な親、削除所有中、改名保留は保存/prompt前に拒否。
- 保存中の再QUERY・直接command・WM_COMMAND/文字を拒否。END(TRUE)で破棄せず、END(FALSE)後も外側が戻るまで再保存を拒否。
- archive/write失敗は元の保存済み本文とdraftを保つ。公開後の検索写し失敗では成功した保存を保持し終了拒否。
  次回QUERYで修復してもarchive/writeを重複しない。
- ShutdownBlockReasonCreate/Destroy失敗時も拒否と所有を正しく保ち、解除を再試行する。
  理由解除中の取消で承認しない。設定面の理由notice更新が暗幕再確保・failure boxを呼ばない。
- flags 0/LOGOFF/CLOSEAPP/CRITICAL/複合は同じ保存契約。通常WM_CLOSE、`:q!`、明示保存終了の意味を維持。
- 所有するfixture子processだけを強制終了し、未保存draftがあっても直前の保存済みrawは正確に残る。
  未保存の自動復旧は確認対象でも実装済み機能でもない。

`green-summary.json`、`green-verification-report.json`、`green-functional-results.json`、
`green-kill-results.json`、`green-initial/`、fixture ZIPを保持。
初回の外部probe識別子衝突・compile警告83件とASan interception_win起動診断も残し、
警告なし・完全なinterceptor coverageを主張しない。

## 表示と受理の境界

最小BOTH560×360 DIPの3言語画像を保存して目視。初回英語理由は400/335pxで末尾省略、和文245px/中文191pxは収容。
英語5文言だけ短くし、機能172件は関係入力不変のため再利用。短文専用probeは19/19成功、exit 0。
5理由×3言語は全て335px内。和文247/273/245/251/292、英文256/256/216/232/256、中文205/205/191/191/172px。
native担当と設計席が新3画像を目視し、全文収容と重なりなしを確認した。
`short-notice-summary.json`、`short-notice-inputs.json`、`short-notice-*-minimum.png`を保持し旧画像を置換しない。

任意の実ログオフ/Update、物理IME、電断耐性、別OS/DPI、全テーマ、OS待機時間内の保存完了、hide実機受理は未測定。
ごみ箱実行中・OOM等は制御した境界注入であり、実資源枯渇とは呼ばない。
保存schema・依存・ゲート変更なし。規則: FR-006/015/018/019、ARC-001/004/010、C-002/012/017/018、QLT-008/012/013。
