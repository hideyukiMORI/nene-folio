# ADR 0053 — OS終了への同意は保存後に行い、最終破棄を通常終了と共有する

- 状態: 受理（2026-10-15のhide指定・外部講評P0-1。設計: AstraのNeNeFolioサナ）
- 日付: 2026-10-15
- Issue: #309
- 規則: FR-006/015/018/019、ARC-001/003/004/009/010/011、C-002/008/012/017/018、QLT-008/012/013
- 関連: [ADR0006](0006-edit-mode-and-save-path.md)、[ADR0016](0016-command-catalog-and-shared-execution.md)、
  [ADR0050](0050-unsaved-note-departure.md)。通常終了の保存/破棄の意味は維持する。

## 問題と修正前の根拠

main `55f08641`はWM_QUERYENDSESSIONとWM_ENDSESSIONを既定処理へ渡していた。
名前付き本文を編集した実Win32隔離窓にQUERY(LOGOFF)→END(TRUE, LOGOFF)を送り、
QUERYがTRUEを返したのにファイルが保存前のまま残ることを修正前に再現した。
`D:/NeNeFolio/agents/309-session-end/`に入力hash・source・fixture前後・expected exit 1を保持する。
実ログオフやWindows Updateを行った結果とは呼ばない。

WindowsのQUERY既定TRUEは終了への同意であり、後でWM_CLOSEが来ることには頼れない。
QUERYで窓を破棄すると、他アプリ等による終了取消でも元へ戻れない。
無題の保存名入力や改名復旧の面を終了通知の中で開くと、対話待ちと再入を招く。

## 決定

1. 終了を許可する条件と、許可後に窓を破棄する処理を分ける。
   通常×/Alt+F4/WM_CLOSEは既存`depart_document`、明示保存終了は`store_body`、`:q`は変更確認、
   `:q!`は明示破棄を使い、最後は全て`finish_quit`へ渡す。OS終了も同じ最終破棄を使う。
   生成失敗とmessage loop後の`folio_window_destroy`は所有資源の解放であり、利用者の終了要求ではない。
2. QUERYでは名前付き文書または文書なしについて、既存`store_body`を1回呼ぶ。
   保存内容の比較、履歴、原子的書込み、先行同期は既存application/adapterの正典経路を維持する。
   無題、改名保留、未確定IME、親が既に無効なモーダル、ごみ箱操作中は先に拒否する。
   QUERYから名前入力・改名復旧・失敗のモーダルを開かない。設定面内の未確定入力も同じIME抑止対象とする。
3. UIが所有する閉じた`IDLE / CHECKING / CANCELLED / APPROVED`の終了通知phaseを置く。
   保存を始める前にCHECKINGへ入り、再QUERYと文書/操作入力を拒否する。成功時だけAPPROVEDへ進む。
   保存中にEND(FALSE)が届いたらCANCELLEDとし、外側の保存が戻るまで入力と再QUERYを拒否する。
   戻った保存処理はCHECKINGだけを承認でき、CANCELLEDならIDLEへ戻してFALSEを返す。
   途中でIDLEへ戻して別のQUERYが始まる余地や、phaseと別の取消フラグの不整合を作らない。
   APPROVEDの再QUERYは追加保存せずTRUE。CHECKING/CANCELLEDの再QUERYはFALSE。
4. APPROVEDへ移した後、元focusを退避して親窓を無効にする。phaseを先に更新し、EnableWindowの同期通知へ備える。
   待機中は通常message loopの入力/acceleratorと、共通コマンド・選択・surface操作の入口を止める。
   承認した本文からENDまでの間に利用者の追加入力が保存から漏れないようにする。
   任意の外部プロセスによるRichEditへの直接改変は対象外。
5. END(FALSE)は終了しない。APPROVEDだった場合だけ親を有効に戻し、有効な元focusへ戻す。
   拒否前からモーダル等で無効だった親を勝手に有効にしない。本文・選択・Undo・開いていたsurfaceを流し直さない。
   QUERYで既に成功した保存を取り消すことはしない。
6. END(TRUE)はAPPROVEDのときだけ`finish_quit(READY)`へ進む。
   未承認/拒否済み/CHECKINGのEND(TRUE)で、アプリ自身が未保存窓を破棄する経路を追加しない。
   ENDSESSION_CLOSEAPP/LOGOFF/CRITICALは同じ保存契約で扱い、ビット値によって破棄許可を作らない。
   強制終了時にはOSが拒否を無視し得るので、未保存内容の保持を保証しない。
7. 拒否時は短い3言語の文言を`ui_text`に置き、ShutdownBlockReasonCreateへ渡す。
   通常のpane noticeにも同じ意味を示し、保存/操作を完了して再試行できるようにする。
   API失敗でもQUERYのFALSEをTRUEへ読み替えない。OS理由表示の成功までは保証しない。
   登録成功の所有をUIに持ち、取消/次の正常な承認/終了時に解除する。解除失敗は解除済みと扱わず、後で再試行する。
8. 失敗拒否は「何も保存されなかった」とは限らない。既存のmd公開後の検索写し失敗、台帳同期等の部分成功を維持し、
   完了できなかったため終了へ同意しないと扱う。保存済みの本文を未保存へ巻き戻さない。

## 終わり方ごとの契約

| 経路 | データと終了の扱い | 検証の境界 |
| --- | --- | --- |
| ×/Alt+F4/WM_CLOSE、メニュー/Exの終了 | 名前付きは保存、無題はADR0050の3択。明示保存・変更拒否・明示破棄の意味を保ち、最終破棄は共有 | 既存通常終了の成功/取消と`:q!`。保存失敗は窓を残す |
| Esc | 通常はfocus/入力面の操作でアプリ終了ではない。該当なし | 既存契約を変更しない |
| OSのQUERY/END | 上記phase契約。保存できない/未確定なら拒否、取消は窓を保つ | 修正前red、保存/故障/無題/IME/再入/取消/flags/理由API失敗の通知注入 |
| kill/クラッシュ/電断 | 通知なしでは終了関数を通せない。最後に成功した原子的保存と保存前履歴が回復の基点。未保存は失われ得る | 隔離fixtureの強制終了後に保存済みrawが保持されること。未保存の自動復旧を成功条件にしない |
| 描画/DPI/resizeの失敗 | 現行UIはこの失敗を終了要求へ変えない。保存/破棄の別経路を作らない | 関連コードの静読。変更しない描画経路の広範囲試験は行わない（QLT-012） |
| 長いI/Oの途中取消 | 保存は同期処理で取消APIなし。ごみ箱の同期操作中は終了要求を拒否する | ごみ箱実行中への通知注入。別の非同期取消機構は追加しない |
| 書込み途中の失敗 | 既存履歴/原子的保存の結果を使い、QUERYを拒否。成功した公開を巻き戻さない | 書込拒否・archive失敗・公開後失敗の直接境界 |

共通指示の「全終了を1関数」は、アプリが制御できる終了要求と最終破棄に適用する。
通知のないkillや生成失敗まで保存終了へ無理に流さない。
定期的な未保存draft退避は、保存形式/復旧/個人データ保持期間を伴う新機能なのでhide判断へ分離する。
これは終了通知のP0修正の前提にせず、今回実装しない。

## 検証と限界

製品変更前のredを保持し、完成候補の対象build/tidy・文言unit・format/conformanceと、
実Win32隔離fixtureで直接の境界を確認する。任意の実OS終了、物理IME、電断耐性を通知注入で保証したとは書かない。
保存処理がOSの待機時間内に必ず完了する保証もない。入力/source/hash・コマンド・結果を保持し、
最終head必須CIへ統合検証を集約する（QLT-012）。schema・依存・ゲート変更なし。Waivers: none。

API根拠: [WM_QUERYENDSESSION](https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-queryendsession)、
[WM_ENDSESSION](https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-endsession)、
[ShutdownBlockReasonCreate](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-shutdownblockreasoncreate)、
[EnableWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enablewindow)。
