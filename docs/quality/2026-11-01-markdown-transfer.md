# Markdownの取り込み・書き出し — #494

- 日付: 2026-11-01
- 規則: ARC-001〜012、C-001〜018、QLT-008/009/012/013、GIT-001〜004
- 設計: [ADR0082](../adr/0082-markdown-transfer.md)、[操作案内](../TRANSFER.md)
- Waivers: none

## 変更と自己レビュー

`folio_state`が複数.mdの追加と現在／カテゴリ／全ノートの書き出しを調整する。
初回保存と取り込みは同じcreate_note・台帳公開／再同期を使い、外部I/Oは`transfer_port`からWin32 adapterへ渡す。
単票は既存copied_text、ZIPは既存persistence読込と純粋なnote_archiveへ合流する。
UIは既存command/menu/modal_owner/入力guard/focus復元を使う。新thread・入れ子pump・runtime依存は追加しない。
ZIPは固定日時・UTF-8名・CRC32のSTOREで、65,534件／128 MiBを公開前に検証する。
外部出力は既存file_bytesの一時ファイル／flush／置換なし公開を使い、本文と台帳の16 MiB制限は維持した。
保存schema・版入力は変更しない。UTF-16入口と操作メニューの拡張をADR0082、形式をFR-041へ記録した。

## 対象検証の記録

証跡は`D:/NeNeFolio/agents/transfer-20261101/`、実行原本は`D:/NeNeFolio/outputs/494-markdown-transfer/`。
通常ノートと通常アプリを試験に使わず、合成fixtureを専用desktopで動かした。最終対象検証は成功した。

- `pwsh -NoProfile -File D:/NeNeFolio/agents/transfer-20261101/build.ps1`のbuild-07成功（正典Debug/tidy）。
- `build/folio_tests.exe --transfer`のunit-04成功。`measurement-04.py`が同じcore/application/testsを正典sanitizer＋確保失敗で構築し、同じselectorを実行したmeasure-run-04も成功。note_archiveは34/34 branches、15/15 functions、143/143 linesを確認。
- `final-check.ps1`のfinal-check-02成功。`eng/conformance.py --build-dir build`と`eng/symbols.py --build-dir build --require core application`は違反0、変更C/headerのclang-format検査と`git diff --check`も成功。final-check-01で不許可のstrrchr依存を検出したため、許可表は変えず検証済みpathの末尾走査へ修正し、unit-04/measure-run-04で確認した。
- `build/folio_tests.exe --menu-surface`と`--unsaved-departure`のboundaries-01成功。変更したメニュー可否と共有create_note公開経路の初回保存・離脱を対象とした。
- Win32のui-02は40 checks / 0 failures。実複数選択、取消、同名、不正UTF-8、未保存単票、カテゴリ／全件ZIP、既存出力拒否、空ZIPと本文・Undo・dirty・focus保持を確認。
- 日本語／英語狭幅／中国語暗色の3画像を目視し、4群11件と新操作名・キー表示が重ならないことを確認。
- 追加した検索中の対象試験はunit-02/measure-run-02で失敗し、非表示カテゴリを存在判定から除いていた境界を修正した。unit-03/measure-run-03と、メニューからOS選択／実出力を行うui-filter-01（9 checks / 0 failures）で確認。最終2条件だけを追加実行し、前段の40checksは関係実装が不変のため再利用した。
- `python -B D:/NeNeFolio/agents/transfer-20261101/audit-files.py`の32checks成功。Python zipfileで3ZIPの名前・内容・CRC・STORE/UTF-8/date、元raw5件・保存済み3件・未保存単票・履歴なし・一時物なしを確認。検索中のZIPも独立読取で3checks成功。原本は`file-audit.json`とui-filter-01の合成出力。
- 初期build-01/02の複雑度超過は処理を整理し、基準を維持した。ui-01の6 failuresは主窓全体を無効とするfixtureの誤前提で、ADR0064に従う内容無効／shell有効をui-02で確認した。
- build-05は作成途中の品質記録への未解決リンクをCNF-006が検出し、コンパイル前に停止した。
- バイト照合の初回はBOM付き入力と保存表現を同一とした試験前提で停止した。ADR0042の署名消費に合わせ、元raw保持と本文／改行保持を別に確認する。

## 限界と統合

物理IME、実reader聴取、別DPI、DWMの実画面呈示は合成Win32試験と区別する。
大量処理は同期のため待ち時間とメモリ使用がある。ZIP64・圧縮・ZIP取り込み・設定／履歴の完全復元は対象外。
最終head必須CI・main・通常exe反映・全保全と終了整理は、完了時に同agentsの`integration-completion.json`へ確定する。
申請済みMSIXを差し替えず、Store認定結果の確認もこの機能追加に含めない。
