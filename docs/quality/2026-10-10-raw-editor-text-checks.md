# #234 — 本文取得でU+FFFDと縦タブを保つ

2026-10-10 JST。規則: ARC-001 / ARC-004 / ARC-009 / C-005 / C-014 / QLT-013。
Waivers: none。実装と検証はNeNeFolioサナが単体で実施。通常データは使用していない。

## 再現と変更

base `09a198f` で、編集に流したU+FFFDは保存用 `EM_STREAMOUT` と検索用 `GT_DEFAULT` の
取得時に空白へ変わり、U+000Bはstream-out時にCRLFへ変わった。内部のraw UTF-16には残っていた。
保存・問い合わせ・検索・置換を `note_pane_text` の `GT_RAWTEXT` / codepage 1200へ揃え、
重複していた公開関数・stream callback・used/overflowedを削除した。番号専用bufferは維持する。
改行は既存coreで正規化する。ADR 0006 / 0023 / 0026 / 0028を先に補正した。

## 対象測定

再実行する入口は `D:/NeNeFolio/agents/234-raw-editor-text/run-probe.ps1`、sourceは同所 `probe.c`。
製品のnote_paneをそのままコンパイルし、測定ビルドだけreallocを差し替える。製品に注入窓口は追加しない。
実Win32 controlは非表示の親に作り、実application・persistence・ICUと接続する。
dataは測定exeの隣のD領域に毎回新しく作る。

- 修正後90 assertion成功（`green-probe.log` / `green-probe-result.json`、exit 0）。
- 同一probeをbaseへ向けるとU+FFFDの保持で失敗（`red-probe.log`、assertion 5、exit 1）。
- 空、U+FFFD、U+000B、LF/CRLF/CR、末尾改行、絵文字、日本語、tabを確認。
  孤立した上位/下位surrogateは変換されず、既存UTF-16拒否へ届く。
- 検索位置は絵文字の後でも正しく、置換の下見と実適用が同じUTF-16を比較する。
  取得と行番号読取で本文・借用buffer・選択・Undo・変更印を保持する。
  縦タブは論理段落を増やさず、末尾CRの空行は数える。
- 初回/拡張realloc失敗で出力引数と既存確保領域を保持し、次の取得で回復する。
- LFとCRLFの実ファイルで変更問い合わせはSAME、no-op保存は履歴0。
  U+FFFDを含むICU置換後の保存は、対象文字と元の改行をbyte単位で保持して再読込できる。
  履歴は各1版だけ増え、変更前のbyte列と一致する。編集終了のno-opも成功する。

`python eng/conformance.py` は0 violation。製品3ライブラリの対象buildは成功。
正典ゲートの最終結果は同所 `gate-result.json` / `fullgate.log`、統合と通常exe反映・整理は
`completion.json` に保存する。共通の本文取得が複数UI操作の入口を変えるため、Ready前の正典ゲートを1回行う。
成功済みの対象測定は、ソースが変わらなければpush/mergeで再実行しない。

00:25:30 JST、source `34e32d9` の `pwsh -NoProfile -File ./eng/check.ps1` はexit 0。
製品79 build step、CTest 2/2、分岐3246/3462（93.76083188908146%）、
低網羅7.68%の拒否、実道具proof19件が成功した。4件のJSONは作業木外の `gate-evidence/` へSHA256照合して保存。
以後は結果を記録する文書だけを更新し、検証済みのソース差分は無い。

## 範囲と残る条件

- 物理IME・高DPI・全画面操作は今回の取得変更の検証として実行していない。
- WindowsのRichEdit実装への依存は残る。`cb` の既存余裕確保とcodepage 1200は維持する。
- 測定exeのASan起動時に `interception_win: unhandled instruction` が出た。
  上記90項目と終了コードは成功しているが、警告が無い実行とは報告しない。
- U+0007・U+2028/U+2029は流し込み時に変わる別の境界。今回の取得修正では対応していない。
- RTFの `\\u-3?` と生のU+000Bは閲覧への流し込み時に失われる。
  baseの独立測定 `view-boundary.c` / `view-baseline.log` でも `A` / `B` だけがraw本文に残った。
  これは取得前に起きる別件として分離し、今回の編集保存の修正範囲へ混ぜない。
- 保存schema・依存・ゲート閾値・除外は変更していない。

後続#236の補足: 単独VTはRTFで保持された。上記の欠落はU+FFFDのescapeに隣接したVTで観測したもの。
入力境界の追加測定と修正は [UTF-8 RTFの確認記録](2026-10-10-utf8-rtf-checks.md) を参照する。
