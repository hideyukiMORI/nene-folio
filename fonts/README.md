# fonts/ — 同梱の書体

NeNe Folio が exe の隣の `fonts\` から読む書体の資産。設計の正本は [ADR 0036](../docs/adr/0036-bundled-fonts.md)。

## 入っているもの

| ファイル | 書体 | ライセンス本文 |
| --- | --- | --- |
| `NotoSansJP-Regular.otf` / `NotoSansJP-Bold.otf` | Noto Sans JP（日本語） | `LICENSE-Noto-CJK.txt` |
| `NotoSansSC-Regular.otf` / `NotoSansSC-Bold.otf` | Noto Sans SC（简体中文） | `LICENSE-Noto-CJK.txt` |
| `IBMPlexMono-Regular.ttf` | IBM Plex Mono（等幅。番号・数・日時・キーの名前） | `LICENSE-IBM-Plex-Mono.txt` |

- 欧文は Noto Sans JP のラテン文字で描く（2026-09-23・hide の判断。欧文専用の書体は同梱しない）。
- すべて静的フォントで、取得したバイト列をそのまま置いている（変換・改変・family 名の変更はしない）。
- 等幅の書体は IBM Plex Mono（2026-11-23・hide の判断。#1217）。欧文と数字だけの書体で、仮名・漢字は持たない。細字だけを同梱する（太字は使っていない）。
- ライセンスはどれも **SIL Open Font License 1.1**。本文は、書体の系統ごとに上の表の 1 ファイル。
- 出所（取得元 URL・upstream の revision・バイト数・SHA-256・ライセンスの対応）は [manifest.json](manifest.json) が正本。取得日は 2026-09-12（IBM Plex Mono は 2026-11-23。manifest の各項目の `acquired`）。

## 守っているもの

CNF-012（`eng/conformance.py`）が、manifest の各ファイルがこのフォルダに在ってバイト数と SHA-256 が一致すること、
manifest に無いファイルが無いこと（この `README.md` と `manifest.json` を除く）、各書体のライセンスが manifest の `licenses` に在ることを検査する。
ビルドは `nenefolio` の POST_BUILD でこのフォルダを exe の隣の `fonts/` へ複写する。

## 更新の手順

1. 新しい版のファイルを取得し、manifest の `sourceURL`・`fullrevision`・`bytes`・`SHA256` を書き換える。
2. **manifest と書体のファイル（とライセンス本文）を同じコミットで差し替える。**
3. `pwsh -NoProfile -File ./eng/check.ps1` を通す（CNF-012 が manifest との一致を確かめる）。
