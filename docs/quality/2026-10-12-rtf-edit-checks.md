# 2026-10-12 編集本文の RTF 流し込みの確認（#245）

ADR 0006 の「2026-10-12 の補正 2」で、編集の入口（`note_pane_edit`）を平文 UTF-16 の流し込みから閲覧と同じ UTF-8 RTF の
流し込みへ替えた。その製品経路で #234 の実 Win32 probe と #242 の RTF 編集 probe を再測した記録である。
作業木は `feat/245-rtf-edit`（基点 main `d74a626`）。証跡は `D:/NeNeFolio/agents/245-rtf-edit/` に置く。
RichEdit は `msftedit.dll` 10.0.26100.9549。

## 再測 A — #234 の 90 項目（2026-10-12 03:55 JST・exit 0）

- 道具: `run-probe234.ps1 -TaskTree D:/NeNeFolio/worktrees/245-rtf-edit`（#234 の `run-probe.ps1` の写し）。
  作業木の `note_pane.c` を ASan / UBSan 付きで compile し、作業木の build の lib を link する。
- `probe234.c` は #234 の `probe.c` の写しで、UTF-16 の入力を `note_pane_edit` へ直接渡していた所だけを替えた。
  入力は UTF-8 → `note_text` → 製品の `markdown_rtf_plain` を通して流し、application の場面は
  `folio_state_pane_edit_rtf` → `folio_state_edit_rtf` を使う。孤立サロゲートは UTF-8 の本文から読み込めないので、
  空を流した後に `note_pane_replace`（入力の経路）で置いて、既存の拒否に届くことを見る。assertion の数は元と同じ。
- 結果: **`PASS: 90 raw-text/UI/application/persistence assertions`**（`probe234.log`）。改行の形（LF / CRLF / 孤立 CR）・
  VT・U+FFFD・絵文字の位置・検索と置換の位置・番号の帯・Undo 1 段・変更印・確保失敗時の出力保持・実保存と履歴が
  平文の流し込みのときと同じに通る。

## 再測 B — #242 の RTF 編集 probe を製品経路で（2026-10-12 04:00〜04:02 JST・両 exit 0）

- 道具: `run242.ps1`。`probe242.c` の 1 つのソースを 2 つの exe にした。
  - rtf: 作業木の src と build の lib（新しい `note_pane_edit`）＋ core の `markdown_rtf_plain`
  - plain: root main の src と `build/` の lib（旧 `note_pane_edit` の平文経路）＋ `utf16_text_create`
  - 両方とも同梱の Noto Sans JP を `FR_PRIVATE` で登録し、`note_pane_reface` で製品と同じ face を当て、
    文字色はライトの `rtf_palette.text`（`#241318`）で作る。lib が ASan / UBSan 付きの Debug なので probe も同じ flag。
- 往復: 12 ケース（LF・CRLF・孤立 CR・空行・末尾改行・1 字・空・TAB・VT・`\{}`・制御文字と不可視の字・CJK と絵文字と結合文字）。
  #242 で application が断る 3 文字（U+0007・U+2028・U+2029）はケース 11 から外した。
  **EQUAL 12/12**（取り出した UTF-16・変更印 0・読み取り専用でない、のすべてが平文と同じ）。
- 打鍵（`A{字}B` を流して末尾へ `x`）: U+2028 と U+2029 は KEPT / UNDO / MODIFY / 打鍵後の本文が平文と同じ（2/3）。
  **U+0007 だけ違う**: 平文の流し込みは BEL を空白（U+0020）に変え、RTF の流し込みは BEL を保つ。Undo と変更印は同じ。
  この字を含む本文は #242 で application が編集を断るので、製品の経路では起きない。
- 書式（`Format sample\nline 2`）: face `Noto Sans JP`・高さ 220・文字色 `#241318`・CFE_AUTOCOLOR 0・挿入点の書式・
  `note_pane_recolor` 後の色と CFE_AUTOCOLOR の **10 項目すべてが平文と同じ**。colortbl 付きの製品の頭では `\cf1` が
  編集色になり、#242 の probe で見えた CFE_AUTOCOLOR の差は消えた。
- 時間（編集開始＝本文の変換と流し込みの全体。3 回の中央値・ms。取り出した本文はすべて平文と同じ）:

  | 本文 | 平文 | RTF | 比 |
  | --- | ---: | ---: | ---: |
  | ASCII 64 KiB | 314.4 | 76.1 | 0.242 |
  | CJK 64 KiB | 307.3 | 229.1 | 0.746 |
  | ASCII 512 KiB | 2097.2 | 436.5 | 0.208 |
  | CJK 512 KiB | 1726.2 | 1235.1 | 0.716 |
  | ASCII 2 MiB | 8185.0 | 1796.6 | 0.219 |
  | CJK 2 MiB | 10307.2 | 5361.9 | 0.520 |

  ASCII では平文の 1/3 以下。CJK（3 バイトの字・80 字ごとに LF）は速くなるが 1/3 には届かない。
  値は ASan / UBSan 付きの Debug の lib で測ったので、絶対値は製品の実行より大きく出る（比は同じ条件どうし）。

## 結論

- 取り出し・Undo・変更印・書式は平文の流し込みと同じで、編集の振る舞いは変わらない。違いは BEL を保つことだけで、
  その本文は #242 が先に断る。
- 編集開始は ASCII で約 4〜5 倍速い。CJK では 1.3〜1.9 倍にとどまる。
