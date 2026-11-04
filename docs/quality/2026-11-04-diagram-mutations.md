# Mermaidの字句変異15,642入力 — #583

代表的な正常/拒否/上限例に加え、括弧・引用符・区切りを各位置で変えたときの解析とモデル所有権を観測した。
4つの合成seedから作った15,642個の異なるUTF-8入力は、4,681件受理・4,599件不正構文・6,362件未対応となった。
部分model公開、原文変更、モデル参照範囲の違反、sanitizer診断は見つからず、製品修正は行わない。

## 入力と正典経路

4seedはflowの引用/文字参照、形状/subgraph/非表示辺、sequenceの枠/活性化、自己メッセージ/文区切り。
元4seedはすべてDIAGRAM_ACCEPTEDを要求する。
tokenは二重引用符、単引用符、角/丸/波括弧、pipe、セミコロン、LF、backslash、percent、dash、山括弧、colon、ampersand、hash、tabの20文字。
Unicode codepoint境界で一文字挿入/置換、一文字削除、prefix切断を行い、同一bytesを除いた。乱数は使わない。
全入力は有効なUTF-8でNULを含まず、長さ0〜121bytes。codepoint位置をUTF-8 byte offsetと混同しない。

| 操作 | 入力数 | 受理 | 不正構文 | 未対応 |
| --- | ---: | ---: | ---: | ---: |
| 元seed | 4 | 4 | 0 | 0 |
| prefix切断 | 348 | 48 | 215 | 85 |
| 一文字挿入 | 7,506 | 2,446 | 2,027 | 3,033 |
| 一文字削除 | 358 | 128 | 81 | 149 |
| 一文字置換 | 7,426 | 2,055 | 2,276 | 3,095 |
| 合計 | 15,642 | 4,681 | 4,599 | 6,362 |

corpus.binはlittle-endian uint32長＋原文bytesの繰返しで、1,633,821bytes、
SHA256`b54c40bf91bf073b1573042eb0589a01228eb4739624c1042787c6725bc50076`。
manifest.jsonに全seed/token/操作/位置/長さ/SHAを保存し、すべての入力を再構成できる。
重複は先に出たrecipeへ一意に帰属させるため、操作別内訳はその順序に依存する。

#577成功Debug corelibを#581のhash照合原本からcopyした。source headは`63853f1`、調査時mainは`7230e88`。
src/eng/CMake入力の差分なしとlibrary hash一致を確認し、build済みのASan/UBSan/nullability計装を再利用した。
外部harnessから既存note_text→mermaid_diagram→diagram_modelを呼び、公開queryを消費してmodel/sourceを解放する。
正規CMakeと同じlld-link・明示runtime/wholearchiveを指定し、製品のbuild/gateには変更を加えない。

## 確認した契約と結果

- 受理時だけ非NULLの完全modelを公開し、不正/未対応/上限拒否では部分modelを公開しない。
- 解析後もnote_textの長さとbytesが元入力と一致する。
- 受理modelのtext/node/link/group/event数と各text長が既定上限内。title/node/link/group/eventの参照先が同modelに属する。
- すべてのlabelを読み、modelとsourceを解放する過程でsanitizer診断が出ない。
- nativeの全15,642行のID/入力長/件数をmanifestと照合し、型付き結果とmodel有無の対応を評価側でも確認する。

link-01はtools/record-run.py経由のlink.ps1、check-01は同じ記録器経由の`python check.py`→mutations.exe。
両runの子/記録側0、native子0、stderr/recordingErrors空。180秒timeoutなし。
process全体の観測は4.310秒だが、入力file読取・起動・stdout出力などを含むため解析単体の性能値へ扱わない。
入力recipe/corpus/library/exe/script snapshotとnative全行を保持する。失敗した製品試験はない。
試験準備のWindows rg glob指定誤りは実在directoryと-g指定へ訂正し、検証成功へ算入しない。

## 判断・限界・収載

元4seed以外には個別の受理/拒否oracleを置いていない。件数は観測値であり、各入力の分類や意味がMermaid仕様どおりと証明したものではない。
有限な単字変異に限定し、文法全域、複数位置の同時変異、任意Unicode/壊れたUTF-8、coverage値、layout/native描画、heap leak、
確保失敗、実UI、性能を保証しない。全heapの健全性や任意入力の安全性も主張しない。
製品不具合は再現せず、製品/test/schema/版/依存/ADR/gateを変更しない。
ARC-001/003/010/012、QLT-008/010/012、GIT-001〜004、ADR0089、開発ワークフロー§8に従う。
追加waiverはnone、既存WVR-0001はWinHTTP callbackだけで継続する。実app/実ノート/Store操作なし。

原本は`D:/NeNeFolio/agents/diagram-mutations-20261104`、実行出力は同名outputs。
品質記録/gate-proofs/日報/引き継ぎ/currentの5文書を更新し、文書リンク/範囲/差分を確認する。
最終head必須CIと正典統合、通常exe hash照合、全証跡保全・追加作業木/output整理の確定値は同agentsのintegration-completion.jsonへ収載する。
