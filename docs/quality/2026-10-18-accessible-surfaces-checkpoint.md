# #321 補助面のUIA・実reader対象確認

Issue: [#321](https://github.com/hideyukiMORI/nene-folio/issues/321)、親#312。設計: ADR0055 C/0056/0058。
規則: ARC-001/002/004/008/010/011、C-002/007/012/018、QLT-008/010/012/013、GIT-001〜004。
最終head必須CI・main/通常exe反映・整理の確定結果は関連PRと
`D:/NeNeFolio/agents/321-accessible-surfaces/completion.json`へ収載する。

## 変更と正典経路

既存`accessibility_host`/`accessibility_node`へcommand layerのfragment root、設定のRadio/Toggle、
候補のSelection/Invoke/Scroll、ヘルプ、結果のlive regionを追加した。
`folio_window`の既存command_surface/selection・描画配置・共通操作を使い、別の選択/保存経路は作らない。
開閉/差替え/候補変更で世代を進め、古い子参照を失効させる。modal中は背後のUIA操作とnative入力を拒否する。
native EDIT/RichEditのValue/Textを維持し、`accessibility_native`の直接注釈で補助面入力と名前入力/保存先カテゴリへ既存翻訳の名前を付けた。
通知は既存結果・未保存・完了状態の変化から発行し、同じ文字列を定期再送しない。
保存schema・外部runtime・ゲート閾値/除外の変更なし。Waivers: none。

## 対象検証

作業木は`D:/NeNeFolio/worktrees/321-accessible-surfaces`、恒久証跡は`D:/NeNeFolio/agents/321-accessible-surfaces/`。
正典Debug/Ninja・clang-cl・ASan/UBSan/nullabilityを使用。native fixtureは製品sourceを直接includeし、正典buildのライブラリと結ぶ外部試験。
製品へ試験hookを追加せず、専用exe隣の合成dataだけを操作した。

| コマンド/観測 | 結果 |
| --- | --- |
| `cmake --build build --target nenefolio` | build-04成功、変更したproductionのclang-tidyを含む |
| `cmake --build build --target folio_tests` / `build/folio_tests.exe --accessibility` | unit-accessibility-02成功。追加文言IDに合わせ期待値表の終端を修正 |
| `pwsh -NoProfile -File D:/NeNeFolio/agents/321-accessible-surfaces/compile-native.ps1` | native-build-06成功。外部fixtureの未使用/nullability警告はログに分離 |
| `native-contract.exe`（専用desktop） | native-run-05: **226 checks / 0 failures**。provider reads 32661、wrong thread 0 |
| `native-contract.exe structure`（専用desktop） | native-structure-01: **3 checks / 0 failures**。StructureChanged中の窓破棄 |
| `native-contract.exe long`（専用desktop） | native-long-01: **18 checks / 0 failures**。長い履歴候補を3言語で確認 |
| `native-contract.exe reader` / `reader-extra` | 実NVDAで10件/7件、失敗0。各段階の試験窓foreground=1 |
| `native-contract.exe reader-help` | NVDAのオブジェクト移動でヘルプ本文の発話を確認（reader-speech-05.txt） |
| `python eng/conformance.py --build-dir build` / 最終build / `git diff --check` | CMake File API情報を生成後、0 violations。build-finalは追加作業なし、空白検査成功 |

通常のnative確認は専用desktopで行い、前後のforeground HWNDは527712のまま、SwitchDesktopなし。
別MTA threadの実UIA clientから取得し、providerの読取がUI所有threadで行われることを測った。
226件は3言語・640×480での補助面名/標準Value、設定値、0件、スクロール、IME中の拒否、背後Save/F6拒否、
close/reopen/query後の古い参照、列挙/focus時の本文/Undo/変更印/書込み回数の不変、既存置換Undo、履歴Undo、
保存失敗/名前入力取消とラベル、変化のない通知の重複なしを含む。
独立レビューの初回4件（初期通知/構造通知中の寿命、補助面focus境界）を修正し、対応するnative確認を追加。
最終差分の読み取りレビューでは追加blocker/highなし。実装・検証は現在席が単体で行った。

## 実reader

NVDA **2026.2 AMD64 portable**、UI言語en、oneCore synth。公式署名済みsetupのSHA256:
`F3F8D29974A88D687B3C4809BE192219EC579C5BDABCDA5AAF53635288BCA824`。
`--minimal --disable-addons --no-sr-flag`と専用config/logを使い、試験後`--quit`で終了。
試験窓の前面化とNVDAのヘルプ移動キーだけComputer Useを使用し、製品操作は専用C fixtureが既存keyboard/UIA入口を呼んだ。
OS版・言語・構成は`reader-result.json`。実NVDAのInput/output logから試験区間の発話だけを
`reader-speech-01.txt`/`02.txt`/`05.txt`へ抽出した。UIAの値を発話に見立てた記録ではない。
ヘルプは専用configだけをlaptop配列にし、候補へ実focusを置いて親/次要素へ移動した。
03/04の全窓読上げ/native入力からの移動ではヘルプへ到達せず、05で本文の発話を確認した。

| 対象 | 実発話の例 |
| --- | --- |
| 設定 | `Settings dialog`、`Choose a theme… button Follow the OS`、`Show line numbers check box not checked`→`checked` |
| 候補 | `Search theme names edit`、`Light`、`Command palette`、`Save`、`Restore from history`、`1 # Alpha` |
| ヘルプ | `List ↑↓ Select / Enter Run / Esc Back / Tab Keys` |
| 検索 | `Find in this note edit`、`1 / 2`、`Not found` |
| 置換 | `Search pattern edit`、`Replacement text edit Reader`、`Alpha / 2`→`Alpha / 1` |
| 未保存/失敗 | `There are unsaved changes…`、native dialogの`Could not write the note back…`と`OK button` |
| 名前入力 | `Save as dialog`、`Note name`、`Destination category`、`Note name edit` |
| 完了 | `Removed from the list.` |

reader生成の発話テキストの観測であり、音声波形/人の聴取受理は主張しない。
reader接続時のfixture stderrにはASanの`interception_win: unhandled instruction`診断1行が各回出た。
手順とsummaryは完了し、ASan/UBSanのメモリ違反レポートはなかった。診断も証跡へ収載した。
物理IME入力・全OS/DPI/reader・一般的なWCAG適合は未測定。IMEは既存composition入口で確認した。

## 残っていた作業物の整理

旧C側41作業木とD側210は、全42件のheadがmerged PRと一致し、main取込・変更なし・リンク/稼働参照なしを確認。
必要なログ/source/小さな合成data等455ファイルを42 ZIPへ収載し、各ファイルのSHA256照合後に
`git worktree remove`で削除した。枝とcommitは保持。詳細はlegacy-artifact-manifest.jsonとlegacy-cleanup-result.json。
#319の残留DLL2個は既存Chromeが参照中のため保持。#320仮出力の削除は再確認後も自動承認審査が
`blocked by policy`で拒否し、詳細理由は返らなかった。別手段で回避せず保持した。
最後の参照確認・#321自身の整理結果はcompletion.jsonと引き継ぎへ記録する。
