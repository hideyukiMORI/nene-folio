# 2026-11-19 ガントの`weekday`と`weekend`（#1030 / ADR0115）

## 何をしたか

ADR0115で未対応にしていた指示語のうち2つを読めるようにした。次の版で、1.5.0の候補には入らない。

- `weekday monday`〜`sunday`: 週の目盛りを置く曜日。書かなければ従来どおり月曜（Mermaidの既定は日曜で、この違いは従来から記録済み）。自動で週の目盛りが選ばれたときにも効く。
- `weekend friday` / `weekend saturday`: `excludes weekends`の週末を金・土にするか、土・日（既定）にするか。

どちらも大文字小文字を区別しない（Mermaidのガントの文法は大文字小文字を区別しない）。曜日名でない語、`weekend`の金・土以外の曜日、語の無い行は不正にした（Mermaidは構文の誤りになる）。

## 試したことと結果

単体（`diagram_tests`）は1回目で通った。確かめたこと:
- `Weekday Sunday`で、月曜に始まる3週の図の目盛りが日曜（12/27、1/3、1/10）になる。
- `weekday monday`と`tickInterval 1week`の組み合わせ。
- `weekend friday`で、木曜に始まる`2d`が金・土を飛ばして日曜まで（4日の棒）になり、次の作業が月曜から始まる。
- `weekend Saturday`は既定と同じ。
- 断る形: `weekend sunday`、語の無い`weekday`、`weekday someday`。

曜日名の表は`excludes`の曜日名（#1022）と同じ関数（`day_named`）を使う。指示語の関数が複雑さの上限に近いので、2つの指示語は別の関数（`week_directive`）に分けた。

Mermaidの意味（`weekday`の既定が日曜、`weekend friday`が金・土）は設計席の記憶による。今回は独立レビューを頼んでいない（変更が小さく、数え方は#1022のレビューで確かめた道をそのまま通る）。

## 検証

`verify.ps1 -Tag w1030`: 書式・差分・構築・適合・シンボル成功、CTest 70件すべて成功（112秒）。測定は分岐90.95%（閾値90%）。通常exeへは反映しない。

Waivers: none。
