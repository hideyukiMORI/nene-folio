# ADR 0068 — 初回検索の本文準備を入力待機中に一件ずつ進める

- 状態: 受理（実装前の独立レビュー2点を反映）
- 日付: 2026-10-25
- Issue: #369
- 規則: ARC-001/004/005/007/010/011/012、C-005/008/012/013/016/017、QLT-008/012/013

## 測定と判断

現行main `fe2b376`のReleaseと、専用desktop上の新規合成データを使った。
1,000ノート×10KiBの初回入力処理は5,984ms、10,000ノートでは64,602ms。
10,000件のCreateFileW合計は63,597ms、UTF-8検証252ms、検索用copy/投入46ms、一致計算41msだった。
一ファイルの読み込み全体は中央値5.76ms、95%点9.24ms、最大81.30ms。
同じファイルを読んだ後の別processでは入力処理926msだった。
通常データのmetadataは3カテゴリ/6ノート/計379bytesであり、合成大規模条件と混同しない。
OS cacheは強制的にcoldへ戻していない。CreateFileW内部の待ちの原因は未特定で、セキュリティ設定は変更しない。
証跡: `D:/NeNeFolio/agents/first-search-20261025/baseline-*-*.log`と生成/計測スクリプト。

全件を一回のEN_CHANGEで読むことが長い停止の原因である。
一ファイルごとの同期readをメッセージループの待機時に分割し、ファイル間では入力/paintを優先する。
背景thread、時刻port、timerごとの待ち時間、入れ子のmessage pumpは追加しない。
一回のOS read自体の待ち時間は上限保証しない。今回の対象は全件分を連続してUI入力から占有する構造である。

## 決定

1. `folio_state`が既存corpusと完成済みの語/一致集合に加え、未完了要求の最新語と次に読む(category,note)を所有する。
   未完了の語は現在の検索結果ではなく、採用待ちの意図。UTF-8検証済みの所有値一つだけを持ち、UIへ複製しない。
   本文は既存`read_note`→`note_text`→`note_corpus_put`だけで読む。保存済み本文だけを扱い、未保存RichEditへ触れない。
2. `set_index_filter`は語の検証/先行同期を維持する。corpus完成済みなら従来の同じ一致計算/採用を同期で実行する。
   未完成なら検証済みの語を準備要求として受理し、その呼出しではcorpus用の本文を読まない。先行同期は既存契約どおり行う。
   準備中に新しい有効語が来たら語だけを差し替え、本文読込位置を巻き戻さない。壊れた語や確保失敗は前の要求を保つ。
3. 未完成corpusの準備開始時には旧一致集合と旧確定語を外し、通常の全索引を表示する。
   準備中の全索引を検索結果とは表示しない。見出しの件数位置は3言語の「準備中」表示にし、クリア操作を使えるようにする。
   `filtering`は準備要求または確定集合の存在を表し、既存の並び替え/開閉/カテゴリ操作の拒否を準備中にも適用する。
   名前変更/ノート保存/新規公開/ノート削除は既存契約のまま許す。現在文書、mode、選択、Undoは保持する。準備開始でcursorを移さず、完成/取消時に既存規則で整える。
4. 一回の`continue_index_filter`は最大一ノートを読む。読込不可/不正UTF-8はそのidentityの旧写しも除き非一致とし、OOMは失敗として返す。
   全件終了後に既存`build_filter`と採用経路を一度だけ通し、その時点の最新要求を結果へする。部分結果は公開しない。
   準備の途中失敗は要求を破棄し走査位置を0へ戻して停止し、既存の結果型で通知する。次の有効入力で再試行できる。
   検索欄を空にすると要求を破棄し走査位置を0へ戻し、以後読み進めない。再開は全件を先頭から読み直す。
   partial本文が残っていても読込済みとしてskipしない。state破棄も同じ所有物を解放する。
5. 未完成corpusへの索引identity/順序の変更は、要求の有無にかかわらず既存`index_changed`で読込位置を0へ戻す。
   保存済み本文が変わる`refresh_copy`も、未完成corpusなら要求の有無にかかわらず位置を0へ戻す。色などの保守的な巻戻しは許容する。
   不完全corpusは一致計算に使わない。再読込不可時の旧copy除去により、保存/削除/再作成後の古い本文を結果へ混ぜない。
   改名/保存写しの修復保留がある場合は、既存`synchronize`の成功前に読まない。失敗なら準備要求を停止する。
   既存の完成済みcorpusの差分更新/修復は変更せず、#219の保存成功とcopy修復を分離した契約を保つ。
6. 合成ルートの既存message loopを、全範囲`PeekMessage(PM_REMOVE)`で一件取得し通常dispatchする一つのloopへ揃える。
   取得したWM_QUITはtranslate/継続より先に終了する。queueが空のときだけ、副作用のないUIの実行可能getterを調べる。
   準備要求があり通常操作可能なら一ノート継続し、準備無し/抑止中なら`WaitMessage`で新規messageを待つ。
   WaitMessage失敗はloopを終了し、無制限retryしない。空queue確認とwaitの間でqueue確認/dispatch/pumpを行わない。
   PeekMessage内でsent messageが処理された後にも実行可否を再確認できるため、解除用のWM_NULLや別timerは追加しない。
   別のdispatch loopを作らず、既存TranslateAccelerator/TranslateMessage/DispatchMessageを共用する。
   読込継続自身はmessageをpumpしない。準備中の入力/描画は通常loopへ戻って処理する。
7. UIの継続入口は既存のbegin/end_dispatch境界で包み、close/session/native modal/補助面表示/IME/本文投入中などの抑止に従う。
   native modalの入れ子loopでは準備を進めず、戻ってから続く。閉じた窓や終了要求後へ成果物を反映しない。
   開始・取消・終了・失敗の表示更新は`filter_input_changed`と同じ更新経路へ合流する。毎ノートの全画面描画やUIA通知は行わない。
8. 語/一致のcore実装、persistence port、保存schema、依存は変更しない。C-013の単一UIスレッドは保つ。
   APIの準備/継続契約、所有表、ADR0024の一括同期読み込みの決定、C-013の文言を同時に更新する。

## 検証

- application: 先行修復が無いsetでcorpusを読まない、continue一回一件、空語取消、最新語のみ採用、不正語/OOMの維持/再試行、欠損/不正本文、
  途中保存/新規/改名/削除、移動の拒否と解除後の既存同期、未保存本文保持、#219の修復保留との境界。
- native: 実message loopを専用desktopで使い、準備中の入力・paint・取消・close・modal・UIA通知の寿命を確認する。
- performance: 新規1,000/10,000件と再processの同条件で、初回入力の時間、継続一回の最大、準備全体時間を別々に測る。
  隔離fixtureの疑似入力と物理入力は区別し、成功済みの無関係な検証は繰り返さない。
- 最終headの必須CIは既存`eng/check.ps1`。ゲート・閾値・抑制・waiverは増やさない。

## 独立レビューと根拠

実装前の読み取り専用レビューで、要求あり/抑止中のbusy-spinと、取消後に旧走査位置から再開する読み落としを指摘された。
決定4/5/6/7へ上記の待機/位置0/可否条件を反映した。追加の重大な欠陥は指摘されていない。
[PeekMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew)はsentを処理してからqueueを調べる。
[WaitMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-waitmessage)は新規queued/nonqueued/inputで解除され、既観測messageだけでは解除されない。
全範囲Peekが空を返した直後だけ待ち、待機判定に副作用を入れない。

Waivers: none
