# ADR 0100: Webリンクのfield命令はschemeを大文字で書き、URIは小文字のschemeで持つ

Status: accepted / 2026-11-17 / Issue #852

[ADR0079](0079-view-web-link-activation.md)の「URLそのものを表示するnative autoURLはWindows標準のリンク色/下線となる」を置き換える（[ADR0099](0099-bare-url-links.md)の同じ趣旨の記述も同様）。
起動の経路（検証済み`web_uri`・RTF HYPERLINK field・`note_pane_links`・`web_link_port`）と、自動URL検出を使わない決定は維持する。

## 背景

表示名がURLと同じリンク（囲まないURL・`<URL>`・`[https://x](https://x)`）は、RichEditがfieldをnative autoURLへ変え、
テーマのリンク色ではなく明ではWindowsの青、暗では本文色の白で描く。#850で囲まないURLがリンクになり、同じノートに2色が並ぶことが増えた。

隔離desktopの実測（Msftedit 10.0.26100.9949、原本`D:/NeNeFolio/agents/link-color-20261117/`）:

| RTFのfield | 読込後のlink type | 描いた色（画素数） |
| --- | --- | --- |
| 命令`https://example.com/docs/start`・表示名が同じ（現行） | 1（autoURL扱い、`GetURL`はS_FALSE） | 明: 青1077・テーマ色0 / 暗: 本文色617・青249・テーマ色123 |
| 命令`HTTPS://example.com/docs/start`・表示名`https://...` | 3（friendly name、`GetURL`は命令のURL） | 明: テーマ色689・青0 / 暗: テーマ色788・青0・本文色0 |
| 命令に`\o`の切替を足す | 1 | 現行と同じ |
| 読込後に`EM_SETCHARFORMAT`で色 | 1のまま | 変化なし（受理1だが描画は青） |
| 読込後にTOM `SetForeColor` / `SetURL` | — | `E_ACCESSDENIED` |

Microsoftの資料も、autoURLの色はクライアントが指定できず、コントラスト不足では白へ落とすと説明している
（[Autolink color contrast](https://learn.microsoft.com/archive/blogs/murrays/autolink-color-contrast)）。

## 決定

- coreのRTF出力でWebリンクのHYPERLINK命令を書く一か所（`emit_field`）が、`http`/`https`のschemeを大文字で書く。
  schemeはASCIIの大小を区別しない（RFC 3986 §3.1）ので同じURIで、RichEditは表示名と違う命令と見てfriendly-nameリンクのまま保ち、
  表示名に付けたテーマのリンク色（`\cf4`）で描く。表示名・検索・選択・コピーの文字は変わらない（命令はhidden）。
- `web_uri_create`はschemeを小文字（RFC 3986 §6.2.2.1の正規形）へそろえて持つ。命令から読み戻した`HTTPS://...`も、
  portと既定のブラウザーへは`https://...`で渡る。scheme以外のbytesは従来どおり原文のまま。
- `note_pane_links`のautoURLへの備え（`GetURL`がS_FALSEならlink typeと範囲から読む）は残す。将来のRichEditが大小を区別せずに比べても、
  見た目が従来へ戻るだけで、起動は壊れない。

## 却下した案

- 読込後に色を当てる（`EM_SETCHARFORMAT`・TOM）: 実測で効かない、または拒否される。
- 表示名へ見えない文字を足して一致を崩す: 検索・選択・コピーの文字が変わる。
- 命令へ切替（`\o`等）を足す: 実測で変換を避けられない。
- schemeを大文字にするのを表示名が一致するときだけにする: 一致の判定を出力側にもう一つ持つことになり、どのリンクも同じ書き方で困らない（ARC-001）。
- 現状維持: 同じノートの中でリンクの色が2種類になり、暗では本文と区別しにくい。

## 検証

unitでRTFの命令の大文字と表示名の不変、`web_uri`のscheme正規化を確かめる。nativeで、表示名がURLと同じリンクが
autoURL（link type 1）にならず、明暗ともテーマのリンク色の画素で描かれることを確かめる。`note_pane_link_selection`は命令の原文（`HTTPS://...`）を返し、
applicationが`web_uri_create`を通した後のportには小文字のschemeで届くことを、既存のstate試験の偽portで確かめる。
最終headの必須CIで正典`eng/check.ps1`を要求する（CIの端末のRichEditでも同じ結果になることの確認を兼ねる）。Waivers none。
