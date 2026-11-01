# ミニマップ表示までの重複処理と順番待ち — #512

hideは#510の改善を受理し、残る待ちの調査から対応可能な箇所を順に修正するよう依頼した。
元の体感は0.3〜0.6秒と訂正された。これは実測値ではなく、実ノート内容を採取していない。

## 正典経路と変更

- `folio_window.c`の既存idle入口で、ミニマップと索引準備を一単位ずつ交互に進める。
  次の優先対象だけを`folio_idle_work.h`の閉じたenumで持つ。各処理の進捗は元の所有者に残す。
  入力/paint優先、16sample/最大256、索引の一単位、IME/modal等の抑止は維持する。
- STREAMING中の本文EN_CHANGEに限り、主窓末尾の`ui_pointer_after`を省く。
  通知そのものと既存の無効化は保持し、投入成功/失敗後のpaintで最終ポインタを反映する。
  送り手のHWND/control idは既存通知処理と同じ判定を使う。通常編集や他の通知は従来どおり。
- 共通頭・caption・操作帯と更新矩形が交わらない場合は、その描画計算を省く。
  操作帯の外枠を配置と共有し、範囲外ならボタン幅のcache確認/参照も省く。
  同じbackbufferと既存描画を使い、別cacheや描画経路は追加しない。
- `minimap_scheduling_tests.c`へ、索引/ミニマップ双方の進行・取消・非表示を追加した。
  本文変換・RichEdit内部の残る負担は後続の切り分けで扱う。

規則: ARC-001/002/004/007/008/011/012、C-002/008/012/013/016/017、QLT-008/012/013、
GIT-001〜004、ADR0083/0084/0026/0068。憲章の所有者表・ADR・現在欄と#510の体感記録を更新した。
保存schema・版・依存・申請済みMSIXは不変。Waivers: none。

## 検証と原本

原本は`D:/NeNeFolio/agents/minimap-pipeline-fix-20261101/`。
各実行は同名`outputs/`の新規directoryへ`tools/record-run.py`で入力/成果物のsnapshot、hash、raw出力と終了値を残した。
入力の対応は`pre-integration-audit.json`、計測は`findings.json`、最終CI/main/通常反映/全保全/整理は
`integration-completion.json`で確定する。終了時は全作業物をhash照合してagentsへ保全し、一時物を整理する。

| 実行 | 結果・範囲 |
| --- | --- |
| `pwsh -NoProfile -File …/build.ps1`（debug-01〜03） | 正典Debug・clang-cl/tidy、製品と予約試験のbuild成功。02の記録失敗は下記参照、03は記録も成功 |
| `cmake --build build --target minimap_scheduling_tests`、`ctest --test-dir build -R '^minimap_scheduling$' -V` | fixture選択順を明示したcontract-03で286checks成功。描画帯の最終調整後contract-04も286checks成功 |
| 最終試験objectを親UI libraryへlink（red-link-01 / red-run-01） | `map progresses before pending index completes`で期待どおり失敗。旧索引優先を検出 |
| 正典UI翻訳単位の隔離probe（surface-compile/run-05） | 実RichEdit/GDIの470checks成功。stream602通知中のroot再判定0、成功/失敗後の指先、通常編集/dirty/Undo、画素一致、範囲外の幅参照0を確認 |
| 同じ合成文書の計測（before/after-normal/table、before/after-final-normal/table） | 計82回。うち最終実装の比較は26回、すべて描画完了。両実装を同じ不変libraryへlinkし、測定は順番に実行 |
| `final-check.ps1` | conformance/実依存graph・core/application symbol・変更C整形・差分を確認。結果は監査原本へ収載 |

追加probeは製品に試験窓口を設けず、同じcompile/link引数から生成した観測用翻訳単位を使う。
元の`folio_window.c.obj`がlink mapに混ざらないことを確認した。
親commitの描画関数を比較用に取り出し、同一の全体/部分更新に対する32bit GDI画像を全byte比較した。
3言語、明暗と合成高コントラストpalette、閲覧/未保存編集、hover/focus、広幅/OSが許す狭幅、
header/caption/操作群の境界とミニマップ/行番号帯を含む。窓最小幅によるclamp後の実矩形で比較した。
OS高コントラスト設定自体は変更していない。画像は主窓の自前描画で、native本文/drawerの子窓は含まない。
明暗の主窓画像も目視し、同じ部品配置・文字・枠を確認した。

最初の予約fixtureは同じノートを再選択して新たな投入を期待したため失敗し、別ノートとの選択順を明示した。
surface-compile-01/03はfixtureのAPI名/field名と診断format型の誤りで失敗した。
surface-run-02はfixtureの位置合わせが失敗後timer検証へ干渉し、通常編集のposted通知も未処理だった。
timer検証を位置合わせ前へ移し、編集通知をpumpしてsurface-run-04で416checks成功した。
その後、操作帯の幅参照を省く製品変更に対して最終05の470checksを実行した。失敗記録も保持する。
debug-02はcompile/link自体は成功したが、記録中のADR編集と試験exe指定先の誤りで記録側が失敗した。
入力を固定し正しい`verification-minimap`のexeを指定したdebug-03は、再configure/no-workと成果物の記録が成功した。
正典source/testは02から不変で、監査でsnapshotと最終成果物を結ぶ。最初の実graph確認はFile API query未作成で失敗し、
queryを作って03で再生成した。規約の例外やゲート緩和は加えていない。

## 最終実装の実測

Windows・120 DPI・正典DebugのASan/UBSan/nullability付き、専用desktopの実RichEdit/GDI。
QPCで既存処理の境界を測り、通常queue→idle→paintを使う。DWM呈示/Release/実ノートの時間ではない。
fixtureのENV行のsourceは親`b7bc453`の識別であり、各実行の実ソースhashは`vectors.json`で区別する。
時刻計測の子processは重ねていない。ランダム化した統計実験や一般的な速度保証ではない。

| 同条件の最終比較 | 変更前 | 変更後 |
| --- | ---: | ---: |
| 表3,000行のstream中ポインタ再判定回数 | 3,002 | 0 |
| 上記再判定の合計時間・3回中央値ms | 14.624 | 0 |
| 本文2,000行の主窓描画合計・3回中央値ms | 10.279 | 7.247 |
| 完成マップの部分更新時の共通頭/caption/モード/操作描画 | 各1回 | 各0回 |
| 索引pending時、マップ開始までの索引単位数・1回 | 8 | 1 |
| 上記の切替開始→マップ描画終了ms | 28.329 | 9.403 |

表全体の描画完了中央値は624.119→565.016msだったが、途中比較では677.293→687.723msだった。
通常本文2,000行の最終比較も163.549→169.720msで、未変更のnative投入時間が変動している。
削除した処理回数と索引との交互進行は確認できるが、全体時間の一律短縮や改善率は主張しない。
本文のMarkdown変換、native投入/表配置には大きな時間が残り、次の切り分け対象とする。

物理IME・実reader聴取・DWM呈示・異なるDPI間は未確認。通常版が起動中なら保存/終了を待って反映する。
