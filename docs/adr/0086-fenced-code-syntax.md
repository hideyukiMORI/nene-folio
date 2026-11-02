# ADR 0086 — fenceの言語指定を同じRTF経路で色分けする

- 状態: 受理（設計。対象検証と最終CIは品質記録へ）
- 日付: 2026-11-03
- Issue: #524
- 規則: ARC-001/003/004/005/008/009/010/012、C-002/003/005/008/011/012/013/016、QLT-008/009/012/013
- 関連: ADR0049/0057/0070/0084

## 根拠と範囲

hideは#522の画像修正後にコードシンタックスカラーと言語対応を進めるよう指定した。
既存の`markdown_blocks`はfenceのinfoを使わず、コード本文をRTFへ単色でescapeしている。
Markdown parser・RTF色表・RichEditの一つの経路を保ち、コード本文に限った基本字句色分けを加える。

## 決定

1. fenceのinfo先頭の空白区切り語を言語IDとし、ASCII英字の大小を無視する。残りのinfoは表示に使わない。
   C/C++（c/cpp/c++/cc/cxx）、JavaScript/TypeScript（javascript/js/typescript/ts）、Python（python/py/python3）、
   JSON、Shell/Bash（sh/shell/bash/zsh）、PowerShell（powershell/ps1/pwsh）、SQLを初期対象にする。
   空・text/plaintext/txt・未知語は単色のまま。言語を本文から推定せず、保存する原文やfenceを書き換えない。
   言語選択GUIや表示用見出しを追加せず、コード本文の文字列・検索位置・段落の意味を維持する。
2. `code_language`の閉じたenumと一つの表がaliases・基本キーワード・字句規則を所有する。
   `code_syntax`はコードブロック寿命の不完全型で、言語とコメント/引用の継続状態だけを持つ。
   既存parserが切り出した行を一度だけ走査し、RTF文字escapeは`rtf_text_append`を共有する。
   AST/token配列・第2のMarkdown parser・本文の複製・再帰・外部library・コード実行は追加しない。
   各行の借用はappend呼出し中だけ、outは同じ変換bufferの借用。閉じ/EOF/container終了/失敗で解放する。
3. 通常文字、キーワード、文字列、コメント、数値の五種に分ける。ブロックコメントは対応言語の区切りを使う。
   Python triple quotes、JS/TS template文字列、Shell/PowerShellの複数行引用、PowerShell here-stringを継続できる。
   通常のC/JS/Python引用は未閉じでも原文を保ち、明示的な行継続以外で後続行へ漏らさない。
   SQL/PowerShellの同じ引用記号の二重化と各言語のescapeを区別する。fenceを越えて継続しない。
   これはコンパイラではない。補間式の内側、JS正規表現、C++ raw literal、Shell here-document、SQL方言固有の
   dollar quote等は厳密な字句色分けの対象外で、文字をそのまま保持する。意味解析・エラー診断はしない。
4. 色は既存`rtf_palette`へkeyword/string/comment/numberの4役割を追加する。RTF色表の既存1〜6を保ち、7〜10へ追記する。
   固定値は127配色の`folio_theme`正本表にだけ収載する。明暗の基本色からコード背景に対し4.5:1以上になる値を
   取り込み時に確定し、runtimeに別の色生成を置かない。既存本文/コード地/単色コード値は変更しない。
   ADR0057の高コントラストでは4役割ともOSのwindowTextへ写す。theme比較も新役割を含む。
5. 編集用plain RTFは原文のまま。閲覧/テーマ変更は既存applicationのRTF再生成とnative投入へ合流する。
   ミニマップは既存native書式sampleを使い、独自lexerを加えない。保存schema・版・依存は変更しない。
   確保失敗は既存MARKDOWN_RTF_OUT_OF_MEMORYへ合流し、部分生成物を公開しない。

## 受理

alias/未知語・各言語の基本token・コメント内引用/引用内コメント・複数行/未閉じ/escape・UTF-8/長文・
fence/container境界・出力文字保持・確保失敗をunitで確認する。127配色の新token可読性、HCと比較も確認する。
実RichEditで文字/色/背景、plain復帰、再着色、選択/Undo/dirty、minimapを対象確認する。
最終headの必須CI成功後、main統合・通常反映・恒久保全・追加作業物整理まで行う。追加waiverは使わない。

## 参照

- [CommonMark 0.31.2 fenceとinfo](https://spec.commonmark.org/0.31.2/#fenced-code-blocks)
- [Python lexical analysis](https://docs.python.org/3/reference/lexical_analysis.html)
- [ECMAScript lexical grammar](https://tc39.es/ecma262/#sec-ecmascript-language-lexical-grammar)
- [PowerShell quoting rules](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_quoting_rules?view=powershell-7.5)
