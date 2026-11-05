# 一括取り込みの停止と再試行 — #626

同名スキップを挟む7path・4新規ノートについて、読込/本文公開/台帳保存の各1〜4位置で拒否し、
検索corpusがcold/warmの計24caseを確認した。拒否解除後は公開済みのprefixをスキップして残りを一度だけ追加した。
製品の欠陥は見つからず、変更は`tests/unit/transfer_tests.c`と関連文書だけである。

## 契約と正典

基点はmain19857d88341e680de72ed82757fbaa8c7310f26f（#624統合後）。
`folio_state_import_notes` → `import_file` → `import_body` → `publish_created` → `synchronize_index` / `record_import`を使う。
[ADR0082](../adr/0082-markdown-transfer.md)の決定3どおり、既存名はスキップし、失敗位置から先を未処理として残す。
本文公開後の台帳拒否は、追加済みを巻き戻さずLEDGER_STALEとし、修復前の次のbatchはLEDGER_UNSYNCEDで拒否する。
取り込みは現在の本文・選択・編集mode・既存本文の保存/履歴を変えない。

規則: ARC-001/003/004/008/010/012、C-004/005/008/012/016、QLT-008/009/010/012/013、GIT-001〜004。
公開API/状態所有/製品source/schema/版/依存/gateに変更なし。追加waiver none、既存WVR-0001は変更しない。
新ADRや別の製品経路は追加しない。主担当が単体で設計・実装・検証し、この限定したtest追加に新たな独立reviewは付けない。

## 入力と期待値

順序は`one.md / alpha.md / ALPHA.MD / beta.md / gamma.md / ONE.md / delta.md`。
oneは既存、alpha/ALPHAは同batch内で重複し、ONEも既存名の大小文字違い。新規はalpha/beta/gamma/deltaの4件。
既存fakeのread/create/ledgerへ指定callだけ拒否するcounterを加え、既存の「全call拒否」は指定0で維持した。
readはMALFORMED、create/ledgerはUNWRITABLEを使い、本文公開前後の差を確認する。

| 新規位置 | 読込/公開拒否: added / skipped / remaining | 台帳拒否: added / skipped / remaining |
| --- | --- | --- |
| 1: alpha | 0 / 1 / 6 | 1 / 1 / 5 |
| 2: beta | 1 / 2 / 4 | 2 / 2 / 3 |
| 3: gamma | 2 / 2 / 3 | 3 / 2 / 2 |
| 4: delta | 3 / 3 / 1 | 4 / 3 / 0 |

台帳拒否の後は拒否を続けた再呼出しも行い、added/skipped0・remaining7・新規read/create0を確認する。
解除後は既存prefixを含む同じ7pathを再試行し、read/createが残りの新規件数と一致し、総数が初期値+4となることを確認する。
検索ではimportedに4件一致し、現在のoneの原文bytes・タイトル・編集mode・既存本文publication0を保つ。

cold検索の読戻しを正しく表すため、このfixtureの間だけ成功したcreateの名前/本文を4件の固定領域へ保持し、
同じAカテゴリのread portへ返す。実ファイルや製品のcacheは作らない。保持は各caseで初期化し、終了後は無効化する。
名前32byte/本文64byteはこの固定fixtureの容量で、製品上限ではない。上限超えはtest assertionで止める。
warmでは開始前に検索してから絞込みを解除し、既存の派生corpus更新経路も同じbatchで確認する。

## 試行と検証結果

configure-debug-01とbuild-debug-01の119actionsは成功した。
transfer-01は新fixtureが初期ノート数を3と決め打ちし、最初のprefix数assertionで子/記録1となった。失敗原本を保持する。
共通fakeはscanでA/B/Cの3カテゴリ・各3ノートへ整合させるため、総数は台帳文面の3ではない。
これは製品の不具合再現ではなく試験条件の誤りだった。実際の開始総数を保持し、増分4と公開prefixを確認する形へ直した。
静読でcoldのread fakeが保存本文を保持しないことも確認したため、上の固定4件の保持をtest内へ追加した。
整形後のpatch context不一致が一度あったが、適用されていないことを確認して正確な行へ適用した。

build-debug-02は変更したtest TUとlinkの2actionsだけ再構築して成功。正規format/tidy/警告error、ASan/UBSan/nullabilityとrecover禁止を維持する。
transfer-02は既存`folio_tests.exe --transfer`で子/記録0、記録error0、全input終了時不変。
このselectorは既存transfer/command/UI textも実行する。stderrのactual/expected行は`command_tests.c`が
key-guide内の候補探索に`same_text`を使う既存の比較出力であり、`require`のFAILやsanitizer診断ではない。
rawはそのまま保持し、空だったとは記録しない。この無関係な表示を本taskで修正したり、全件再試行したりしない。
canonical Debugの確保注入はinactiveであり、このrunを全OOM点の成功とは扱わない。既存の計装は最終必須CIで維持する。

対象fixtureは既存`run_transfer_tests`へ接続し、別selectorや製品のtest窓口を増やさない。
文書差分/参照/規約確認と最終head必須CIのeng/check.ps1を経て、正典mergeへ進める。
製品未変更なので#624の通常exeと成功済み製品検証を再利用し、再build/起動を行わない。

## 原本と限界

恒久原本は`D:/NeNeFolio/agents/626-import-batch-retry/`、全体journalは`D:/NeNeFolio/agents/quality-20261105/journal.md`。
source/test/exe/config/input/raw/終了値はrecord-run.pyで保存する。文書検査・CI・統合・通常exe/data不変・全保全/専用領域整理は
同taskのintegration-completion.jsonと実runで確定する。branch/commitは保持する。

fake portによるapplication契約の検証であり、実Win32 I/O・電源断・複数fileの原子性・UI Undo/dirtyを証明しない。
実ノート・通常app・Storeを試験に使わない。既存の移動/履歴/write途中失敗の非原子性を解消した変更ではない。

## 統合・不変照合・整理の確定

PR627最終head8163e882の必須CI41942785824 full-gate/check成功後、main0e879fbへ正典統合しIssue626 CLOSED。
#624の通常exeとdata32fileは全bytes/hash/作成更新日時不変。再build/通常app起動なし。
1,322fileをZIPへ全保全/hash照合し、取込tree/clean/ignored/リンク/稼働参照を確認後に626専用worktree/outputを削除した。
branch/commit保持。確定原本はagents/626-import-batch-retry/integration-completion.json。
