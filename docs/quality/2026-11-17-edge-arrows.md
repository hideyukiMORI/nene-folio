# 本文の端での↑↓の品質記録 — #854

2026-11-17。hideの依頼「カーソルが最上端・最下段にいるときに上下キーを押すと、Windows系のアプリは行頭や行末に移動する。メモ帳の動きに合わせて」。
作業木E:/NeNeFolio/worktrees/854-edge-arrows（branch feat/854-edge-arrows、#853の枝から開始し統合後のmainを取り込む）、
対象実行の原本E:/NeNeFolio/outputs/854-edge-arrows、probeの原本D:/NeNeFolio/agents/edge-arrows-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 参考にしたもの

同じhideの別製品NeNe Nib（#1386、ADR 0146の追補11）が同じ依頼を実装済み。決め事のうち、
「折り返した画面行の端で判定する」「Shiftは選択を伸ばす」「もう端なら何も変えない」「キャレットが見える所へ送る」をそのまま採った。

## 調査（製品の外のprobe、`probe.c`）

隔離desktopの`RICHEDIT50W`へ`WM_KEYDOWN`を送り、選択の変化を読んだ（probe-01.log）。

| 操作 | RichEditの既定 |
| --- | --- |
| 最初の行で↑ / 最後の行で↓ | **動かない**（キャレットはそのまま） |
| 最初の行でShift+↑ / 最後の行でShift+↓ | 文書の先頭 / 末尾まで選択を伸ばす |
| 最初の行でPgUp / 最後の行でPgDn | 文書の先頭 / 末尾へ移る |
| 折り返した段落の最初 / 最後の画面行で↑ / ↓ | 動かない |
| 空の文書・1行の文書 | 動かない |

メモ帳と違うのは「Shiftなしの↑↓が端で動かない」ことだけだった。

## 直し

- `note_pane`のサブクラスで、Ctrl・Altなしの↑↓（Shiftは可）と、既存の`Ctrl+k` / `Ctrl+j`（`navigation_command`）の上下だけを見る。
  動く側の位置（Shiftなしは↑なら選択の始まり・↓なら終わり、Shiftありは選択の動く端）が最初の表示行（↑）・最後の表示行（↓）にあるときだけ、
  文書の先頭 / 本文の末尾へ置く（Shiftありは反対の端を残して伸ばす）。`EM_SCROLLCARET`で見える所へ送る。
  判定は`EM_EXLINEFROMCHAR`と`EM_GETLINECOUNT`（RichEditの表示行）なので、折り返しも同じ一つの規則で扱う。
- それ以外（端でない行、PgUp/PgDn、IME変換中、読込中）はRichEditの既定のまま渡す。
- 本文・Undo・未保存の印・保存は変えない（キャレットと選択だけ）。

## 試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| build-01 | clang-tidy: 試験の補助関数の引数5（上限4）。Shiftを鍵の値の印（`with_shift`）へ畳んだ |
| native-01 | 失敗: 試験が末尾を`WM_GETTEXTLENGTH`（段落の区切りをCR+LFの2文字で数える）で求めていた。キャレットと同じ数え方の`EM_GETTEXTLENGTHEX`（`GTL_NUMCHARS`）へ直した |
| native-02 | 失敗: 最後の行のShift+↓が、RichEditの既定では本文の後ろの見えない段落記号まで選ぶ（15..24、本文の末尾は23）。メモ帳と、Nibの「Shiftは同じ着地まで伸ばす」に合わせ、Shiftも同じ規則で扱うことにした。最初はTOMの`EndKey(tomStory, tomExtend)`で伸ばしたが同じく24まで選んだので、`EM_SETSEL`（反対の端を残し、動く端を本文の末尾へ）にした |
| 追加の試験 | 選択の動く端が端の行に無いとき（反対の端だけが端の行にあるとき）は既定の1行移動のままであることを、上下とも足した。最初の版は閲覧表示で既定の移動先が偶然反対の端と同じ位置になり、選択が畳まれて失敗した（試験の材料の問題）。反対の端を本文の両端に置き直した。編集表示ではRichEditが本文の末尾の端を段落記号まで広げることがあり（既定の動き）、それを許す形にした |
| native-08 / 09 | 成功（760 checks） |
| 変異1 | 鍵の経路の呼び出しを消す変異は「未使用の関数」でコンパイルが落ち、古いexeが走って成功に見えた（無効な結果）。記録だけ残す |
| 変異2 | 鍵の経路が常に断る変異で、最初の行の↑が「4..4（既定の動かない）」となり失敗することを確認（mutant-run-02）。戻して再構築し成功 |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/854/verify.ps1 -Tag v1`、#853統合後のmainを取り込んだ後。取り込みの衝突2件は、#853の品質記録をmainの版に、試験はこの枝の追加分だけにして解いた）。

| コマンド | 結果 |
| --- | --- |
| `cmake --build <E:build> --parallel 4`（全target・clang-tidy込み） | 成功 |
| `python -B eng/coverage.py` | 成功。分岐90.88%（下限90・除外不変） |
| `python -B eng/symbols.py --build-dir <E:build> --require core application` | 成功（違反0） |
| `python -B eng/conformance.py` / `--build-dir <E:build>` | 成功（違反0） |
| `ctest --test-dir <E:build> --output-on-failure --no-tests=error` | 61件すべて成功（157.0秒） |
| `git diff --check origin/main` | 成功 |

最初のv1の実行は、作業手順の誤り（取り込みの衝突でコマンドの連鎖が止まり、検証用scriptが作られていなかった）で起動に失敗した（終了値64）。scriptを作って取り直した。
