# PNG/JPEG変異入力の復帰と検証限界 — #591

合成PNG/JPEGから3,605入力を作り、既存の復号経路をDebug/Releaseで各2周観測した。
全14,420条件で型付き結果・拒否時のout保持・直後の正常画像一致・原bytes不変が成立した。
一方、native試験は両版とも1で終了している。GIF初回のハンドル+1とDebugのASan初期化診断を分離して記録し、
試験全体を成功へ書き換えない。製品不具合の根拠は得ておらず、製品/test/依存/gateは変更しない。

## 対象と入力

正典経路は`image_decode_read`→OS WIC→`image_bitmap_create`。
Debugは#587の成功buildのcore/application/adaptersを、保全archive全体と各entryのSHA照合で再利用した。
sourceは`7ddcbf79fa2a39daf8680229c9a63fa80aeb75da`。現在mainとのsrc/tests/eng/CMake入力不変を確認した。
Releaseは#589の測定用正典CMake build、source `265b36a3be607498e76bd189e4bcfb1a5e8b42f7`の同3libを同様に照合した。
Windows11 build26300、固定LLVM19.1.5/MSVC14.44.35207、Python3.12.10。
原本は`D:/NeNeFolio/agents/image-decode-mutations-20261104`、全run/raw/bitmapは同名outputsに置いた。

PNGは17×11のRGBA、JPEGは23×13のRGBで、座標から決定的な色を作った。PNGは標準zlib、JPEGはPillow12.3.0で作成した。
原PNG774bytes、JPEG1019bytesの各byte位置でprefix切断とbit7反転を行い、元2入力、PNG寸法9変形、
末尾追加2件、非対応GIF/BMP、16MiB+1bytes、処理前/DECODING/RESIZINGでの取消3件を加えた。
RESIZING取消用PNGは4096×4。乱数・実ノート・外部画像・実appは使用しない。
corpusは19,311,671bytes、SHA256`0ab69743aabde21d687cd13361289bcc364422b01d0826c5601b6f96ccf82130`。
manifest.jsonに全ID・入力名・長さ・SHA・限定した期待結果を残し、prepare.pyに生成手順を保存した。
重複bytesは除去していないため、3,605はユニークなbyte列の数ではない。

## 確認した境界

各入力の直後に元PNG/JPEGを交互に復号し、bitmap全bytesを開始時の基準画像と比較した。
拒否時は既存の生存bitmap pointerをoutのsentinelにし、不変を確認する。成功時は別ownerが返り、寸法が1..2048であることを確認した。
入力の実bytesは呼出し前copyと比較し、最後にcorpus全体も照合した。nativeのimage_adapterはcallback文脈の生存ownerとして作るが、
この試験はworker queueへ画像要求を投入しない。直接decoderの復帰を、worker/ticketの競合確認へ拡張しない。

各版・各周の結果はREADY2,760 / FAILED836 / TOO_LARGE4 / UNSUPPORTED2 / CANCELLED3。
全件の構造化結果がDebug/Releaseで一致し、各版の周回間も一致した。
各版5,520件の成功bitmapと基準2枚、計11,044fileを実データから再hash照合した。
版間5,522組・各版の周回間2,760組はすべてSHA一致。直後の正常画像14,420回も全bytes一致した。

変異画像ごとの受理/拒否の仕様oracleは置いていない。WICは一部の途中切断や破損を受理するため、
READYを「入力が完全に正しい」とは解釈しない。画素の美観や元画像との一致も変異入力の期待値ではない。

## 失敗を切り分けた結果

`check-01`（Debug）と`trace-01`（Release）はnative/評価/記録側がすべて1、recordingErrorsは空。
失敗assertは各周のhandle復帰条件だけで、初期117→1周後118→2周後118、GDIは0→0→0だった。
Releaseの各入力後の追跡により、最初のGIF（ID3599）でだけ+1と確認した。

`gif-01`は製品wrapperを呼ばず、WIC factory/stream/decoderだけを直接作成・解放する診断である。
PNG初回は95→107、JPEGは107維持、GIF初回は107→108、後続GIF3回は108維持。
GIFのdecoder/stream/factoryは各回のRelease戻り参照数がすべて0で、COM終了時は108→99だった。
同じ+1が製品コード外でも再現するため、この差を製品ownerの解放漏れと認定しない。
OS/codecが保持する全資源の所有証明や、任意回数・全heap leakの保証ではない。

Debugには`interception_win: unhandled instruction`が1行ある。`startup-01`は復号前に終了する入口だけで同じ診断を再現した。
moduleBaseと診断addressをmapに対応させると`libucrt:frexp.obj`の`frexp+4`、命令bytesは`0f 29 74 24 30 0f 28 f0`だった。
固定[LLVM19.1.5のinterception実装](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-19.1.5/compiler-rt/lib/interception/interception_win.cpp)でも、
未知の命令を診断してinterception失敗を返す経路と確認した。取得原文とSHAをagentsへ保存した。
この観測を製品のmemory違反へ断定せず、またsanitizer成功とも扱わない。toolchain更新・抑制・除外は行っていない。

JPEG変異に対する診断は各版2,858行/46種で一致し、rawのまま保持した。ReleaseとDebugのstderr差は上記ASan1行だけ。
`FAIL: handle/GDI counts return`2行も各版のrawに残す。全stderrを空と扱うことや、診断を捨てて成功にする処理は行わない。

## 実行記録と判断

すべて`tools/record-run.py`で入力snapshot・raw・終了値を保存した。

| run | コマンドと結果 |
| --- | --- |
| link-01 / link-02 | Debug link。子/記録0。外部harnessのfopenにdeprecation警告2件。02は起動入口とmapを追加 |
| check-01 | `python check.py check-01`。native/評価/記録1、7,210条件、上記診断/handle差を保存 |
| startup-01 | `decode-02.exe --startup`。子/記録0だがASan診断あり。成功したsanitizer検証ではない |
| link-trace-01 | Releaseへlink。外部harnessをfopen_sへ直し/WX、子/記録0・stderr空 |
| trace-01 | `python trace.py trace-01`。native/評価/記録1、7,210条件、初回GIFのhandle増加箇所を特定 |
| link-gif-01 / gif-01 | WICだけの直接診断。子/記録0・stderr空、COM解放参照数とhandle推移を記録 |
| audit-01 | `python audit.py`。子/記録0・stderr空、全11,044画像/入力/全行/失敗終了/診断差/mapを監査 |

audit成功は保存記録の整合確認であり、nativeの失敗を成功に変えるものではない。
任意入力、別codec/OS、色管理、EXIF、描画UI、worker競合、全heap安全・確保失敗・性能は未保証。
実行全体の経過秒はbenchmark値へ使わない。追加の製品修正や同じ全入力の再実行は行わない。

ARC-001/003/010/012、QLT-008/010/012、GIT-001〜004、ADR0085、開発ワークフロー§8を適用。
品質記録/gate-proofs/日報/引き継ぎ/currentの文書5件だけを変更する。API/schema/版/依存/ゲート不変。
差分/相対リンクを確認し、最終headの必須CI成功と正典統合を要求する。通常exeは#587との入力/hash一致で再利用する。
統合/通常exe照合/保全整理の確定値はagentsのintegration-completion.jsonへ。追加waiverなし、WVR-0001は既存範囲のみ。
実app/実ノート/Storeへの操作なし。
