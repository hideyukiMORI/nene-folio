# 設定JSONの固定語を全長で照合する — #628

表示設定と起動設定の固定語に、JSONで展開されたNULが含まれる場合の誤受理を修正した。
folio_settingsのkey/languageとstartup_settingsのkeyの3箇所で、既存の内容比較に展開後の長さ一致を加える。
正規のキーや言語の末尾にNULを付けた未知語を受理すると、不正設定を保持する既存の保護に入らず、
次の設定変更で原ファイルが正規形に書き替わり得る。実ノートや通常設定を使って再現していない。

## 契約と正典経路

Issue: #628。ARC-001/004/009/010/012、C-005/008/012/016、QLT-008/010/012/013、GIT-001〜004。
[ADR0025](../adr/0025-settings-file.md)決定1/6、[ADR0032](../adr/0032-languages.md)決定1、
[ADR0077](../adr/0077-note-storage-location.md)決定2/7にある未知キー/不正値拒否を守る修正である。
JSONの合法な \u0000 をreader全体で禁止しない。readerはbytesと長さを保持し、各codecが既存の語彙を検証する。

note_ledger、category_ledger、rename_plan、rename_journalの固定語には既に同じ長さ照合がある。
全json_reader_text利用箇所を照合し、名前/path/theme/IDの検証は既存の長さ付き入口を使うことを確認した。
製品変更は2ファイル・3比較だけ。新しい公開API/owner/確保/codec/保存経路はない。
設定版1〜5、startup版1、schema、依存、ゲート、文言は不変。追加waiver none、既存WVR-0001継続。

## 再現と回帰

静読で親と独立read-only reviewerが同じ欠落を別に確認した。最小反例は次のとおり。

- 起動設定: {"version\u0000bad":1,"directory":"D:/notes"}
- 表示設定key: {"version":1,"number\u0000bad":true}
- 表示設定language: {"version":3,"number":true,"theme":"dark","language":"en\u0000bad"}

旧実装で新しい表示設定/起動設定の拒否assertionがそれぞれ失敗することを確認した。
redは最初の失敗で停止するため、全候補やlanguage個別の旧受理を実測したとは扱わない。
修正版は新selector --settings-codecs から既存JSON/settings/storageの回帰をまとめて実行して成功した。

- 設定版1〜5の固定stringすべてで、各文字位置と末尾へ \u0000 / \u0000extra を挿入する。
  174位置×2＝348入力。既知key、テーマ値、ja/en/zh-Hansを含み、MALFORMEDと非NULL既存outの保持を確認する。
- startupのversion/directoryで先頭/中間/末尾NUL・末尾NUL+extraの8入力を拒否し、既存outを保持する。
- 正常なUnicode escapeのkey/theme/languageとstartupのkeyを受理する。比較は原JSONの長さではなく展開後の長さである。
- JSON readerはNULを含むkey/stringの3bytesと終端、後続tokenを維持する。
- 既存の版移行・schema拒否・書出し往復・default/言語/テーマ・path境界も同selectorで確認する。

## 実行と失敗原本

基点main19857d8、D専用fix/628-settings-tokens。対象検証後に#626のmain0e879fbをffした。
#626の差分は別test TUと文書であり、今回のcodec/対象test/ビルド設定は不変なので対象結果を再利用する。

| run | 結果 |
| --- | --- |
| configure-debug-01 | 子/記録0、Debugの正典設定 |
| build-red-01 | 子1/記録125。追加testのarray内隣接literalを-Wstring-concatenationが拒否。exe未生成のartifact記録error1 |
| build-red-02 | 同じ配列1要素を括弧で明示。残り9actions成功、警告/規則の抑制なし |
| red-settings-01 | --settings-codecs、意図した拒否assertionで子/記録1 |
| red-startup-01 | --storage-location、意図した拒否assertionで子/記録1 |
| build-green-01 | 製品3比較の修正後、Debug/tidyの4actions成功 |
| green-codecs-01 | --settings-codecs、子/記録0、stderr空、記録error0、全8input終了時不変 |

build/configureは全634inputを記録し、各runの入力終了時は不変。
最初のformatはLLVM絶対pathの推測が外れたため、既存eng/toolchain.ps1経由へ訂正した。
文書生成の最初のtool scriptはJavaScript構文で拒否され、ファイル未作成を確認してから修正した。
conformanceのinput-list準備は旧taskのFile API reply名を複製して不存在となり、子起動前に停止した。
新taskの実reply一覧で作り直した。最初のrun-directoryは未作成と確認した。
失敗も上書きせず原本を保持する。canonical Debugでは確保注入はinactiveのため、OOM sweep成功とは扱わない。
新確保はなく、既存の最終CI計装を維持する。フルゲートは最終head必須CIのeng/check.ps1に集約する。

## 原本・限界

恒久原本はD:/NeNeFolio/agents/628-settings-tokens/、全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。
対象source/test/exe/config/raw/終了値をrecord-run.pyで保存する。
文書/規約検査・最終CI・統合・通常反映/data不変・全保全/専用領域整理は同taskのintegration-completion.jsonで確定する。
branch/commitは保持する。実app・実ノート・Storeを試験に使わない。

coreの入力拒否と出力保持を検証したもので、実ディスクの障害やOS競合を網羅する試験ではない。
通常の書出しは元からNUL入り語を生成しない。修正前に既に書き替わった設定を復元する変更ではない。

## 統合・反映・整理の確定

PR629最終head a5643f37の必須CI41944717201 full-gate/check成功後、main1dc20301へ正典統合、Issue628 CLOSED。
merge-01は必須PR見出し5件不足でReady前に拒否された。本文だけ訂正して固定したmerge-02は成功、製品head不変。
旧Draft runのfailure/skippedはci-finalに保持し、required-finalに最終headの成功runを別途保存した。
通常build/check成功、data32file bytes/hash/作成更新日時不変。1,323file全保全/hash再照合後、専用worktree/outputを削除。
branch/commit保持。確定原本agents/628-settings-tokens/integration-completion.json。
