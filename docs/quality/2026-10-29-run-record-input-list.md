# 記録入力一覧の実装 — 2026-10-29

Status: 対象tool検証と最終static conformance審査済み。 Governing rules: ARC-001, ARC-012, QLT-008, QLT-012, QLT-013.

Issue: #432。受理設計は `D:/NeNeFolio/agents/432-run-record-input-list/design-decision.md`、
実装前の読取レビューは `D:/NeNeFolio/agents/quality-20261029/run-record-arguments-review.md`。
基点main: `e2da1abe6a31c7c8dac9f79d3d3bd1ec80c677ad`。Waivers: none.

## 変更した挙動

`tools/record-run.py` は単一の `--input-list ABS_JSON` を受け取る。UTF-8（BOM可）のJSON文字列配列を読み、
一覧file自身を最初のinputとして追加し、その後へ直接指定の `--input`、一覧の要素を順番に続ける。
一覧optionの重複、一覧の相対/NUL path、不正UTF-8/JSON、配列以外、文字列以外、一覧要素の相対/NUL pathは
子起動・run-dir作成より前に拒否する。子argvの `--` 以降は従来どおり不解釈である。

JSON parseに使った元bytesを `captured_bytes` として保持し、先頭inputの既存snapshotへ渡す。
snapshotはそのbytesを `io.BytesIO` から共通chunk-copy/hash loopへ流し、snapshot後に一覧fileの現bytesを
既存経路でdigest比較する。変化・消失・読込失敗はBのsnapshotを残した記録失敗とし、子は起動しない。
子の実行後も既存 `compare_inputs` が同じsnapshot hashと現fileを照合する。一般のpath validationに失敗した場合は
従来どおりrun-dirを作らず、Bの保存を約束しない。

run/result schema、snapshot名、child argv、artifactの意味、既存の直接 `--input` duplicate許容は変更しない。
外側の長いinput argvだけを短くする。子argv自身のOS長さ制限、fileの原子的固定、未指定依存の検出は対象外。

## 変更ファイル

- `tools/record-run.py`: input-list parse/resolve、captured bytesを既存snapshot経路へ接続。
- `tests/conformance/test_run_record.py`: 長い展開列、format/validation、B保全、子変更と既存run拒否の境界を確認。
- `docs/quality/2026-10-29-run-record-input-list.md`: 本記録。

## 検証

親の静読後、既存10件と新規境界を一回のdiscoveryで実行した。

```powershell
$env:NENEFOLIO_RUN_RECORD_TEST_ROOT = 'D:/NeNeFolio/outputs/432-run-record-input-list/tests'
$env:PYTHONDONTWRITEBYTECODE = '1'
python -B -m unittest discover -s tests/conformance -p test_run_record.py -v
```

実際のdiscoveryは専用test rootを設定し、変更した `tools/record-run.py` で記録した。
記録runは `D:/NeNeFolio/outputs/432-run-record-input-list/runs/test-20261029T154718JST/`。
結果はchild exit 0、recording wrapper exit 0、recordingErrors 0、15 tests / 10.731s / `OK`。
入力は一覧自身と6ファイルの計7件で、実行中のsource hashはすべてunchanged。
test outputは `stderr.log` にraw保存（2,940 bytes、verbose結果と `OK`）、`stdout.log` は0 bytes。
`run.json` / `result.json` / 0001–0007の入力snapshotを保持した。

実行前に一度、PowerShellのpath array作成式が6 pathを1つのstringへ結合した不正JSONを使い、
外側record-runがprepare時に125で拒否した。この試行はrun-dir/childなしで、malformed payloadと
実出力の取得制約を別記録へ残した。失敗試行は有効なsuite実行ではなく、suiteは上記の一回だけ実行した。

最終staticでは `eng/conformance.py` の `inventory(root)` が実際に列挙する740ファイルの絶対pathを
input-listに渡し、次を一度実行した。

```powershell
python -B tools/record-run.py --run-dir D:/NeNeFolio/outputs/432-run-record-input-list/runs/static-20261029T160000JST --cwd D:/NeNeFolio/worktrees/432-run-record-input-list --input-list D:/NeNeFolio/outputs/432-run-record-input-list/static-inputs.json -- python -B eng/conformance.py --root D:/NeNeFolio/worktrees/432-run-record-input-list
```

child exit 0、wrapper exit 0、recordingErrors 0、741 inputs（input-list自身を含む）、全source unchanged。
外側commandのstdout/stderrは別ファイルへredirectして保存し、両方0 bytes、外側exit 0。
その後 `git diff --check` も一度実行し、exit 0、stdout/stderrは各0 bytes。
suite実行は追加していない。これらは全品質ゲート `eng/check.ps1` の代替ではなく、最終PR headのCI結果は別途確認する。

製品C/unit/native/OOM/性能計時/通常exe buildは対象外。記録runには子discoveryのraw stdout/stderrと
child/wrapper終了値、入力source hashを保持した。最初の不正入力試行では外側record-runのstdout/stderrを
別々にraw保存していない。wrapperの観測テキストはcombined tool returnであり、streamの帰属は
`D:/NeNeFolio/agents/432-run-record-input-list/launcher-attempt-01-correction.json`に訂正記録した。
元の`launcher-attempt-01.json`は保持している。

## 残る制約

snapshot後から子起動までの外部改変、観測間に変更後復元されたfile、未指定依存、長い子argv、artifact列のOS上限は保証しない。
一覧snapshotは解析bytesのhashと同一であることを記録し、snapshot時点のsourceを照合するが、一覧pathの選択内容が十分であることは認定しない。
