# #369 — 初回全文検索の準備中も入力を処理する

日付: 2026-10-25。基点main: `fe2b3766207233a5aa4f5152374418008071fffe`。
規則: FR-032、ARC-001/004/005/007/010/011/012、C-005/008/012/013/016/017、QLT-008/012/013、GIT-001〜004。
Waivers: none。

## 変更と測定の判断

通常データはmetadataだけを確認し、3カテゴリ/6ノート/計379bytesだった。本文やノート名は出力していない。
新規合成データの現行Releaseでは、初回EN_CHANGEが1,000件で5,984ms、10,000件で64,602ms占有された。
10,000件のCreateFileWは合計63,597ms、UTF-8検証252ms、写し投入46ms、一致計算41ms。
全件を一度に読むことをUI停止の原因と判断し、実装前に[ADR0068](../adr/0068-idle-index-preparation.md)を受理した。
OS cacheをcoldへ戻していない。CreateFileW内部の待ち原因は未特定で、セキュリティ設定は変更しない。

`folio_state.c/h`が未完了の最新語と次の一件の位置を所有し、最初の入力はcorpus用本文を読まず要求を受理する。
既存read_note→note_text→note_corpus_putとbuild_filter/adoptを保ち、完成後だけ最新語の全結果を公開する。
語の差替えは進捗を保つ。取消/失敗は止め、未完成時の保存・索引変更と再開は先頭から読む。
読めない/不正本文の再読込では旧copyも除き、OOMを単なる非一致へ変えない。#219の修復先行契約を保持する。

`main.c`の唯一のloopで全範囲PeekMessageを優先し、queueが空なら一ノートを継続する。
抑止中/要求無しはWaitMessage、WM_QUITは即終了、WaitMessage失敗はloopを終了する。
`folio_window.c/h`は既存dispatch寿命と通常操作可否を使い、開始/取消/完成/失敗を同じ表示更新へ通す。
再入で終了要求が立ったら後続更新/失敗dialogを抑止する。毎ノートの全画面更新やUIA通知は追加しない。
`drawer_window.c`と`ui_text.c/h`は3言語の準備中表示を追加。結果の件数と部分cacheを混同しない。
単一UIスレッドを保ち、製品thread/timer/入れ子pump、port、保存schema、依存、ゲート変更は無い。

## 対象検証

恒久証跡: `D:/NeNeFolio/agents/first-search-20261025/`。
作業checkout: `D:/NeNeFolio/worktrees/369-first-search-responsiveness`。
一時出力: `D:/NeNeFolio/outputs/first-search-20261025/`。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/folio_tests.exe --index-preparation
python eng/conformance.py
git diff --check
python D:/NeNeFolio/agents/first-search-20261025/build-target-oom.py
pwsh -NoProfile -File D:/NeNeFolio/agents/first-search-20261025/build-candidate-profile.ps1
python D:/NeNeFolio/agents/first-search-20261025/run-candidate-profile.py 10000
pwsh -NoProfile -File D:/NeNeFolio/agents/first-search-20261025/build-native.ps1
python D:/NeNeFolio/agents/first-search-20261025/run-native.py 15
```

- Debug/Release buildとclang-tidy、対象単体、規約/空白検査が成功。関数宣言位置とloopの複雑度/入れ子の初回build失敗は修正しログを保持した。
- 新しい準備契約10条件と、既存の検索・保存copy修復境界・ui_textを対象selectorで実行した。
  旧完成後の試験は同じpublic set/continueを完了まで動かすdriverへ変更し、準備契約の試験は生APIを使う。
  一回一件、最新語、取消/並べ替え後の再開、旧copy除去、保存/改名/削除/新規、空索引、失敗/再試行を確認。
- 確保失敗21点を順に注入し回復した。語の確保失敗が以前の要求と進捗を保つこと、#219の6点の修復境界も成功。
  注入ビルドは既存coverageと同じASan構成。最初に追加したnullability注入は既存note_text.cの非null fieldへ失敗mallocを代入する境界で停止したため分離記録した。
  製品Debug/通常nativeはASan/UBSan/nullabilityを維持し、ゲート・除外・既存コードは変更しない。
- 実際のmain.c loopとfolio_window.cを使う外部native fixtureは16条件・221 checks / 0 failures、各process exit 0。
  未保存RichEdit本文/選択/dirty/Undoの保持と実Undo、最新語採用、途中クリア、native nested modal中の停止を確認した。
  IME通知/設定面/メニュー/無効ownerではWaitMessageへ入り、sent messageで復帰。6件の読込を完了し、busy-spin無し。
  WM_QUIT優先、WaitMessage失敗、途中close、read OOM一回通知、UI更新内の再入close後の通知抑止を故障注入で確認した。
  再入closeは更新呼出し内へsent WM_CLOSEを注入したもので、実readerの通知応答そのものではない。
- 初回nativeのモーダル解除timerが生成前に終わって待機したため、当該fixture processだけを停止し補助処理を修正した。
  初回14の累積refresh数と15の注入時点もfixture側を修正し、継続内の差分/実read失敗を条件として再検証した。
  各初回ログを保持し、成功済みの無関係な試験は繰り返していない。
- 3言語の準備中表示を専用desktopで撮影し、件数位置/クリア/本文の保持を目視確認した。実DPI120。
  native stderrは既知のinterception_win起動warningのみで、最終ASan/UBSan/nullabilityエラーは無い。

独立レビューは読み取り専用。設計の抑止中wait/取消後位置0に加え、実装のカテゴリ改名後の位置初期化順と再入close後のdialogを修正した。
前者は対象単体、後者はnative故障注入で検証し、レビューも解消を確認した。調査/設計/実装/検証は現在のサナが単体で実施した。

## 性能結果と限界

同じ件数/本文長の新規合成データを別のD領域へ作成。6件は各64bytes、1,000/10,000件は各10KiB、最大100カテゴリ。
候補では20ms間隔の確認messageと描画要求を追加した。基点にはこの負荷が無いため、全準備時間は同一負荷の速度比較ではない。

| 条件 | 基点の初回入力処理 | 候補の初回入力処理 | 候補の入力後から準備/描画終了 | 継続一回の最大 | message待ち最大 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 新規6件 | 12.023ms | 0.134ms | 98.532ms | 11.446ms | 19.164ms |
| 新規1,000件 | 5,984.025ms | 0.383ms | 11,500.533ms | 21.115ms | 25.471ms |
| 新規10,000件 | 64,602.313ms | 0.467ms | 139,863.758ms | 119.539ms | 114.680ms |
| 同じ10,000件・後の別process | 926.158ms | 0.163ms | 2,233.594ms | 84.804ms | 81.227ms |

新規10,000件では確認message4,386件・paint4,334回を準備中に処理した。全件読込の速度向上とは主張しない。
完成済みcorpusの再検索は従来の同期計算を使い、10,000件の後のprocessでalpha3.17ms、非一致z73.11msだった。
保存本文の読込/検証/写し作成の一件分と、最後の一致計算は同期であり、巨大な一ファイルやOS I/Oの待ち上限は保証しない。
入力/paintが途切れないと準備完了が遅れる。物理入力/物理IME/実reader/異なる物理DPI/DWM呈示は未測定。

製品入力hash・集計・終了値は`verification-final.json`、生測定はbaseline/candidateログと各summary。
最終PR headの必須CIは`pwsh -NoProfile -File ./eng/check.ps1`。CI/統合/通常exe/整理の確定結果は実際のPRと`completion.json`を照合する。
統合/必要反映/恒久成果物の収載後、当該追加worktree/一時出力を整理しbranch/commitを保持する。
旧#368/#373/削除拒否済み9箇所は再試行しない。稼働中アプリの終了・自動起動・公開Releaseは行わない。
