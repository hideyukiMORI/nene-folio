# 囲まないURLのリンクの品質記録 — #850

2026-11-17。開始main `c54ad21`、途中で#839統合後のmain `dd7c36a`を取り込んだ。設計は[ADR0099](../adr/0099-bare-url-links.md)。
作業木E:/NeNeFolio/worktrees/850-bare-url-links（branch feat/850-bare-url-links）、対象実行の原本E:/NeNeFolio/outputs/850-bare-url-links
（完了後はD:/NeNeFolio/agents/bare-url-20261117へ移す）。Issue本文とADRの下書きもD:/NeNeFolio/agents/bare-url-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 範囲と境界

- 閲覧表示の「ただの文字」の中の`http://`・`https://`・`www.`のURLを、既存の山括弧autolinkと同じfield・色・hiddenの区切りで出す。
  起動は既存の`web_uri`→`note_pane_links`→`web_link_port`のまま。UI・adapter・application・保存schema・版は変えていない。
- 規則の照合先は[GFM 0.29の拡張autolink](https://github.github.com/gfm/#autolinks-extension-)と
  [cmark-gfm `extensions/autolink.c`](https://github.com/github/cmark-gfm/blob/master/extensions/autolink.c)。
  後者はweb取得の席に読ませ、`check_domain`（`allow_short`・`_`の扱い）、`autolink_delim`（末尾の記号・括弧・`;`）、
  `www_match`の直前の文字、`url_match`のscheme直前の英字の巻き戻しを引用付きで確かめた。
  当初の案（schemeにもGFMの「行頭・空白・`*_~(`」を要求、末尾の`'` `"`を残す）は、この照合で実装に合わせて直した。
- GFMからの意図的な差: URLはASCIIで止まる、`www.`はASCII以外の文字の直後も可、ドメイン253 bytes、`.`11個以上の`_`例外なし、
  URLの途中の対になった強調記号の手前で止まる。理由はADR0099。
- 試験の外部起動は無い。nativeは隔離したdesktopでpaneの`note_pane_link_selection`まで（ブラウザーは起動しない）。

## 試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| 下書き（Bashのheredocで作成） | `\\`が`\`へ、`'\0'`が実際のNUL byteへ崩れた（2回。memoryの既知事例と同じ）。backslashを含むファイルはWrite/Editで作り直し、NULは別scriptで置換して0個を確認 |
| build-01 | clang-tidy `readability-function-size`: `next_bare_link`の引数5（上限4）。終端をtoken番号から求める形へ |
| build-03 / 04 | clang-tidy: `domain_end`の認知的複雑度11・12、`trim_once`の引数5、`markdown_bare_link_length`の複雑度11（上限10）。関数を分けた（ゲート不変） |
| unit-01 | 選択子の名前違い（`--web-link`。正しくは`--web-links`）。手順の誤り |
| unit-02 | 新しい試験は全部成功。既存の`markdown_tests.c`の「`https://example.com`は装飾しない」が失敗。ADR0079の旧決定を表す期待なので、ADR0099で置き換え、英字直後の`xhttps://`と裸のメールを拒否側へ、囲まないURLのfieldを受理側へ移した |
| unit-full-02 | `folio_tests.exe`全体成功 |
| native-minimap-01 | `minimap_structure_tests.exe`成功（272 checks）。日本語に挟まれたURL・`www.`の行先・隣の文・コードspan・検索用の本文・編集表示 |
| format-01 | `clang-format`違反4件（自分の改行）。`-i`で整形 |
| coverage-01 / 02 | 測定buildで失敗。#839の取込後はコードブロックの見出し行に「コピー」のfieldがあり、試験の「`\field`が無い」が成り立たなくなった（取込前のunit-full-02では通っていた）。条件を「httpへのHYPERLINKが無い」へ直した |
| coverage-03 | 成功。分岐90.84%（下限90・除外不変）。`markdown_bare_link.c` 91.3%、`markdown_inline_tokens.c` 90.7%、`markdown_web_link.c` 70.8% |
| 追加 | `markdown_web_link_bare`の範囲外・空の拒否、`web_uri`が拒む長さ（8192超）を装飾しないこと、確保失敗の注入元へ`www.`のURLを追加 |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/850/verify.ps1 -Tag v1`）。他製品の席と端末を共有しているため、構築は`--parallel 4`、
coverage・CTestは一つずつ順に流した。

| コマンド | 結果 |
| --- | --- |
| `cmake --build <E:build> --parallel 4`（全target・clang-tidy込み） | 成功 |
| `python -B eng/coverage.py` | 成功。分岐90.87%（下限90・除外不変）。`markdown_bare_link.c` 91.33%、`markdown_inline_tokens.c` 90.85%、`markdown_web_link.c` 83.33% |
| `python -B eng/symbols.py --build-dir <E:build> --require core application` | 成功（2 libraries、違反0） |
| `python -B eng/conformance.py` / `--build-dir <E:build>` / `eng/test-conformance.py` | 成功（違反0） |
| `ctest --test-dir <E:build> --output-on-failure --no-tests=error`（fixtureのdataを新しくして1回） | 61件すべて成功（130.7秒） |
| `clang-format --dry-run --Werror`（変更したC/H）・`git diff --check origin/main` | 成功 |
| `folio_tests.exe`（全体）/ `--web-links` | 成功 |
| `minimap_structure_tests.exe` | 成功（272 checks） |

## 実画面（`image_status_tests --bare-url-visual`）

隔離したfixtureの窓（760×720、日本語）を明・暗・高コントラストで採取した。
画像は`D:/NeNeFolio/agents/bare-url-20261117/visual/bare-url-{light,dark,contrast}.png`。ゲートではなく記録用の選択子（ctestに登録しない。`--code-copy-visual`と同じ扱い）。

- 日本語の文の中のURL・`。`・末尾の`.`・閉じ括弧`)`が、意図どおりリンクの外に残ることを目で確かめた。表セル・リスト・太字の中もリンクになり、コードspanはならない。
- **色が2種類ある**: 表示名とURLが同じリンク（囲まないURLと`<URL>`）は、RichEditがnative autoURLへ変えるので明ではWindowsの青、暗では本文と同じ白に下線。
  `www.`（行先に`http://`を付けるので表示名と違う）と`[表示名](...)`はテーマのリンク色（明では赤茶、暗では橙）。
  これはADR0079で既知の挙動で、今回の変更で新しく生じたものではないが、囲まないURLが増えると目に付きやすくなる。読めること・高コントラストで飾らないことは確認した。
  テーマのリンク色へ揃える方法は別Issueで調べる（表示名とURLを変えずにautoURLへの変換を避ける手段が必要）。
