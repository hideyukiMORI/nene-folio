# 連続入力中の描画と候補照会 — 2026-10-25（#375）

Issue: [#375](https://github.com/hideyukiMORI/nene-folio/issues/375)。基点main `3ab9d44`。
hideの「カーソルキー/ホイールを連打すると1〜2秒後にまとめて反映される」という報告を受け、
単発のopen/paint計測では不足していた保持UIA項目と連続入力の関係を調べた。
調査・設計・実装・検証は単体で進め、候補所有/IME/同期描画と完成差分のみ独立した読み取り専用レビューを受けた。
判断は[ADR0065](../adr/0065-candidate-index-and-input-presentation.md)。
規則: ARC-001/004/005/008/011/012、C-002/005/008/012/013/016/017、QLT-008/012/013、GIT-001〜004。

## 原因と変更

通常の操作一覧の選択移動でもUIA nodeは保持される。各nodeのsnapshot更新で、参照の有効性確認から
子一覧・候補・群・command登録表を繰り返し先頭走査し、60回のDownでは群走査が約302万回に増えた。
選択移動自体は60回合計約12msだったが、UIA snapshotは合計3.02秒だった。
posted入力/通知が続く間、InvalidateRectだけで予約したWM_PAINTへ到達せず、最後にまとめて描画していた。

- `command_candidates`が群順、絞り込み結果、見出し/候補の子位置を保持する。`theme_candidates`も128候補の対応を保持する。
  登録表・文言・照合規則は従来のcoreを使い、主窓が派生資源だけを生成/破棄する。
- 描画・hit・キー・UIAの件数/名前/参照確認を同じ対応に集約した。行矩形は子位置の差から求め、旧走査経路を削除した。
  同じ言語/改訂ならEDITの取得・UTF-8確保も行わない。初期化、IME、失敗、世代飽和、閉鎖時の旧参照拒否を維持する。
- 保存状態を確定する既存の入力後合流点で、更新領域がある可視の主窓・索引・設定暗幕・補助面を描き、その後にUIA差分を通知する。
  同じWM_PAINTを使い、worker、timer、入力の間引き、別のbitmap正本は追加しない。
- 独立レビューで発見したpalette Enterの失敗理由混同を修正した。候補準備の失敗は既存OOM経路へ返し、
  成功後の直接参照で候補なしを区別する。Ex用queryの二重取得も除去した。

保存schema・実行時依存・ゲート閾値/除外は不変。Waivers: none。

## 連続入力の比較

恒久証跡: `D:/NeNeFolio/agents/375-ui-input-latency/`。
修正前の操作一覧は`D:/NeNeFolio/agents/help-input-backlog-20261025/`。
どちらも同じ基点のDebug・ASan/UBSan/nullability、3カテゴリ6合成ノート、日本語・monalisa・行番号ON、
960×640論理寸法、OS DPI 120。各条件60入力。外部のproducerが所有HWNDへPostMessageし、通常のmessage loopを通した。

| 条件 | 修正前の最大post→paint | 修正後の最大post→paint | 修正前→後の描画回数 |
| --- | ---: | ---: | ---: |
| 全操作をDownで横断、Sleep(33) | 2,921.10ms | 11.62ms | 11→60 |
| 全操作をwheelで横断、Sleep(8) | 1,203.62ms | 44.08ms | 6→55 |

修正後のUIA列挙35項目後も、key/wheelとも60回描画し、最大post→paintは8.73/7.43msだった。
修正前の通常Downの最大描画間隔2,939.00msに対して修正後65.69ms、wheelは1,195.78→43.00ms。
入力間隔はWindowsのSleepの粒度により実際には約46/16msであり、描画間隔とpost→paintを混同しない。
選択移動で`command_query`を呼び直さなくなり、候補の内容は不変なままUIAが参照できる。

これは各条件1系列の診断であり、統計的な性能保証ではない。時刻は計装したcopyのQPC、終点はGDI描画処理の完了で、
物理入力からDWM/display呈示までの時間ではない。hideの並行作業の負荷は制御していない。
UIA列挙は同一processのhostを通すfixtureであり、実readerや別processのUIA clientの測定ではない。

## 他部品の横断調査

別fixtureでは100カテゴリ×10ノート、先頭本文512合成行、履歴5版を使用した。
設定/テーマ/操作/キー説明/メニュー/履歴/検索/置換/索引/本文を、同じ入力後の合流と同期処理の両面から調べた。
下表の候補は各60入力、索引以外の一覧はUIA列挙後。修正前後でデータ条件を揃えた。

| 部品 | 修正前 最大post→paint | 修正後 最大post→paint | 修正後描画数 |
| --- | ---: | ---: | ---: |
| 設定Tab | 6.75ms | 7.39ms | 60 |
| テーマDown/preview | 60.71ms | 99.55ms | 58 |
| メニューDown | 9.94ms | 9.07ms | 60 |
| 履歴Down | 6.51ms | 5.62ms | 60 |
| 検索Enter | 10.10ms | 9.35ms | 60 |
| 置換語の入力 | 7.62ms | 7.10ms | 60 |
| 1,000ノートの索引wheel | 21.14ms | 6.97ms | 60 |
| キー説明wheel | 28.24ms | 29.15ms | 60 |

テーマ候補は同根の照会重複があったため修正した。`surface_read`合計560.54→53.60ms、
UIA snapshot合計750.84→229.17ms。通知回数も90→58に変わるため、関数一回の改善率にはしない。
一方、native本文の再着色に最大50.73ms、preview全体に69.67msの標本があり、最大応答は改善していない。
全体が一律に速くなったとは扱わない。本文量に比例するRTF再投入は今回の候補対応と別の境界である。

メニューは幅計測が残るが60入力に対して停止せず、ADR0062の必要条件なく別cacheを追加しない。
キー説明も折り返し再計測が残るが、本条件では秒単位停止はない。索引は既存のlayout参照/行位置cacheを確認し、1,000件で停止なし。
より大きい件数・巨大本文・外部reader接続時の一般性能は未保証。

本文の合成WM_MOUSEWHEELはこの隔離desktopのRichEditをスクロールさせず、WM_PAINTも0回だった。
これを「遅延なし」や「描画停止」と判定しない。PageDown/PageUpでは60入力/60描画を確認し、
最初の10入力で先頭表示行が12→129へ動き、逆方向で戻ることを記録した。
名前入力/失敗箱/テーマ取消は後述のnative契約を確認した。全ノート初回全文読込のUI停止は[#369](https://github.com/hideyukiMORI/nene-folio/issues/369)として別件のまま。

## 対象検証と再現

証跡directoryの各`*-inputs.json`/`*-build-inputs.json`はsource・library・exeのhashを保持する。
`backlog-*.csv`/`backlog-summary.json`、`baseline-*.csv`/`baseline-summary.json`、`post-*.csv`/`post-summary.json`が生記録と集計。
fixtureは外部copyにだけ計装し、製品に時計・試験口・抑制を追加しない。専用desktopを作成するがSwitchDesktop/実入力/Computer Useは使わない。

- `. ./eng/toolchain.ps1; cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug; cmake --build build --target nenefolio -j 4`:
  Debug build/clang-tidy成功。最初に見つかった複雑度超過2件は責務を小関数へ分けて解消し、ゲートを変更しなかった。
- `pwsh -NoProfile -File <証跡>/post-build.ps1 -Run -Case functional`:
  6,819 checks成功。3言語・通常幅/最小幅で、旧mainから抽出した独立oracleと候補順/件数/全先頭位置の行矩形を比較。
  hit/UIAとの一致、空候補、無効slot、query変更後の旧参照拒否、初期WM_SETTEXT、合成IME、invalid UTF-16、世代飽和、focus、preview取消、不要paintなしを含む。
- `pwsh -NoProfile -File <証跡>/geometry-build.ps1 -Run -Case <functional96|functional192>`:
  各6,819 checks成功（計3比率20,457）。主窓実装のGetDpiForWindowをfixture内だけで96/192に置換し、同じ候補/幾何契約を照合した。
  OS/native部品の実DPIは120のままであり、物理モニター移動・実IMEの試験ではない。
- `pwsh -NoProfile -File <証跡>/backlog-build.ps1 -Run -Case allrows`と`post-build.ps1 -Run -Case <部品名>`:
  上記の連続入力が成功。本文の追加確認は`body-key-build.ps1 -Run -Case body`。
- `pwsh -NoProfile -File <証跡>/fault-build.ps1 -Run -Case fault`:
  11 checks成功。候補資源の第1/第2確保失敗と部分破棄、EnterのOOM/空/成功、保持UIA rootありで同期paint中のcloseを検証。
  失敗表示の呼出しをfixtureで観測し、誤ってunknownへ落ちないことを確認した。
- `pwsh -NoProfile -File <証跡>/native/run-native.ps1 -Mode <name-flow|failure|theme-cancel> -SkipBuild -RunLabel reviewed`:
  30/22/7 checks成功。実native dialogの取消・主窓close・dispatch復帰・元focusとpreviewの保持を確認。
- `python eng/conformance.py`: 0 violations。`git diff --check`: 成功。

Enter修正後のbuild/失敗注入で差分を検証した。候補対応・幾何・連続移動・描画合流はその修正で変わらず、成功済み結果を再利用する（QLT-008/012）。
fixture準備中の型/引数/生成パスの誤りは修正して再実行した。製品不具合と混同しない。
実行時に既知の`interception_win: unhandled instruction`起動警告があるが、各最終fixtureはexit 0でASan/UBSan/nullabilityの違反報告はない。

最終PR headの必須CIで`eng/check.ps1`を要求する。統合/通常exe/整理は`completion.json`と実際のPRを照合する。
物理IME、実reader、物理DPI跨ぎ、実display呈示は未確認。保存schema・依存・ゲート変更なし。Waivers: none。
