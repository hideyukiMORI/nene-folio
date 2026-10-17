# サイズ計算の表現可能性 — 2026-10-17

Issue: [#311](https://github.com/hideyukiMORI/nene-folio/issues/311)。
基点main: 1e799d1e16eb09aa82c6458161c20e9868fe5d0d。
恒久証跡: D:/NeNeFolio/agents/311-size-overflow/。

## 到達経路と評価

外部講評P4が指摘した4実装を、現在の呼出元と上限へ照合した。
現在のx64製品で算術overflowに自然到達する入力は確認していない。
以下のSIZE_MAX近傍の試験はcoreへ渡す宣言長の境界であり、利用者操作での製品再現ではない。

| 経路 | 現在の上流制限 | 最初にあふれる宣言長・必要量 |
| --- | --- | --- |
| text_bufferの終端付き必要量 | note_text_from_editorは正規化後16MiB以内、markdown_rtfは受理済み本文を定数倍以内で展開 | count = SIZE_MAX - 現在length |
| UTF-16 → UTF-8 | 本文RichEditはINT32_MAX単位、入力欄は固定長 | length = (SIZE_MAX - 1) / 3 + 1 |
| UTF-8 → UTF-16 | 保存本文16MiB、名前・入力欄・固定文言 | length = SIZE_MAX / sizeof(char16_t) |
| note_replaceの累積 | 置換入力255単位、一致200000件、本文RichEditはINT32_MAX単位 | 旧計数は事後に4000000単位上限を検査していた |

x64のSIZE_MAXは18446744073709551615、UTF-8変換の最初のoverflow長は6148914691236517205単位、
UTF-16変換は9223372036854775807 bytes。
置換の粗い安全側上界 200000 × (255 × INT32_MAX + 255) + INT32_MAX もSIZE_MAXより小さい。
詳細な呼出元とファイル行はreachability.mdを参照する。保存上限と算術上の表現可能性は別の契約である。

## 実装判断

正典は既存のtext_buffer、utf8_text、utf16_text、note_replace。
新しい汎用計算APIやテスト用の製品窓口を作らず、それぞれの確保・計数の入口で検査する。

- 変換先の最大必要量をsize_tで表現できなければ、入力読取りと確保の前に既存のOUT_OF_MEMORYを返す。
  この値は「必要な容量を確保できない」を表す。新しいenumやUI文言を増やさず、失敗時out保持をヘッダに明記する。
- text_bufferは終端を含む必要量を加算前に検査する。失敗ラッチ以外のbytes/length/capacityは不変。
  通常の過剰確保だけがあふれるときは、表現可能な必要量そのものへ戻してreallocを試みる。
- note_replaceはliteral・capture・非一致prefix・tailを、残りの4000000単位以内か加算前に検査する。
  成功した値だけを必要長の出力へ渡す。測る→確保→書くの2段と選択経路は共通のままにする。
  1件・全部・各行最初、NOT_FOUND/BAD_SPAN/PARTIAL_MATCHES、失敗時out保持を維持する。
- 16MiBの保存上限、置換4000000単位、一致200000件は変更しない。
  [ADR0028](../adr/0028-regex-replace.md)の確保前拒否を回復する内部修正であり、公開API・状態所有・保存形式は変えない。

## 検証結果

修正前の基点sourceを凍結し、run-red.ps1の最小probeでexpected exit 1を得た。
UTF-8/UTF-16ともoverflow宣言長でmalloc(0)を1回要求し、bufferは必要SIZE_MAXに対しrealloc(507)へ折り返した。
最初の確保を必ず失敗させ、巨大な実確保・入力読取りなし、既存出力/本文保持を記録した。
red-results.json、red-inputs.zip、入力hash、source/exe/logを保持している。

完成候補で以下を実行し、成功した。

- 固定eng/toolchain.ps1でcmake -S . -B D:/NeNeFolio/outputs/311-size-overflow/build -G Ninja -DCMAKE_BUILD_TYPE=Debug。
- cmake --build D:/NeNeFolio/outputs/311-size-overflow/build --target nenefolio_core folio_tests -j 4（clang-tidyを含む）。
- folio_tests.exe --size-overflow。通常buildはprobe inactive、最大fit要求の3ケースは実行しない。
- 外部証跡compile-probe.pyで、eng/coverage.pyと同じcore/applicationのmalloc/calloc/realloc置換経路を使い、
  folio_size_probe.exe --size-overflowを実行。exit 0、probe active、最大fit 3ケース実行をstdoutで確認。
  測定対象は同selectorだけで、ローカルの全coverage/閾値確認は実行していない。
- 変更C/headerのclang-format --dry-run --Werror、python eng/conformance.py --build-dir D:/NeNeFolio/outputs/311-size-overflow/build、
  git diff --check成功。conformanceは0違反。

対象は最初のoverflow長/SIZE_MAX、最大fit、通常空/Unicode、buffer通常伸長とexact必要量への退避、
失敗後の保持/latch、置換4000000ちょうど/1超過、通常prefix/tailとの合算、同match内/複数matchのcapture累積、
3scopeと既存の拒否優先順位。共有bufferの直接callerである既存Markdown RTF、16MiB保存境界も同selectorで確認した。
UTF変換の拒否はapplicationの変更問い合わせへOOMで伝わり、out印を保持する。
保存入口もOOMとなり、本文writeと履歴archiveの呼出回数は0、保存済本文とEDIT modeは不変だった。

core note_replace_buildの失敗はoutを保持する。一方、application folio_state_apply_replaceは既存公開契約どおり失敗時outをnullptrにする。
呼出元が別名で保持する既存edit自体は変更しない。この2つを分けてテストした。

初回失敗も保持した。formatはsweep引数の改行、tidyはmeasure_matchのネスト4段を検出し、production側を修正した。
追加applicationテストの初回期待はcoreのout保持と混同していた。folio_state.h:322の既存契約と入口のclearを照合し、期待だけを訂正した。
修正後の対象検証が成功するまで完了とは扱わず、ゲート・抑制・既存契約は変更していない。

入力はcandidate-inputs.json、検証コマンド/結果はverification-report.json、独立静読はreview.mdを恒久証跡へ収載する。
最終headの必須CI・統合・通常exe反映・専用作業木整理は、関連PRとcompletion.jsonで別に確定する。

## 範囲と限界

実際の巨大メモリ確保や資源枯渇は行わない。最大の表現可能要求は既存の測定ビルド用allocation probeで拒否して観測する。
製品の正典ビルドではprobeは効かないため、そのケースを明示的に区別する。
外部から宣言された巨大長の先行拒否を測るテストは、小さい実在のnonnull番地を渡すが、その入力は読まれない契約である。
不透明な製品型の内部を偽造しない。容量の内部値は公開せず、公開bytes pointer/length/contentと実装レビューで保持を照合する。

redと測定版のstderrにはASanのinterception_win起動診断が各1行ある。測定版はexit 0でASan/UBSanエラー報告なしだが、完全なinterceptor coverageとは主張しない。
全core確保の監査、置換planの一般入力検証、性能最適化、GUI/OS終了の再検証は今回の範囲外。
schema・依存・ゲート・公開配布の変更なし。Waivers: none。
規則: ARC-001/008/010、C-005/007/008/012、QLT-008/012/013、GIT-001〜004。

