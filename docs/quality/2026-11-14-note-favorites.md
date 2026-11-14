# お気に入り — 対象検証（#813）

2026-11-14。対象は星の付け外し、カテゴリ内の安定した優先表示、星だけと検索語のAND。
設計と保存順は[ADR0090](../adr/0090-note-favorites-and-display-order.md)、操作は[キー案内](../KEY_BINDINGS.md#お気に入り)。
規則: ARC-001/003/004/009/010/012、C-002/003/005/012/016/018、FR-008/009/015/032/044、QLT-008/009/012/013。

## 入力と環境

作業元はmain `79af98d`。D:/NeNeFolio/worktrees/813-note-favoritesで実装し、
固定toolchain（eng/tool-versions.json）とDebugのASan/UBSan/nullabilityを使用した。
原本はD:/NeNeFolio/outputs/813-note-favorites/の各run。tools/record-run.pyが入力snapshot・hash・
raw stdout/stderr・child/wrapperの終了値・指定exeを保存する。統合後の保全先は
D:/NeNeFolio/agents/favorites-20261114/から辿る。実ノートは試験に使っていない。

## 対象検証

| コマンド・記録 | 結果と範囲 |
| --- | --- |
| `cmake --build build --target folio_tests nenefolio favorites_native_tests settings_accessibility_tests` | 固定clang-cl/tidyで構築。新しい対象にも同じicon resourceを結線 |
| `build/folio_tests.exe --favorites`（unit-05） | 成功。旧/新版台帳、異常系、安定順と逆引き、設定版6と旧版読込、表示/鍵、検索とのAND、保存/移動/戻し失敗、改名journalと星の比較、コマンドと3言語文言 |
| `ctest --test-dir build -R '^favorites_native$' --output-on-failure`（native-07） | 成功。専用desktop/dataの実RichEditで、星クリック・capture取消・UIA・メニュー、星だけ/優先、0件時の文書と読み上げ名、本文/選択/dirty/Undo/Redoとmd/履歴不変、再起動の星/優先保持とscope初期化 |
| `ctest --test-dir build -R '^(favorites_native\|settings_accessibility)$' --output-on-failure`（native-03のsettings_accessibility） | 設定面の対象は成功。追加項目の表示/探索と既存操作。favorites側の同run失敗は後続runで修正・確認 |
| `pwsh -NoProfile -File D:/NeNeFolio/outputs/813-note-favorites/oom.ps1`（oom-02） | 成功。eng/coverage.pyの測定buildを再利用し`--favorites-oom`だけ実行。版2の読込・星変更・改名・挿入・照合の全確保を順に失敗させる。applicationの星変更は8失敗点と各再試行で、失敗時の書込なし・本文保持を確認 |

最終headの`pwsh -NoProfile -File ./eng/check.ps1`はPR必須CIを正本とする。ローカルでフルゲートは反復していない。
変更のない対象結果はpush/レビュー/統合で再利用する。CIの成功・統合・通常exe反映は外部の確定記録に残す。

## 見つけた問題と修正

- 最初の構築で台帳factoryのnullable束縛と未初期化フラグ、関数複雑度、native resourceの対象漏れを検出し修正。
- 星保存の成功値を失敗専用変換へ渡していた箇所を単体/実窓で検出。保存成功後にだけ台帳を採用する形へ修正。
- 旧版の出力文字列と項目数の期待を版2/追加3コマンドへ更新。UIAテストはlayoutの改訂ではなく公開treeから行の識別を取得する。
- 改名planの比較に星の比較を加え、名前が同じでも星が異なる意図を同一扱いしない反例を追加。
- 独立レビューで星だけ表示の読み上げ名不足を確認し、DRAWER/TREEのNameへ同じ状態文言を投影。0件時と解除後も対象試験へ追加。
- 読み取り専用の独立レビューは修正を確認し、台帳/設定・改名比較・移動の保存と同期・表示順・検索条件・操作対象について追加の具体的欠陥なしで終了。
- native-01はtoolchain環境なしでctestを起動できず未実行。native-02の星操作は保存成功の誤判定で失敗通知を開きtimeout。記録は削除せず後続の修正と区別する。

## 互換性と限界

台帳は版2、設定は版6を書く。旧台帳版1・設定版1〜5を読み、起動だけでは移行しない。
変更後のJSONは旧アプリの対応外。Markdown/ZIPに星は含めない。タグ・検索プレビュー・版上げ・配布は対象外。
星を含む移動先台帳→md→移動元台帳の順で途中終了時の星を保持し、不要な名前は従来の走査照合で除く。
実電断・外部からの同時変更・Store署名版の試験ではない。スクリーンリーダー製品での聴取や全テーマ/DPIの目視は未実施。
追加waiver none。既存WVR-0001（WinHTTP callback ABI）は本変更と無関係に継続。
