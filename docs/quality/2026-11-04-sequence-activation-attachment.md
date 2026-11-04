# 活性棒の外縁へ接続するsequenceメッセージ — #573

活性中のmessageが生命線の中心xへ接続し、矢印が棒の中へ入っていた。16段の自己messageでは折返し線も棒内に埋まった。
実font/GDIの7入力×明暗で76端点中54件が外縁に接触しないことを再現し、修正後は全76端点が期待する側へ接続した。

## 正典経路と判断

製品編集前に[ADR0089](../adr/0089-native-lazy-mermaid.md)へ方針を追記した。
既存`diagram_sequence_layout.c`の確定済みy/end/depthを使い、`activation_bounds`を棒描画と接続判定で共有する。
送信・到着の各yで、同じ参加者の実際に表示される棒の外側を選ぶ。棒がない場合は生命線の中心へ戻す。
開始/終了境界、最小高8、未閉鎖のbottomは既存描画と同じであり、activation stackを再実行しない。

同workspaceへ相対xの2048件×2のint配列を加え、測定段階で一度計算して幅予約と出力が共有する。
無活性図では走査しない。活性図は各messageの2端点×上限内イベントを照合する。追加確保回数は0、workspaceは16KiB増える。
自己messageは両端の大きいxから40の幅で折り返し、文字もそこから従来の24を空ける。右余白予約へ同じ相対xを加える。
参加者間は失われる距離を`widen_span`へ足し、実測文字幅+32の間隔を確保する。
必要な参加者のx、図の横幅、message文字の中心は変わる。y・矢頭・線種・活性期間・primitive数・公開API・ownerは維持する。
schema・版・依存・renderer・worker・gateの変更はない。

## 試験の境界

既存native試験へ`--activation-attachments`を追加し、引数なしの必須CIにも結線した。
実sceneの生命線とactivation primitiveを使い、端点が見える棒の外縁の1logical px以内へ接触することを確認する。
棒の輪郭は既存native描画による判定を使用し、線種/矢頭・自己pathの点数・モデルmessageとの件数対応も確認する。
全14bitmapを保存した後に失敗数を集約する。製品のactivation状態を試験側でも再実行する方式ではない。

| 添字 | 入力 |
| --- | --- |
| 0 | `+/-`による呼出/返信 |
| 1 | actorへの逆方向、明示activate/deactivate |
| 2 | 自己messageの開始/到着、入れ子の開始/終了境界 |
| 3 | 双方の活性、入れ子、両側矢頭・開いた矢頭・終端記号 |
| 4 | 16段未閉鎖の自己message、多行文字、左注記 |
| 5 | 無活性の往復/自己対照 |
| 6 | 双方16段、長い往復文字、終了済み |

coreには深さ1〜16×自己/双方×閉鎖/継続の64条件を追加。長い文字と活性棒の非交差、右外余白、自己の折返しが棒外にあること、
閉鎖後のmessageが生命線に戻ることを確認する。横方向の変更が影響する既存frame見出し/message文字の画像比較も確認した。
修正前4枚・修正後4枚を視認。無活性対照の明暗2枚は全SHA256が不変、他12枚は変更された。
文字矩形外の不変は主張しない。今回は接続点と必要な横幅自体を変えている。

## 試行と結果

作業先`D:/NeNeFolio/worktrees/573-activation-attachment`、出力は同名`outputs/`、恒久原本は同名`agents/`。
開始mainは`5fe5c24`。固定Debug/Ninja・clang-tidy・ASan/UBSan/nullabilityで対象buildし、`tools/record-run.py`へ入力/argv/raw/終了値/成果物を保存した。

| run | 操作/結果 |
| --- | --- |
| build-red-01 | nativeのstyle include不足・fixture生成の複雑度11/10で子1。未生成exeのartifact snapshot失敗で記録側125 |
| build-red-02 | include追加・深いfixture生成を分離。製品未変更build成功、子/記録側0 |
| red-01 | native14bitmap、76端点中54件不接触、子/記録側1 |
| build-green-01 | message_offset複雑度11/10、子/記録側1 |
| build-green-02 | 右側幅の選択をloop外へ出しbuild成功、子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --activation-attachments <green-bitmaps>`、76端点全接触・435checks成功 |
| core-green-01 | `folio_tests.exe --diagram`、追加64条件を含む図対象成功 |
| frame-control-01 | `diagram_render_tests.exe --frame-headings`、見出し上書き0・278checks成功 |
| message-control-01 | `diagram_render_tests.exe --message-labels`、文字上書き0・242checks成功 |
| verify-01 | clang-format、conformance違反0、symbols core/application2lib違反0、diff成功 |
| build-release-01 / link-01 | 固定Releaseの同じ製品libraryと外部QPC計測harnessをbuild/link、成功 |
| paired-01 | 5入力×交互6組、12process・60画像・生成/cache各1260sample成功 |

成功runはすべて子/記録側0。build-red-01以外のrecordingErrorsは空。verifyのstderrはADRのCRLF→LF案内のみで検査エラーなし。
失敗したbuild/redを成功に置き換えず保持する。`run-summary.json`へ全結果/bitmaphashを収載。
stdout.txtの推測、旧性能task名、不存在のquality/README読取は診断失敗として記録し、製品検証へ数えない。

## 追加計算量のRelease計測

旧版は#569成功済みRelease（source`be19334`）を保全ZIPのSHAと個別filehashで照合して取り出した。
製品source差分は今回のsequence layoutと#571のflow layoutだけで、後者はsequenceのみの入力では通らない。
font7fileと計時コードは一致。旧/現行を交互順に6組実行し、各process/inputの初回を別記、続く20回の中央値を求めた。
表はその6process中央値の中央値と組差の中央値であり、単独UI操作の所要時間ではない。

| 入力 | 旧生成ms | 現行生成ms | 組差中央値 | 文字計測/配置ms | 遅い組 |
| --- | ---: | ---: | ---: | ---: | ---: |
| activation-0.mmd | 1.154 | 1.181 | +1.24% | 0.289 → 0.298 | 3/6 |
| activation-4.mmd | 2.848 | 3.118 | +8.91% | 0.695 → 0.707 | 6/6 |
| activation-5.mmd | 1.674 | 1.687 | +2.13% | 0.409 → 0.412 | 4/6 |
| activation-6.mmd | 2.610 | 2.762 | +4.61% | 0.682 → 0.650 | 6/6 |
| activation-dense.mmd | 17.979 | 18.787 | +4.16% | 0.359 → 0.671 | 6/6 |

4つのnative入力は旧/現行それぞれのred/green bitmapのSHA256と寸法を全回照合した。無活性対照は旧/現行同一画像。
多イベント入力は30回×双方16段の開始/1message/終了、1,950イベント・960活性開始・30message。
この入力の寸法/hashは各版の初回から固定して残り5回と照合した安定性確認であり、旧/新の同一画像は要求しない。
全生成sampleは正有限値・phase和一致、cacheも正有限値。初回最大96.267ms、非初回最大27.941msも除外せずrawに残した。
OS報告のprocess全期間CPU busyは7.38〜13.40%。自身のbuild/反映/保全と同時実行せず、他processの負荷やCPU affinityは制御していない。
QPC範囲は`diagram_render_read`とcache copyで、process起動・font登録・worker待ち・UI timer・画像保存は含まない。
全sample/phase/負荷は`paired-01`、要約は`paired-01-summary.json`、件数/最大値は`raw-audit.json`。
16段自己・双方・多イベントは6/6組で生成時間が増えた。自己は横幅388→484、双方は396→498、多イベントは344→373へ広がり、
描画中央値も増えている。自己の生成+約0.270ms、双方+約0.152ms、多イベント+約0.808msを観測した。
多イベントの文字計測/配置は0.359→0.671ms（+約0.312ms）で、追加走査の負荷を含む増加が全6組にある。
他のphaseや寸法も変わるため、生成全体の差を走査だけに帰属させない。無活性対照の組差は正負が混在した。
この有界入力の接続を正す費用として保持し、速度改善/差なしとは報告しない。追加の状態機械や索引を加える最適化は行わない。
多イベントの最終bitmapも視認した。有限入力の計測であり、任意入力の最大時間やUI応答を保証するものではない。

## 自己レビュー・限界・完了条件

単体で実装/自己レビュー/対象検証を実施。規則はARC-001/003/004/008/012、C-002/008/011/012/014/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
追加waiverはnone、既存WVR-0001はWinHTTP callbackのみ継続する。今回の7入力/明暗/16段と5入力の性能観測であり、任意図の衝突・全DPI/書体・最悪時間は保証しない。
実app/実ノートを試験に使わず、Store再提出/公開を行わない。最終head必須CI・正典統合・通常反映/data照合・保全整理の確定値は
`D:/NeNeFolio/agents/573-activation-attachment/integration-completion.json`へ収載する。
