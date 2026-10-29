# 表開始とsetextの優先順位 — 2026-10-29

Issue: [#426](https://github.com/hideyukiMORI/nene-folio/issues/426)。基点main `1cae763146080d886caa1603b876ec4c28e4dd95`。
規則: ARC-001/003/008/012、C-012/014/016、QLT-008/012、GIT-001〜004。Waivers: none。
[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)と[ADR0071](../adr/0071-ordered-markdown-containers.md)の既存契約を修復する。

## 問題と最小修正

表開始の`markdown_table_starts`はheaderのraw pipeを要求し、escapeを考慮する既存cell分割と条件が食い違っていた。
`a\|b`の次行が`---`なら1列表になり、逆にpipeのない`head`の次行が`|---|`でも表にならなかった。

同関数でheaderのblank/ATX/thematic/fenceとdelimiterのsetextを既存line分類器で拒否し、raw pipe走査を削除した。
残る判定は既存の列数一致とdelimiter妥当性だけ。block先読みとcontainerのlazy継続中断はこの同じclassifierを使う。
block側だけ順序を変える第二の条件、headerのsetext一律拒否、新しいparser/型/owner/API/確保/依存/保存schemaは加えていない。

これによりescaped pipeや外側pipe付きheaderと`---`は既存setextへ渡り、pipeなしheaderと`|---|`/`:---:`は1列表になる。
既存spanへのcurrentline setext、container strict match、escape偶奇、delimiter grammar、native上限/literal表示は同じ経路を保つ。
利用者向け[MARKDOWN](../MARKDOWN.md)に1列表とsetext、先行blockの境界を2行追記した。新ADRやwaiverは不要。

## 独立redと完成fixture

新しい`tools/record-run.py`で旧source用Debug configure/buildを記録し、次の2件を独立processで実行した。
両方child exit 1 / wrapper exit 1、recordingErrors 0、全入力の前後hash一致。先のassert失敗で後の例を未実行にしていない。

| run | 旧sourceの実値と期待 |
| --- | --- |
| `red-setext` / `--table-start-setext` | 実値は`\trowd`・1cellの`a|b`、期待は同本文のh2全RTF。`rtf body`で失敗 |
| `red-bare` / `--table-start-bare` | 実値は通常段落`head |---|`、期待はtable開始。`syntax fragment and forbidden interpretation`で失敗 |

完成した`run_markdown_table_start_tests`は既存全体Markdown entryと`--table-start-priority`が共用する。
同じ2関数を独立redにも用い、既存`convert_syntax`/`expect_body`/`expect_fragment`で公開RTFを確認する。

- escaped/outer pipe header、単一/複数dash、h1/h2、複数spanへのsetext。
- pipe省略・colon付きの1列表、backslash 1/2/3本、通常複数列、不正delimiter、列数不一致。
- blank/ATX/fence/thematicの優先、fence本文と後続通常段落への復帰。
- `===`の単独table headerと既存paragraphへのh1を区別し、headerの過剰拒否を防ぐ。
- quote/list内のh2と1列表、prefix欠落時のstrict不一致、lazy本文からroot表へ離れる境界。
- `> quote\nlazy\|text\n---`はquote本文とroot thematic。内側h2やroot表へ変えない。

最後の例の期待は実行前に訂正した。初稿はthematic直前の`flush(true)`を想定してquote段落へ`\sa120`を付けていたが、
実際の経路はcontainer変更の`FLUSH_REQUIRED`により`accept_line`内の`flush(false)`が先に段落を出す。
後の`flush(true)`はspan0で何も出さないため`\sa120`は付かない。親の指摘をsourceで再確認し、red/green前に訂正した。
実測出力を期待へ取り込んだものではなく、quote本文・所属・root ruleの期待は維持した。

## 対象検証と記録

| 対象 | 実行・結果の根拠 |
| --- | --- |
| red configure/build/tidy | `cmake -S <worktree> -B <outputs>/build-red -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build <outputs>/build-red --target folio_tests --parallel 4`。各child/wrapper 0 |
| green configure/build/tidy | 別の`build-green`で同じ正典Debug target。各child/wrapper 0。旧exeを上書きしない |
| green selector | `build-green/folio_tests.exe --table-start-priority`、child/wrapper 0。`Markdown table-start priority and container tests passed`。stderrなし |
| format / File API conformance / diff | 同じ完成source/testに対する最終runのargv、raw log、child/wrapper exitは恒久agentsの`results.json`と`records/`を正とする |

compile/tidyはCMakeに固定されたclang-cl/clang-tidy 19.1.5。Debugは既存ASan/UBSan/nullabilityと非回復設定を維持する。
版固定は`eng/tool-versions.json`（CMake 3.31.6-msvc6、Ninja 1.12.1、Python 3.12.10、MSVC toolset 14.44.35207）。
Pythonは`-B`または正典toolchainの`PYTHONDONTWRITEBYTECODE=1`を使った。

green build呼出しは`eng/toolchain.ps1`の版確認中に長く待った。13:14:48 JSTに自身pwsh PID11752のchildが
`llvm-readobj.exe --version` PID37908で、まだrecord-run directoryが無いことを照合した。
同じ呼出しを待ち、13:14:53 JSTにrun-dirを作成してbuildへ進んだ。外部processへ干渉せず、二重起動もしていない。
この前段はwrapperのrun開始時刻に含まれない。観測・親子PID・起動commandは`preflight-observations.md`へ分離して保全する。

証跡の正本は`D:/NeNeFolio/agents/426-table-start-priority/`の`completion.json`、`commands.json`、`results.json`、
`hashes.json`、`self-review.md`と各recordのinput/artifact snapshot・raw stdout/stderr・終了値。
実行時の原本は`D:/NeNeFolio/outputs/426-table-start-priority/`。初回redや成功のsnapshotを後のsourceへ付け替えない。
読取時に推定filenameが不在だったものは実在pathを列挙して訂正し、製品/testの失敗とは分離した。

## 再利用と残る範囲

新しいallocation経路は無いため追加OOM、通常UI/native/倍率/Undo、検索、性能計時、local full gateは実施しない。
既存native表の描画/配置と上限退避の検証を再利用する。CommonMark/GFM完全互換の追加や未対応blockへの拡張は行わない。
最終PR headの必須CI、commit/push/PR/merge、通常反映は親担当。

作業木・red/green build/exeと全試行は未統合のため保持する。親が統合・通常反映・archive全hash照合後、
dirty/untracked/ignored・唯一成果物・link・稼働参照を監査して新規作業物を整理する。branch/commitは保持する。
通常data/Nibと旧削除拒否34pathへ操作しておらず、cleanupを試していない。
