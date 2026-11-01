# 文書投入後のミニマップ待機短縮 — #510

hideの「対応して」（2026-11-01）に基づき、直前の調査で見つけた120ms待機と読込中の予約重複を修正した。
同じnative sample・idle経路を使う。アタリや段階表示は追加しない。

## 正典経路と変更

- `folio_window.c`の予約を`minimap_update`のCLEAR / SETTLE / READYへ分けた。
  文書投入成功時はREADYで構築を開始可能にし、取得は既存idleに任せる。
  編集・通常resize・倍率・表示復帰はSETTLEで120msの入力集約を保つ。
- 投入前に旧timerと像を破棄し、STREAMING中のEN_CHANGEではcache/本文の印と像の失効だけ行う。
  timer再予約と描画通知を読込完了へ集約する。失敗・未準備・busyでは像とpendingを残さない。
- 行番号幅の変化は共通`pane_inset_changed`で読み、投入後の配置を構築開始前に整える。
  遅れたpaintで120ms待ちへ戻らない。最大256sample・16/idleと完成像だけの公開は不変。
- `tests/win32/minimap_scheduling_tests.c`を実Win32のCTestへ追加した。製品に試験用窓口は設けない。

規則: ARC-001/002/004/007/008/011/012、C-002/008/012/013/016/017、QLT-008/012/013、
GIT-001〜004、[ADR0084](../adr/0084-bounded-native-minimap.md)・[ADR0068](../adr/0068-idle-index-preparation.md)。
保存schema・版・依存・申請済みMSIXは不変。Waivers: none。

## 検証と原本

原本は`D:/NeNeFolio/agents/minimap-ready-20261101/`、実行記録は同名`outputs/`の各新規runに保存した。
`tools/record-run.py`で指定入力/成果物のsnapshot・SHA256・raw出力・終了値を記録する。
終了時に全作業物をagents側へhash照合して保全し、一時物を整理する。最終状態は同agentsの
`integration-completion.json`、記録間の照合は`pre-integration-audit.json`を正本とする。

| 実行 | 結果・範囲 |
| --- | --- |
| `pwsh -NoProfile -File …/build.ps1`（debug-01） | 正典Debug・clang-cl/tidy成功。製品、構造試験、予約試験をbuild |
| `ctest --test-dir build -R '^minimap_(scheduling\|structure)$' --output-on-failure`（contract-01/02） | 構造95checks成功。予約試験はfixture側のUIA操作参照で失敗し修正 |
| `cmake --build build --target minimap_scheduling_tests`と予約exe（test-build-02〜04 / contract-03/04） | 選択操作の区別、折り畳み表示では登録済み配置shortcutを使うようfixtureを修正。contract-04で140checks成功 |
| 同じ予約試験objectを変更前UI libraryへlink（red-link-01 / red-run-01） | 旧版は`document ready without a timer`で失敗。新しい期待が旧待機を検出する |
| 正典UI翻訳単位の隔離probe（state-compile-01 / state-run-01） | 実RichEdit/GDIの31checks成功。602回のstream通知、旧timer消去、stream失敗/再試行、像の空/完成、狭幅/最小化 |
| 合成文書の計測（compile/run-baseline-01、compile/run-request-01、parent-compile/run-01、repeat-run-01） | 62回、うち新実装53回・旧実装9回。全件描画完了・sample上限・構築1回。`findings.json`参照 |
| 最終整形後の対象build（test-build-05/06）と`ctest -R '^minimap_scheduling$' -V`（contract-05） | 140checks成功。05はコンパイル成功後、指定inputの生成`build.ninja`がCMake再生成で変わり記録側が失敗。06で入力選定を正し、生成物を変えず完了を再確認 |
| 台帳順序を明示した対象build/CTest（test-build-07 / contract-06） | 142checks成功。初回CI 41299960444の失敗と同じ症状を逆順fixtureで再現（reversed-order-red-01）。既存台帳が逆順でも準備で正し、同じ期待を通過（reversed-order-green-01） |

予約試験は専用desktop/dataと実RichEditを使い、timer通知を捨てて公開UI/idle境界から操作する。
文書切替・途中の再切替・閲覧/編集・行番号幅・1回16/合計256・native sample失敗/再試行、
menu/modal/IME・非表示/復帰、入力/resize待機と本文・選択・Undo・dirty・focusの保持を確認した。
追加probeは製品ソースを変更せず、生成した翻訳単位とnative subclassだけで観測/失敗注入する。
旧文書の像はstream開始前に消え、読込失敗時も空。新文書の一部を先に公開しないことをGDI画素hashで比較した。
初回CIでは予約試験の`long document begins in one slice`が失敗した。テストが走査順序を暗黙に仮定しており、
逆順の台帳で同じ失敗をローカル再現した。fixture準備で正典codec/portを使って台帳順序を明記した。
製品実装と16/256等の期待値は変更していない。CI上の実際の走査順序そのものは採取できていない。
追加の5桁確認は`gutter-run-02`の10checks成功。120 DPIで番号欄が実際に5px広がり、その後のpaintでも待機に戻らない。
最初のgutter fixtureは番号表示の指定漏れで失敗し、その記録も保持した。
`final-check-01/02`で規約/実依存graph・中核symbol・変更Cの整形・差分を確認する。結果は監査記録へ収載する。

## 実測

Windows、120 DPI、正典DebugのASan/UBSan/nullability付き、専用desktop上の合成文書。
QPCで既存処理の境界を測り、通常queue→idle→WM_PAINTを使う。手動timerで待ちを飛ばしていない。
GDI描画処理の終了時刻であり、DWMが画面へ提示した時刻ではない。実ノート内容は取得していない。

前段65回のうち通常文書35回と、今回同じ7文書を各5回測った比較。数値は中央値ms。

| 本文投入終了からマップ描画終了 | 変更前 | 変更後 |
| --- | ---: | ---: |
| 本文120行 | 128.346 | 13.440 |
| 本文2,000行 | 139.344 | 18.273 |
| コード2,000行 | 137.745 | 20.837 |

大きい文書の初回測定では本文の準備/投入にも大きな時間変動があったため、その記録を残したうえで、
旧`folio_window.c`と新実装を同じ変更のない依存libraryへ結び、近い時刻に順番に各3回測った。
共通条件の比較であり、ランダム化した統計実験や一般的な速度保証ではない。

| 近い時刻の比較（中央値） | 変更前 | 変更後 |
| --- | ---: | ---: |
| 表3,000行の更新予約回数 | 3,003 | 2（投入前破棄＋投入成功） |
| 表3,000行の予約処理ms | 24.265 | 0.027 |
| 表3,000行の投入後→描画ms | 142.397 | 44.437 |
| 本文2万行の投入後→描画ms | 156.738 | 62.782 |

前段の予約処理中央値42.579msと今回24.265msを混ぜて同時刻の改善率にしない。
2万行のnative投入自体は旧782.777ms・新783.792msで、今回の変更はこの大きな処理を高速化しない。

## 残る境界

hideの6秒待ちは合成条件で未再現。実ノートで同じ長い遅れが消えたとは未確認である。
大文書の本文準備、native内部の行参照時間、queue内の入力/paintや索引の優先による待ちは残る。
物理IME、実reader、DWM呈示、異なるDPI間、Releaseの時間比較は今回の測定に含めない。
見た目の設計は変更しておらず、前段の明暗/高コントラスト画像を再撮影していない。
