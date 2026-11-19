# 2026-11-20 台帳を書く直前の読み直し — 第7段 実ファイルでの試験（#1085 / #1069 / ADR0117）

## 何をしたか

ADR0117の第7段。第2〜4段で、ノートの台帳（カテゴリごとの`index.json`）を書く道は、書く前に`index.json`とフォルダを読み直すようになった。
その確かめは単体の偽の保存先だけで、偽の保存先は本物と違う点が4つ見つかって直してきた（[第4段の品質記録](2026-11-19-ledger-refresh-pending.md)）。
そこで、本物のファイルで同じことを確かめる試験を足した。次の版の変更で、公開した1.5.0には入らない。

- 新しい試験の実行ファイル `ledger_refresh_tests`（`tests/win32/ledger_refresh_tests.c`、CTestの`ledger_refresh_native`）。窓は作らない。
- 保存先は本物のアダプタ（`persistence_adapter`）。`data/`は試験の実行ファイルの隣（`build/verification-ledger-refresh/data`）で、始めに前の実行の残りごと消す。消す前に、場所がこの試験の出力先であることを確かめる。
- 外の書き手は、Win32で直接`.md`と`index.json`を書く・消す（アプリの状態は生きたまま）。

## 試験の手順（1回の実行で順に当てる）

| 段 | 外でしたこと | アプリの意図 | ディスクの`index.json`で確かめたこと |
| --- | --- | --- | --- |
| 星 | `outside1.md`を足す | alphaに星 | `alpha*/beta/gamma/outside1`。結果は「一覧を読み直した」の知らせ |
| 並べ替え | `index.json`の順を`gamma/alpha/beta/outside1`に書き換え、`outside2.md`を足す | outside1をalphaの場所へ | `gamma/outside1/alpha*/beta/outside2`（外の順と星と外のノートが残り、動かした1つだけが当たる） |
| 初回保存 | `outside3.md`を足す | 無題のノートを「fresh」で保存 | `…/outside2/fresh/outside3`（下の「分かったこと」） |
| 同じ名前 | `taken.md`を足す | 無題のノートを「taken」で保存 | 「名前が使われている」で断り、`taken.md`の中身は変わらない。**takenが一覧に出る**（下の欠陥） |
| ごみ箱 | `outside4.md`を足す | gammaをごみ箱へ | gammaだけが抜け、外のノートはすべて残る |
| 別カテゴリへの移動 | 移動元に`outside5.md`、移動先に`outside6.md`を足す | betaをotherへ | 移動元・移動先の両方で外のノートが残り、betaは移動先の末尾 |
| 断る | `outside2.md`を消す | outside2に星 | 「外で変わったので行わなかった」で断り、`index.json`はbyte単位で変わらない。一覧からoutside2が消える。次の星は通り、outside2の無い台帳を書く |

## 見つけたこと

| 見つけたこと | 対応 |
| --- | --- |
| **製品の欠陥**: 初回保存（と取り込み）で、名前が外で作られたファイルに取られていたとき、「名前が使われている」で断るが、その名前を持っているノートが一覧に出ない。次に台帳を書くまで、利用者には理由のノートが見えない | 断った後に、そのカテゴリを読み直して一覧へ出す（`folio_state.c`の`show_taken`。`create_edited`と`import_body`の2か所から、準備した物を手放した後に呼ぶ）。結果は「名前が使われている」のまま。読み直しに失敗したら一覧はそのまま |
| `index.json`に無い名前（外で足したノートと、このアプリがいま作ったノート）は、フォルダの走査の順（NTFSでは名前の順）で末尾に並ぶ。外のノートがあると、いま保存したノートが最後とは限らない | 第4段の決めごと（台帳は「読み直したファイルとフォルダの突き合わせ」が正本）のとおりなので変えない。ADR0117に書いた |
| 上の`show_taken`で一覧に出したノートは、`index.json`に書かれるまで位置が決まっていない。次の書き込みでは走査の順に並ぶ（メモリの並びは使わない） | 同じ理由で変えない。ADRに書いた |
| 設計席の試験の期待の誤り2つ（並べ替えの置く先は名前で引き直す、を「先頭へ」と書いた。走査の順を「足した順」と書いた） | 試験の期待を直した。製品の誤りではない |
| 偽の保存先と本物の違い | 今回の7段では見つからなかった（単体と同じ振る舞いを実ファイルで確かめた） |

## 変異で確かめたこと

試験が本当に欠陥を捕まえることを、製品のコードをわざと壊して確かめた（作業木で、終わったら戻し、更新日時を今にして構築し直した）。

| 変異 | 結果 |
| --- | --- |
| `refresh_notes`の先頭で何もせずに戻る（読み直しを止める） | 実ファイルの試験が最初の段（星）で落ちた |
| `show_taken`が読み直さない | 実ファイルの試験（「同じ名前」の段）と単体の`verify_refresh_taken_shown`の両方が落ちた |
| 戻した後 | 両方とも通った |

## 実行した検証

作業木 E:/NeNeFolio/worktrees/955-pie（構築は E:/NeNeFolio/outputs/955-pie/build）、構築は`--parallel 4`。

- `ledger_refresh_native`・`folio_unit`・`first_save_native`・`save_as_native`・`favorites_native`・`move_category_native`: すべて成功（CTest、26秒）。
- `python eng/conformance.py --build-dir build`: 違反0。
- 全体のゲート（`eng/check.ps1`）は最終headの必須CIに任せる（QLT-008 / ADR0044）。

## 作業中の事故（記録）

変異を当てるPowerShellの手順で、`[IO.File]::ReadAllText`/`WriteAllText`に相対パスを渡した。.NETはPowerShellの`Set-Location`ではなくプロセスの作業フォルダ（本体のcheckout）で解くので、1回目と2回目の変異は**本体のcheckoutの`src/application/folio_state.c`**に書かれた（作業木には当たらず、変異の試験が「通った」ように見えた）。
本体のcheckoutのgitの差分で、足した4行だけであることを確かめ、その4行だけをbyte単位で取り除いた（`git status`は空に戻った）。本体のcheckoutでは何も構築していない。以後は絶対パスで当てた。
教訓: PowerShellで.NETのファイルAPIを使うときは、絶対パスを渡す。変異の試験が通ったら、まず変異が構築に入ったか（ninjaの出力）を見る。

## 残ること

- 第5段（カテゴリの台帳）と第6段（改名）はまだ読み直さない。外で`categories.json`を書き換えると、カテゴリの操作で上書きしうる。
- 読み直しとファイルの移動の間の隙間、古い`index.json`を後から書き戻す外の道具、改名の記録の凍った台帳は、ADR0117の「守れないまま残ること」のまま。
