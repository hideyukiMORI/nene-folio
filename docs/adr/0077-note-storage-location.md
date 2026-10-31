# ADR 0077 — 保存先は別の起動設定で指定し次回起動から反映する

- 状態: 受理（2026-10-31、製品編集前。hideの#390方針と対応継続依頼による）
- 日付: 2026-10-31
- Issue: #477
- 規則: ARC-001〜012、C-002〜018、QLT-008/009/012/013、GIT-001〜004

## 決定

1. ノートの保存先は選択したフォルダー自身で、カテゴリ・台帳・表示設定・履歴を従来と同じ相対配置に置く。
   `persistence_adapter`は合成ルートから検証済みのrootを一回受ける。exeの位置を再解決しない。
   portableの起動設定不在時はexe隣`data`を維持し、空data/読取専用の既存起動契約も維持する。
2. 起動設定は`nenefolio-startup.json`版1、順序固定の`{"version":1,"directory":"..."}`。
   `directory`は絶対DOS/UNCパス。空文字列だけは配布形式の既定場所を意味し、portableを移動しても
   exe隣dataへ追随する。未知版・欠落/重複/未知キー・不正文字列は拒否し、上書きや既定値へのfallbackをしない。
   `storage_path`と`startup_settings`がcoreの唯一の検証/codec。JSONは既存reader/writerを使う。
   ノート側`settings.json`版4と台帳/履歴schemaは変えない。
3. `storage_adapter`が配布形式と既定pathをOSから解決する。package identityが無い版をportableとして
   exe隣へ起動設定を置く。identityがある版はWindows Known FolderのDocuments/NeNeFolioを既定root、
   LocalAppData/NeNeFolioを起動設定の場所とする。Documentsはpackage固有への転送を要求せず、実際の利用者の場所を使う。
   初回インストール版は既定値を確認するまでノートを開かない。取消は設定を書かず終了する。
   MSIX生成/配布/Store申請とMSI識別はこのIssueの範囲ではなく、実パッケージの更新/アンインストールは未検証として残す。
4. `storage_state`（application）が起動時のroot、次回起動用の設定、初回/保存先復旧のphaseを所有する。
   `storage_port`を通じて読込・選択先検証・設定保存を行い、UIや`folio_state`へ永続化を複製しない。
   runtimeの設定変更は次回起動から反映する。現在のpersistence・本文・Undo・dirty・検索/改名状態に触れず、
   終了には既存の保存/取消経路を使う。保存先変更でノートをコピー/移動/削除しない。
5. 起動時に明示済みの保存先が消失/読書き不能/使用中なら、理由と指定pathを見せて保存先選択へ戻す。
   起動だけで消えたrootを再作成せず、別の場所も自動採用しない。利用者が選択を確定したときだけ、
   既存親の直下のroot作成を許す。祖先を再帰作成しない。作った空rootは後続失敗時も削除しない。
   現在使用中のrootを次回設定へ戻す場合は新たなロックを取らず、起動時の検証へ戻す。
6. 選択先の検証はdirectory読取、共有なしdata lock、小さな作成/書込/flush/読取の一時probe。
   `data_lock`をpersistenceと選択先検証が共有し、`.nenefolio.lock`の取得経路を二つにしない。
   一時probeは既存fileを上書きせずdelete-on-closeで片付け、利用者ノートを読まない。
   新rootの検証lockは検証中だけ保持し、起動時の本lockを改めて取得する。将来の利用可否は保証せず、競合はその場で拒否する。
   この検証は全台帳/mdの健全性を保証しない。実読込は既存`folio_state_create`の単一経路で行う。
7. 起動設定は既存`file_bytes_store`による原子的置換。保存中は起動設定隣の専用lockを取り、
   読込時の設定と現在fileを比較してから書く。他プロセスの変更/未知版を上書きしない。
   変更がないruntime操作は書かず、全メモリ準備と検証/保存に成功してから次回設定を採用する。
   起動設定への書込権限が無くても、portable既定の従来起動は妨げない。
8. 設定へ「ノートの保存先…」を追加し、現在の場所と次回起動の場所をコピー可能な欄で表示する。
   場所の直接入力とWindowsのfolder pickerを同じ確定処理へ渡し、取消では何も設定しない。
   `dialog_theme`/`ui_part`の外形、既存`modal_owner`のguard/終了寿命、native入力のUIA名を共有する。
   folder pickerのOS部品へ独自配色や業務状態を持たせない。UIのshell32依存はfolder選択の初期位置設定に限る。
9. 初回と復旧時も同じ保存先入力面を使う。初期言語は既存起動失敗と同じJAで、通常設定ではノート側の選択言語を使う。
   所在の失敗は型付き結果と3言語の`ui_text`で伝える。初回起動のOS外観は既存appearance port経由で解決する。
10. 初回確認は主窓より先に作られるため、prompt自身がCOMを対で初期化/解放する。GDI+の取得/解放は
    `icon_paint_start/stop`へ集約し、主窓とpromptが各tokenを所有する。重複した起動処理を残さない。
    pathラベル・説明・buttonは言語/DPIの実測幅/高さで配置し、状態文は必要に応じてスクロールできるnative欄へ置く。

## 検証

core: UTF-8/絶対path/禁止文字/長さ、版付きJSON往復と拒否。application: portable互換・初回取消・
復旧・次回設定の不変性・書込拒否・同値・確保失敗。adapters: 実directory/lock/probe/競合設定/再起動を合成データで確認。
UI: 3言語、小窓、鍵/取消、コピー可能なpath、native名、未保存本文/Undoと既存modal終了経路を確認する。
成功入力を再利用し、最終headの必須CIで単一ゲートを要求する。実MSIXと物理IME/readerは未実施なら区別する。
Waivers: none。

## 一次資料

- [GetCurrentPackageFullName](https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-getcurrentpackagefullname)
- [SHGetKnownFolderPath](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath)
- [KNOWN_FOLDER_FLAG](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/ne-shlobj_core-known_folder_flag)
- [IFileDialog::SetOptions](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifiledialog-setoptions)
