# Mermaid文区切りの同値性 — #575

既存の対応済み構文に対し、文の区切りを変えても描画が変わらないかを調査した。
8つの合成図×8種類の区切り×明暗で128画像を生成し、基準LFの16画像に対する変形後112画像の全SHA256が一致した。
この範囲では欠陥が見つからず、製品コードとリポジトリのテストは変更しない。

## 入力と対象

sourceは#573最終head`df6ba8b729a5a95d459c06332873bad29537b343`、main統合は`2c4768a`。
成功済み固定Releaseのcore/application/adaptersの3libraryと、既存native試験のrender/font/bitmap保存helperを使った。
外部harnessのmainは各入力を明暗で1回ずつ生成するだけで、既存の試験mainや性能計測の21回loopは呼ばない。
adapterにはexeのあるD側の合成試験用rootを渡し、実app・実ノートを使わない。

| 図 | 内容 |
| --- | --- |
| 0 | TB、引用内の日本語/文字参照、引用pipe辺ラベル、circle |
| 1 | LR、group、複数行見出し、台形/逆台形、破線、stadium |
| 2 | RL、文中ラベル付きの実線/太線/破線、戻り辺 |
| 3 | BT、引用内の`;`/`%%`/閉じ括弧、double circle、hexagon |
| 4 | sequence、actor/alias、自動番号5から2刻み/off、`+/-`、note、文字参照 |
| 5 | sequence、loop/alt/else、自己message、入れ子activationと終了 |
| 6 | sequence、par/andとcritical/option |
| 7 | sequence、両側/開いた矢頭/終端記号、note over、自己、多行文字 |

8種類は基準LF、CRLF、CR、セミコロン、文の行頭/行末のspace/tab、空行/空文、独立行コメント、行末コメント。
各変形は完成した文の間へ適用し、引用文字列内の改行/空白/コメント記号には触れない。
`inputs.json`と64個のUTF-8原文を保持し、書込時のnewline変換を無効にしてCR/CRLFをそのまま保存した。
各原文のSHAを実行前に照合した。文字参照中のセミコロンと引用中のコメント記号も入力に残っている。

## 実行と結果

恒久原本は`D:/NeNeFolio/agents/statement-separators-20261104`、出力は同名`outputs/`。
文書作業木は`D:/NeNeFolio/worktrees/575-statement-separators`。

| run | コマンド/結果 |
| --- | --- |
| link-01 | `pwsh -NoProfile -File <agents>/link.ps1`。PowerShell配列内のCソースpathの引用符不足で起動時に失敗。子/記録側1 |
| link-02 | 引用符を訂正。同じ固定clang-cl `/MT /O2` と既存Release3libで外部harnessをlink。子/記録側0 |
| compare-01 | `python -X utf8 -B <agents>/compare.py`。harness1process、64source×2theme、128画像。子/記録側0、stderr0 |

すべて`tools/record-run.py`で新規run-directoryへargv、入力snapshot、raw stdout/stderr、終了値を記録した。
全runのrecordingErrorsは空。link-01ではexe生成を成功扱いせず、失敗rawも保持する。
link-02にexeをartifactとして保存。compare-01はsource/exe/font/scriptを入力、`comparison.json`を成果物として収載する。
比較表には各画像の実path、寸法、SHA256、基準との一致を保存した。実bitmap128枚もZIP保全に含める。
比較完了後に実ファイルの全hashを再照合した。成功後に製品や入力を変更せず、文書化・push・統合では再実行しない。
基準のflow明/sequence暗の2画像を視認し、文字参照・番号・pipe文字が表示されることも確認した。

## 調査中に修正対象から外した候補

数値文字参照の0は既存decoderがU+FFFDへ置換し、modelはNUL/不正UTF-8/制御文字を拒否している。
sequenceの活性なしdeactivateとleft/right noteの複数参加者もparserが明示拒否する。空group単体はノードなしとして拒否し、
通常nodeと併存する空groupは既存回帰で見出し保持を確認している。これらは読取による候補除外であり、新しい実行検証の成功として数えない。
図原文のMarkdown収集は既存line parserの行内容にLFを足す経路であり、今回の直接CR/CRLF入力はparser/renderer境界の追加観測である。
調査時に不存在pathやPowerShellのrg glob指定で読取失敗があり、実在fileへ訂正した。製品検証の失敗/成功に混ぜない。

## 規則・限界・完了条件

ARC-001/003/012、QLT-008/010/012、GIT-001〜004、ADR0089、開発ワークフロー§8に従い記録した。
製品/schema/API/依存/版/gate/ADRの変更なし。追加waiverはnone、既存WVR-0001はWinHTTP callbackに限り継続。
一致は同じWindows/font/palette/Releaseでの8図と8変形に限る。任意の空白挿入、引用内改行、未対応構文、全書体/DPI、保存/編集UIの同値性は保証しない。
この128画像から速度改善や全入力の正しさを導かない。#573の成功済みCI/対象試験も再実行しない。
文書差分/参照/変更範囲確認後、最終head必須CIと正典統合を行う。通常exeは#573とのhash一致を照合し、再build/data操作は不要。
確定値と保全/整理結果は`D:/NeNeFolio/agents/statement-separators-20261104/integration-completion.json`へ。
