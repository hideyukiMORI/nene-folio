# ADR 0082 — 外部Markdownの追加と原文の書き出し

- 状態: 受理（2026-11-01、製品編集前。hideの実装依頼）
- 日付: 2026-11-01
- Issue: #494
- 規則: ARC-001〜012、C-001〜018、QLT-008/009/012/013

## 決定

1. 操作メニューと共通commandへ、カテゴリへのMarkdown取り込み、現在ノートのMarkdown書き出し、
   保存済みカテゴリ／全ノートのZIP書き出しを追加する。カテゴリは既存cursor/currentの対象規則で指定し、
   選択ダイアログの題名にも名前を示す。カテゴリ未選択時は既存の理由を表示する。
2. `transfer_port`を合成rootから`folio_ports`へ渡す。外部ファイルの読込と新規公開は
   `transfer_adapter`、本文と名前の検証は既存core、取り込みの繰返しと列挙は`folio_state`が持つ。
   絶対DOS/UNCパスは既存`storage_path`の同じ検証をファイルにも使い、存在や種別はadapterで扱う。
3. 複数のUTF-8 `.md`を選択順に処理し、既存`create_note`と台帳の公開／再同期へ合流する。
   同名はスキップし、元ファイルと既存本文を上書きしない。不正入力・I/O・台帳失敗はその位置で停止し、
   追加済み数・同名数・未処理数と理由を示す。成功済みを削除して巻き戻さない。
   追加は現在本文・文書選択・Undo・dirty・モードを変更しない。初回履歴は作らない。
   検索の派生集合は既存corpus/filter経路で更新し、台帳未同期は既存index_pendingで保護する。
4. 現在ノートの書き出しは閲覧ならMarkdown原文、編集なら未保存を含む本文を既存`copied_text`で取得する。
   これはC-014のUTF-16入口の追加だが変換／改行／BOMの実装は増やさない。無題も書き出せる。
   取り込みと単票／ZIP出力はADR0042の本文・保存表現に従う。元rawのBOM有無は保持状態に追加しない。
   元ノートの保存・命名・履歴追加・選択変更は行わない。
5. ZIPは保存済みの本文を既存persistenceから読み、`カテゴリ/ノート.md`だけを収める。
   検索で絞り込んでいてもカテゴリ／全ノートの対象は保存済み全件で、メニューでも利用できる。
   空カテゴリはファイルを持たないためZIPに含めず、空集合は有効な空ZIPとする。
   台帳・設定・履歴は含めず、完全バックアップ／復元とは呼ばない。
6. ZIP32のSTORE方式（無圧縮）、UTF-8名、CRC32、固定1980-01-01をcoreの純粋な`note_archive`で生成する。
   入力は全件読込を成功させたsnapshot。ファイル数65534、完成ZIP128MiBを上限として事前検証し、
   超過／確保失敗で出力を公開しない。時刻や環境をcoreへ入れず、依存を追加しない。
7. 外部出力も既存`file_bytes`の一時ファイル→flush→置換なし公開を使う。既存出力への上書きは拒否し、
   別の名前を選ぶよう知らせる。通常の本文／台帳16MiB制限は維持し、検証済みarchive専用入口だけ128MiBを扱う。
8. Windowsのファイル選択はUIに閉じ、既存modal owner/入力guard/終了要求とfocus復元へ合流する。
   複数選択と保存先選択以外のファイルI/OをUIへ置かない。同期処理であり、新threadや入れ子pumpは追加しない。

## 検証と限界

同名／不正UTF-8／途中失敗／台帳未同期／元本文と選択の保持／未保存の書き出し／ZIPの境界と確保失敗を対象単体で確認する。
実ファイルの元hash保持・新規公開・独立ZIP読取とnative選択面を隔離fixtureで確認する。
大きい一括処理は同期時間とメモリを要する。ZIP64、圧縮、ZIP取り込み、全設定の復元は今回扱わない。

一次資料: [ZIP仕様](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT)、
[IFileOpenDialog::GetResults](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileopendialog-getresults)。
Waivers: none。

## 既存UI規定の補正

ADR0060の操作メニュー3群7件を4群11件へ拡張する。既存の最初の2群と破棄群の間に取り込み/書き出し4件を置く。
カテゴリ右クリックにも取り込み/カテゴリZIPを加え、描画・キー・UIA・実行は既存のメニュー記述子を共有する。
直接shortcutとEx別名は増やさない。操作名と保存先選択の題で、ZIPが保存済み本文を対象とすることを示す。

## 2026-11-04: CRCの1byte表で同期生成費用を減らす（#605、製品編集前）

合成payload 1/8/32/64MiBの実`note_archive_create`はRelease中央値
3.4884/28.6329/111.42085/225.68755msだった。唯一のprivate CRC32関数の
16-entry表による2nibble処理を、同じpolynomial `0xedb88320`の256-entry表による
1byte処理へ置き換える。初期値・最終反転・unsigned byteの解釈は維持する。
不変表は64から1024bytesへ増える。新しいowner・確保・公開API・依存・CPU命令は追加しない。

生成ZIPの全bytesとCRCの独立照合、同じRelease生成処理の比較で互換性と費用を確認する。
ZIP32 STORE・UTF-8名・固定日時・上限・同期呼出し・原子的な公開は既存のまま。
CRC-onlyの外部実験をZIP全体の改善実績とは扱わず、CIへ時間閾値を導入しない。
