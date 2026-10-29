# 背景読込の共有設定だけでは既存保存と両立しない

Issue: #438。base main `c860e0b`。2026-10-29。
規則: ARC-001/004/007/010/011、C-005/008/012/013、QLT-008/010/012/013、GIT-001〜004。Waivers: none。

## 結論と範囲

READ|DELETE共有readerと、shareREADにしたごみ箱preflightの候補を、既存保存・改名・専用ごみ箱portと組み合わせた。
改名・ごみ箱は今回の合成条件で成立したが、保存の最後のMoveFileExWがERROR_ACCESS_DENIEDで失敗した。
readerを閉じると同じ保存が成功した。この共有設定だけの背景読込案は採用しない。

現製品の同期readでは生じていない並行条件を先に調べたもので、新しい製品不具合の再現とは呼ばない。
製品source、共有規則、背景thread、ADRの受理状態、通常exeは変更していない。
初回45確認中2FAILと保存限定12確認中2FAILは、いずれも同じ保存場面の戻り値と新path本文の期待である。期待を変更して成功へ読み替えていない。

## 正典経路と診断の差

現file_bytes_readはGENERIC_READ / FILE_SHARE_READで開く。DELETE共有を追加するだけでは、
recycle_prepareのDELETE|FILE_READ_ATTRIBUTES / share0と双方向の共有条件が衝突する。
候補はpreflightのshareをREADに一箇所変更した非製品copyとした。書込・別DELETE access・DELETE非共有readerは引き続き拒む。
元の共有なし契約を変更するならADR0040の受理補正が必要であり、この診断では変更していない。

probeが合成mdのread-only handleを明示openして保持し、実portのwrite_note/apply_rename/trash_noteを呼んだ。
canonical parserを複製せず、背景worker統合試験とも呼ばない。RecycleItemの観測はtransfer解放後・destination解放前に一回置いた。
ごみ箱成功時だけ、APIが返した新destinationから$R/$Iを同定した。既存ごみ箱の列挙・内容読取・purgeは行っていない。

保存限定copyはtemporary CreateFileW、WriteFile、FlushFileBuffers、MoveFileExWの直後の結果を記録する。
観測後にGetLastErrorを復元し、引数・共有・flags・短絡評価・エラー分類・元の保存成功期待を維持した。
reader保持時とclose後の同じ保存を各一回だけ比較した。成功済みの改名・ごみ箱・拒否条件は再実行していない。

## 実行と結果

全試行はtools/record-run.py、新規run directory、入力snapshot、成果物snapshot、raw stdout/stderr、子/wrapper終了値で保全した。
原本のrootは`D:/NeNeFolio/outputs/438-read-sharing-feasibility/records/`、恒久保全は`D:/NeNeFolio/agents/438-read-sharing-feasibility/`。

| record | 子 / wrapper | 結果 |
| --- | --- | --- |
| configure-01 | 0 / 0 | 正典Debug設定 |
| build-01 | 1 / 125 | 診断HANDLE/HDESKのnull注釈とcandidate.cのheader扱いでcompile停止。未生成artifact2件も記録エラー。native未実行 |
| configure-02 | 0 / 0 | 診断の注釈と独立TU/forced-include headerへ補正。共有差分・oracle不変 |
| build-02 | 0 / 0 | core依存＋非製品probeのみbuild/tidy成功 |
| run-01 | 1 / 1 | 45確認 / 2FAIL / 0SKIP。保存成立性を否定 |
| save-configure-01 | 0 / 0 | 保存限定targetを設定、既存core再利用 |
| save-build-01 | 0 / 0 | 6診断TU＋linkのみ成功。core hash不変 |
| save-run-01 | 1 / 1 | 12確認 / 2FAIL / 0SKIP。保存失敗地点とclose後の成功を確認 |

build-01以外の7runは記録エラー0。初回buildの失敗も原本を残し、抑制・除外・閾値変更で通していない。

実行launcher（各内部のexact argv/input/artifactはrun.jsonを正本とする）:

```powershell
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-plan.ps1 -Phase configure
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-plan.ps1 -Phase build
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-plan.ps1 -Phase configure -RunId 02
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-plan.ps1 -Phase build -RunId 02
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-plan.ps1 -Phase run
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-save-plan.ps1 -Phase configure
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-save-plan.ps1 -Phase build
pwsh -NoProfile -File D:/NeNeFolio/agents/438-read-sharing-feasibility/execute-save-native.ps1 -AuthorizationRecord D:/NeNeFolio/agents/438-read-sharing-feasibility/parent-save-native-acceptance.json
```

### 初回suite

旧share0対照はERROR_SHARING_VIOLATION32、候補preflightはREADYだった。
DELETE非共有reader/writer/deleterの各条件はBUSYとなり、close後の原本文も一致した。
改名はCOMPLETED、元path不在・新path本文・旧handle本文を別々に確認した。

RecycleItemはHRESULT 0x00270008とnonnull destinationを返した。元held fileと新$Rのvolume/file ID、
対応$Iの元fullpath、$Rの27bytes本文が一致した。元path不在だけでは判定していない。
reader保持中の同名createはSTOREDで、新pathは新本文、旧handleは旧本文のままだった。
新しい$RWBLGH0.md/$IWBLGH0.mdを保持し、purgeしない。正確なpathはresults.json。

### 保存限定suite

18:21 JST、held時はtemporary Create成功→Write20/20成功→Flush成功→MoveFileExW FALSE、直後のGetLastError=5だった。
write_noteはPERSISTENCE_UNWRITABLEで、元pathと旧handleの本文は元のまま。共有違反32と混同しない。
reader close後は全観測stageが成功し、MoveFileExW TRUE、write_noteはPERSISTENCE_STORED、新pathの20bytesが一致した。
両native実行とも専用noninput desktopからの復帰・所有handle解放を確認し、PID31708/37596の終了を確認した。

## 証拠の照合と限定

親はrun-01の311入力/13成果物、save-run-01の309入力/4成果物について、現物とsnapshotの全hash・実行中入力不変を照合した。
保存限定configure286入力/3成果物、build305入力/2成果物も同様に照合した。
初回5runの保全1548ファイルとfinalsource23ファイル、実compile56entriesの一致も確認した。
全8runの2469記録ファイル/81,303,218bytesと各source保存copy、最終報告85ファイルも親が全hashを照合した（parent-final-acceptance.json）。
原本はparent-native-run01-acceptance.json、parent-archive-vector-acceptance.json、parent-save-build-input-acceptance.json、parent-save-native-result-acceptance.json。

親の最初のvector照合はfile単独辞書で複数targetを混同しexit1となった。exact(file,command)照合へ訂正し一致を確認した。
担当側のtidy抽出も同じsourceの別targetを混同して一度停止し、save targetへ絞って訂正した。どちらもbuild/native再試行ではない。
source読取時の存在しない候補名とPowerShell上のrg path glob指定も、inventory/rg -gへ訂正した読取エラーであり、試験結果とは区別する。

環境はWindows 10.0.26300、SDK10.0.26100.0、clang-cl19.1.5、正典C23/Debug ASan・UBSan・nullability/no-recover/MT、canonical clang-tidy。
六つのtoolbinaryはpath/version/hash/sizeを記録した。SDK/system import librariesの全保全までは行っていない。
初回native stderrにはASan interception_winのunhandled instruction一行がある。保存限定stderrは空だが、初回の制約を消さず、sanitizerの全面的な正常動作を主張しない。
任意のWindows版/媒体/filter/外部processとの互換性、全API失敗経路、性能・物理入力を検証したものではない。

## 一次資料と次の設計境界

[CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)の双方向共有条件を現sourceへ適用した静的判断と、今回の実測を分けている。
[RecycleItem](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itransfersource-recycleitem)のdestinationは観測対象の同定に使った。
[FILE_RENAME_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information)には既存handleを保つ置換のPOSIX semantics、
[FILE_RENAME_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)にはFileRenameInfoExでFlagsを使う説明がある。
これらは将来案の資料であり、現保存の失敗地点の代用証明や新保存経路の採用ではない。

背景化を進めるには、保存時の公開契約か読込/変更操作の順序付けを再設計する必要がある。
UIで読込完了を待つjoin、共有/保存flagの緩和、同期fallback、完全削除fallbackでこの結果を回避しない。
背景workerの寿命・取消・終了、corpus複製、matching準備、任意のOS待ち時間と極狭幅表の可読性は未解消。
性能計時・製品全体のローカル再build/回帰は今回追加していない。

文書の最終検証・必須CI・統合・通常exe hash維持・全保全・整理は、実PRと同taskのintegration-completion.jsonを照合する。
