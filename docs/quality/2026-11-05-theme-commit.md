# テーマ確定後も未保存本文と履歴を保持する — #671

既存theme_preview_nativeへ、明示的なテーマ確定と、その後の別候補の取消を追加した。
通常の短い日本語/絵文字本文を使い、設定は保存されても未保存本文・Undo/Redo・保存履歴は変わらず、
取消先が直近の保存済みテーマになることを確認する。製品の振る舞いを変更する修正ではない。

## 契約と正典経路

基点main `96a907936b76ef6147973f94f383047dc438512e`。根拠は[ADR0061](../adr/0061-theme-live-preview.md)決定4〜6。
`tests/win32/image_status_tests.c`の既存`--theme-preview`を使い、候補行取得だけ同じtest内のhelperへ抽出した。
公開設定INVOKE→テーマ→native検索/候補SELECT→現在世代の候補INVOKE→別候補SELECT→CLOSEを通す。
新しい製品入口・依存・CTest・timeout・data lock・設定schemaは追加していない。

既存の本文保存確認の後、一回Undoで未保存差分を作る。設定確定の前後に全native本文units、選択、
dirty/Undo/Redo可否を比較し、一覧の入力語/選択/focusと保存採用印を確認する。
保存themeの期待値は開始choiceと異なるnord/draculaの明示値。実persistence portでsettingsを読み直し、
themeだけが変わりnumber/minimap/languageが保持されることを確認する。
その後に別候補をpreviewし取消すと、確定直後のtheme/palette/保存settings全bytesへ戻る。
取消後の一回Redo/Undoと全ノート・全保存履歴bytes/store_revision不変も比較する。

## 実行と結果

全実行は`tools/record-run.py`で入力copy/hash、stdout/stderr、子と記録器の終了値を保存した。

| run | コマンド・結果 |
| --- | --- |
| configure-debug-01 | 正典toolchainで専用Debug configure。子/記録0 |
| build-debug-01 | clang-format検査、`cmake --build <dedicated-debug> --target image_status_tests --parallel 2`。142actions、警告error/tidy維持、子/記録0 |
| contract-01 | `image_status_tests.exe --theme-preview`。既存取消/本文保存と今回確定の363checks、1.266745秒、stderr空、子/補助/記録0 |

専用dataは開始時0file。既存30秒上限を変えず、通常app・実ノート・Storeを試験に使わない。
共有候補行helperはこのselector内だけで使うため、無関係なnative selectorや全件suiteをローカルで反復しない。
文書/参照/source/build graphのconformanceと、最終source/CMake/文書SHAの自己レビューを記録する。
最終PR headの必須CIで正典`pwsh -NoProfile -File ./eng/check.ps1`の成功を要求する。

## 途中の補助不備と限界

文書検査helper生成の初稿はPythonのescape警告と生成regex内の改行、二稿は同じraw指定漏れでSyntaxErrorとなった。
製品build/native実行とは別の補助不備で、文書conformanceの実行前にraw行指定へ訂正した。
初稿/二稿は同agentsのprepare-tools-initial.py/prepare-tools-second.pyへ保全し、全体journalへ経緯を残した。
追加抑制・ゲート変更なし。製品のbuild/nativeは初回成功である。

今回の結果は公開accessible actionとnative入力/本文の契約であり、物理クリック/Enter、実UIA reader、
OS外観切替、設定write失敗、再起動、全127テーマを再試験した結果ではない。
新しい性能改善や描画全体の品質を主張しない。製品/API/CMake/schema不変、親単体、追加委任なし。
規則: ARC-001/004/008/011、C-005/008/012/014、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。

原本D:/NeNeFolio/agents/671-theme-commit/。最終CI/正典統合/通常exe-data不変/全保全と専用領域整理は
同taskのintegration-completion.jsonへ。branch/commitを保持し、#634その他の保留領域は触らない。
