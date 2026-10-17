# ADR 0054 — 本文の流し込みを確認し、未完成の表示を編集・保存へ渡さない

- 状態: 受理（2026-10-15のhide指定・外部講評P1-1。設計: AstraのNeNeFolioサナ）
- 日付: 2026-10-15
- Issue: #310
- 規則: FR-005/006/015、ARC-001/004/008/009/010/011、C-002/005/008/012/018、QLT-008/012/013
- 関連: [ADR0006](0006-edit-mode-and-save-path.md)、[ADR0013](0013-index-focus-vim-keys-and-persistent-mode.md)、
  [ADR0038](0038-history-restore.md)、[ADR0050](0050-unsaved-note-departure.md)。

## 問題と修正前の根拠

main `55f08641`の実Win32隔離fixtureで、試験所有RichEditのEM_STREAMINに部分本文`part`と
dwError=ERROR_READ_FAULTを注入した。編集開始はREADY、控えは書込可能、変更印はfalseとなり、
後続の正典`store_body`が部分本文4 bytesを元mdへ公開した。元本文は履歴1へ残った。
正常な空RTFはEM_STREAMINの戻り0・dwError0で成功する対照も確認した。
`D:/NeNeFolio/agents/310-stream-failure/`にsource/hash・fixture前後・expected exit 1を保持する。
これは試験側の故障注入であり、実メモリ不足の再現ではない。

変更印を消さないだけでは防げない。保存の正典はRichEditから得た本文と保存済み本文を比較するため、
falseでも内容が違えば書く。通常編集だけでなく、編集中のノート切替、新規、履歴復元、明示破棄も同じ投入を使う。
applicationは切替先の本文/対象を既に採用している場合があり、単なるエラーメッセージ追加では旧/部分本文の誤保存が残る。

## 決定

1. 同じRichEditと同じEM_STREAMINの経路を使う。閲覧/編集の公開投入を、modeを引数に取る
   `note_pane_load`の1つへ揃え、閉じた結果型で成功・未使用可能・失敗・処理中を返す。
   旧`note_pane_render`/`note_pane_edit`は呼出し側と同じ変更で置き換え、二つ目の控えや平文の退避経路を作らない。
2. `note_pane`だけが、外部表示境界の`UNAVAILABLE / STREAMING / READY`を所有する。
   既存のstreaming boolをこの閉じた状態へ置き換える。applicationの文書・本文モードは引き続き唯一の正本であり、
   この状態は「選んだ本文を控えへ完全に投入できたか」だけを表す。modeの第2の写しを持たない。
   初期状態と失敗後はUNAVAILABLE。同期メッセージを送る前にSTREAMINGへ入り、入れ子の投入は外側状態を変えず拒否する。
   UIの再入操作も同じreadinessを参照して拒否し、外側が借用中のapplicationのRTFを再生成・解放させない。
   本文のEN_CHANGE/EN_VSCROLL通知と描画は既存の無効化経路へ通す。
   事前拒否は、編集RTF再生成を行うshow_note/enter_edit/履歴採用、閲覧RTFを替えるテーマ/言語採用、
   対象を替えるswitch_noteと、root/layer/EDITの共通入力境界、RichEditのEN_MSGFILTER通知に限定する。
3. 投入中は読み取り専用にする。EM_STREAMINのdwErrorと非負の文字数戻り値、callbackで未供給のバイトがないことを確認し、
   成功時だけREADYへ進む。戻り0は空本文の正常値なので拒否しない。RTFバイト数と戻った文字数は比較しない。
   編集時に書込可能へする処理も成功を確認し、成功した編集投入だけ変更印をfalseにする。
   失敗時は読み取り専用のまま、直前の変更印を戻す。本文・選択・Undoを元どおり戻せるとは約束しない。
   失敗cleanupで読み取り専用化を1回試しても失敗した場合は、そのRichEditだけをdisableする。
   親窓と既存の札は維持し、成功した再投入だけが子窓をenableする。無制限に再試行しない。
   同じ子窓を別の製品経路でdisableする所有者はない。親のsession等による入力停止は子のenableでは解除しない。
4. READY以外では本文取得・置換・行索引の再構築/有効cache採用を拒否する。
   保存/検索/置換が共用する本文取得から未準備の結果を返し、出力引数を変えない。
   保存への変換は専用の`FOLIO_STATE_PANE_LOAD_FAILED`へ揃え、メモリ不足や検索だけの文言へ潰さない。
   行番号の幅は既存の古い桁数保持契約を維持してよいが、不完全本文の行を表示しない。
5. 全投入の結果を呼出し側へ伝える。通常編集、reflect/opened、カテゴリ展開による選択、新規、明示破棄、
   閲覧移行、履歴復元、テーマ/言語の閲覧再描画、初回窓生成を含む。
   失敗から保存・履歴置換等の後続処理へ進まず、成功扱いを返さない。
   初期窓生成の失敗は既存の生成失敗経路へ返す。表示中の失敗は既存の失敗表示/本文下端の通知で明示し、
   受動的な再描画失敗も黙らない。未準備の通知はnote_paneの状態から導き、別の真偽へ複製しない。
   通知の有無が変わったときだけ共通の投入後処理で既存surfaceの下端予約を同期配置し直す。
   配置はADR0053の無対話な`arrange_command_contents`へ揃え、OS終了QUERY中に失敗のモーダルを開かない。
   終了通知phaseがIDLE以外のときは、テーマ・言語の受動的な再描画失敗もreadiness由来の通知だけで示す。
   APPROVED後の通常message loopから届くWM_SETTINGCHANGEでも失敗の箱を開かない。
6. applicationが選んだmode/文書対象は巻き戻さない。実RichEditを編集可能にせず、操作結果を失敗にし、
   取得の境界で誤保存を拒否することで「失敗しても編集開始成功としない」を実現する。
   ノート切替/新規で旧文書は既に保存または明示破棄されている。対象の原文はapplicationが保持するので再試行できる。
   これは一般的な本文rollbackや、一度失われたRichEditのUndoの復元ではない。
7. 既存の「閲覧」「編集」を再試行の入口にする。同modeでもUNAVAILABLEなら投入を試み、
   READYの同modeは今までどおり本文・選択・Undoを流し直さない。
   編集の再試行は、必ず`folio_state_pane_edit_rtf`で現在の`state.body`からRTFを作り直す。
   保存後の古いedit cacheをそのまま再投入しない。無題も通し、NAMED限定のselection検査で再試行を拒まない。
   選択済みの「閲覧」または「編集」の同mode札を再クリックする操作も未準備時だけ受け付ける。
   未準備EDITからVIEWへの移行は保存用本文取得で拒否されるので、両方の札を無条件の再試行入口とは案内しない。
   3言語の共通失敗文言は短く「本文読込失敗。同じモードで再試行。」の意味を伝え、通知と失敗の箱で共有する。
8. 履歴復元は、閲覧から入る場合または控えが未準備の場合にだけ、現在の保存済み原文から編集土台を再生成・投入する。
   成功してから選んだ版で置き換える。正常EDIT+READYの未保存本文は土台を流し直さず、既存のUndo1単位で復元する。
   最初の投入失敗後の同じ履歴行の再クリックでも回復できるようにする。
9. READYの通常編集に新しい再投入契機を足さない。テーマ/言語の編集時は従来の再着色/face変更だけ、
   閲覧移行は保存後、明示破棄は利用者の破棄意図、新規/切替は既存の離脱許可を通す。
   OS終了の保存も同じ本文取得を通るため、未準備を終了の成功へ読み替えない。

## 検証

修正前redを保持し、完成候補で次の直接境界を確認する。

- 正常空本文/非空本文、改行/Unicodeと既存編集の往復。正常EDITの同mode操作で本文/選択/Undo/変更印を保持。
- 編集投入の部分/空/旧本文と非0 dwError、未供給バイト/異常戻り、入れ子投入、本文取得/置換拒否。
  失敗後のCtrl+S、離脱、OS終了から今回の不完全本文をarchive/write/createへ渡さない。
- 名前付き初回編集、編集切替、新規無題、明示破棄、VIEWからの履歴、VIEW再描画の失敗と再試行。
  保存後の最新本文を使う再試行、正常EDITでの履歴Undo、3言語の失敗通知。
- 初回窓生成の失敗、対象build/tidy・変更した境界の既存unit・format/conformance。

Stream成功を字形やRichEdit全実装の無欠陥まで一般化しない。外部APIが成功を返しながら本文を変える別の問題は、
既存の編集不能文字検査等の契約で扱う。実メモリ不足・物理IME・全OS/DPIは未測定として残す。
schema・依存・ゲートは変更しない。最終head必須CIを維持する。Waivers: none。

## 採用しなかった案

- dirtyだけを直す: 保存の内容比較を止めず、不完全本文を公開する。
- EM_STREAMIN戻り0を失敗にする: 正常な空本文を拒否する。
- 失敗後も編集可能にしエラーだけ出す: 保存や後続操作の安全が保てない。
- 全application遷移を旧modeへ戻す: 切替済み対象・無題・履歴の所有を別に巻き戻す必要があり、失敗した控えの内容は戻らない。
- 2つ目のRichEditや自動的な平文fallback: 保存可能な本文の経路を増やし、failureが続く場合の安全を解決しない。

API根拠: [EM_STREAMIN](https://learn.microsoft.com/en-us/windows/win32/controls/em-streamin)、
[EDITSTREAM](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-editstream)。
