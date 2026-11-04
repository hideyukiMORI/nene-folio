# 小ノートのZIP件数上限と生成費用 — 2026-11-04（#608）

## 結果と判断

公開note_archive_createへ、一意な名前と1byte本文の0/1/128/1024/8192/65534件を渡した。
全6条件で有効なZIPを生成し、65535件はNOTE_ARCHIVE_TOO_LARGEでoutを公開しなかった。
上限65534件の生成は後4sample中央値20.39865ms。今回の範囲で不具合や新たな性能修正の根拠は得られず、製品変更を追加しない。

Issue: #608。規則: ARC-001、QLT-008/012/013、GIT-001〜004。文書5件だけの変更。
追加waiverはnone。既存WVR-0001はWinHTTP callbackだけで不変。

## 入力・手順・観測

原本は`D:/NeNeFolio/agents/archive-entry-scale-20261104`、実行先は同名outputs。
#605最終head5da9856の正規Release coreを、成功run artifact/現file/コピーのSHA256一致で再利用。
headerも同sourceから明示copyし、link-01のinput snapshotへ収載した。
同headはCI41818502692 full-gate/check成功後、PR607/main3a40067へ統合済み。関係入力は不変。

固定eng/toolchain.ps1を使い、外部scale.cをclang-cl C23 /MT /O2 /DNDEBUGでlinkした。
sourceに期待件数を固定し、各5sampleの初回を分離、後4sampleの中央値を記録した。
QPC境界はnote_archive_createだけで、名前検証/確保/CRC/コピー/ZIP headerを含む。
fixture準備・破棄・計測ZIPの保存・全ノートの読込・実UI/ファイル公開を含めない。

| 件数 | 後4sample中央値ms | ZIP bytes |
| --- | ---: | ---: |
| 0 | 0.00020 | 22 |
| 1 | 0.00095 | 113 |
| 128 | 0.03750 | 11962 |
| 1024 | 0.31040 | 97130 |
| 8192 | 2.36580 | 792426 |
| 65534 | 20.39865 | 6465668 |

本文はxの1byte、名前はA/n{index}.md。helperが全sample後に本文と全entryの不変を確認した。
Python zipfile/zlibで名前・一意な列順・件数・member bytes・CRC・STORE・UTF-8 flag・固定1980-01-01を独立照合。
ZIP長は22+各memberの77+2*name lengthという独立式とも一致した。
65535件は各6processで拒否し、許可件数を超えて有効entryを用意しなくても出力を公開しないことを観測した。

`tools/record-run.py`のlink-01/check-01は子/記録側0、raw stderr/recordingErrors空。
全18file（6ZIP/6raw/6stderr）とrecord artifactを再hash照合した。失敗した実行なし。
この診断はReleaseでありsanitizer検証とは扱わない。時間のCI閾値を作らず、全入力/全機種の応答期限へ一般化しない。
局所helperの確認であり、全ノートをファイルから集める操作全体の時間・メモリや実UIを保証しない。

## 完了と再利用

現在のサナが単体で実施し、追加エージェントなし。実app/実ノート/Storeを操作していない。
ローカルは文書参照・差分・conformanceだけで確認し、製品入力不変の成功済み検証と通常exeを再利用する。
最終head必須CIでeng/check.ps1成功後、正典統合・通常exe SHA不変・全保全hash照合・worktree/output整理を行う。
head/CI/main/保全整理の確定原本は同agents内のintegration-completion.jsonへ。
