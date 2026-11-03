# ADR 0087 — 保存ファイルの日時と既存キーを一つのステータスバーへ写す

- 状態: 受理（2026-11-03、hideのステータスバー改善指示）
- 日付: 2026-11-03
- Issue: #530
- 規則: ARC-001/003/004/007/008/010/011、C-002/003/007/008/012/014/016/017/018、QLT-008/012/013
- 関連: ADR0030/0049/0056/0057/0060/0081/0085

## 決定

1. 下端に32 logical pxの常設ステータスバーを置く。本文は既存の下余白32pxを使い、
   一覧とsplitterもその上までとする。通知の出入りで本文の寸法・位置を変えない。
   既存のpane notice描画とACCESSIBLE_NOTICE（StatusBar）を拡張し、別の通知面は作らない。
2. 通常は左に更新日時、次に作成日時、右に既存のヘルプ／コマンド一覧のキーを示す。
   キーはfolio_shortcut記述子、句はui_textのja/en/zh-Hans表を使う。新しいキーや操作は作らない。
   幅が不足すれば作成日時、長い補助案内の順に省き、最後は更新日時を省略記号付きで描く。
   全文は同じStatusBarのUIA Nameと標準tooltipに残す。未選択はキー案内、無題は未保存を示す。
   画像や失敗等の通知がある間は日時を置き換え、通知を優先する。通知消去後は通常情報へ戻す。
3. status_background/status_textを単一のtheme_palette表へ追加する。既存のテーマ色を元に
   本文と異なる落ち着いた面と4.5:1以上の文字を固定し、UIはCOLORREF変換だけを行う。
   高コントラストはOSのbutton対を保持し、上辺の線でも本文と分ける。任意のOS設定色の比は保証しない。
4. 日時はファイルシステムのcreation/last-writeを意味する。記録開始日や編集開始時刻ではない。
   persistence_port.read_note_timesが既存のノートpath解決を使い、adapter内でGetFileTime、
   FileTimeToSystemTime、SystemTimeToTzSpecificLocalTimeExによって取得時のローカル時刻へ変換する。
   固定のYYYY-MM-DD HH:mm表現をcoreの不透明なnote_file_timesへ渡す。現在時刻・既定localeを
   core/application/UIで読まず、日時の別台帳・保存schema・自動更新timerを足さない。
5. applicationが現在の文書の日時snapshotを所有する。選択・初回/別名保存・実保存・改名・
   カテゴリ移動の確定時に更新し、無題/削除で解除する。取得不能は型で区別し、古い値を残さない。
   未完了改名では表示を取得不能へ落とす。日時取得失敗は本文の読込・保存成功を取り消さない。
   窓の再activationでも安全なときに同じ取得を行う。描画やキー入力ごとのI/Oは行わない。
6. file_bytesの既存原子的置換で、置換前ファイルの作成時刻を一時ファイルへ引き継いでから
   公開する。取得/設定失敗は公開せず、元ファイルを保持する。新規ファイルと別名保存は新しい日時。
   更新日時は実際の書込み後に取得する。同一本文の保存は従来どおり書き込まず、日時も進めない。
   履歴復元は現在ファイルの更新として扱い、作成日時を保持する。外部copyや他のeditorによる
   属性変更まで「最初にノートを書いた日時」として復元することはできない。

## 検証と境界

対象unitはsnapshot寿命・選択/保存/失敗・未保存・文言と配色。実Win32では固定時刻の合成ファイル、
原子的保存後の属性、主窓の明暗/狭幅/三言語、通知の出入り、UIA/tooltipと本文保持を確認する。
成功済み対象結果を再利用し、最終headの必須CIを統合条件にする。全OS/DPI/実readerの保証とはしない。
タグ・新規コマンドパレット・Store/配布更新・#466/#479は対象外。追加waiverはnone。

## OS契約の参照

- [File Times](https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times): ファイル時刻の取得と書込handle終了後の更新。
- [SetFileTime](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfiletime): 作成時刻だけの設定とFILE_WRITE_ATTRIBUTES。
- [SystemTimeToTzSpecificLocalTimeEx](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltimeex): 動的なタイムゾーン規則による変換。
