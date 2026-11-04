# Mermaid拒否と同じworkerの正常復帰 — #578

不正構文・未対応構文・各上限を超えた図が部分画像を公開せず、次の正常要求へ復帰するかを調査した。
最初の22入力と面積専用へ訂正した追加1入力を明暗で観測し、直接生成46件と画像portへの拒否要求92件を記録した。
その直後の正常生成92件は、すべて同じ図/配色の基準bitmapとbytes・SHA256が一致した。
製品の欠陥は見つからず、製品・リポジトリのテスト・規約・上限を変更しない。

## 正典経路と入力の再利用

sourceは#573最終head `df6ba8b729a5a95d459c06332873bad29537b343`、現mainは`6deb9b6`。
両者のsrc/tests/eng/fonts/CMakeLists.txtに差がないことを確認した。
#573の保全ZIP全体と対象Releaseのcore/application/adapters3lib、書体7fileのSHAを照合し、D側の新出力へ抽出。
書体は現checkoutの全hashとも一致した。再buildせず、同じ固定clang-cl `/MT /O2`で外部harnessだけをlinkした。
既存nativeのfont/bitmap/待機helperと、実製品のparser/metrics/layout/renderer/image_port/worker/cacheを使用する。
各processが所有するadapterは1つ、試験rootはD側のexeのdirectoryである。実app/実ノートへ触れない。

## 拒否条件

| 分類 | 合成入力・結果 |
| --- | --- |
| 不正構文6種 | flow未閉鎖/辺終点なし、sequence枠未閉鎖/余分end/未活性deactivate/複数参加者へのleft note。DIAGRAM_INVALID→IMAGE_FAILED |
| 未対応4種 | erDiagram、init directive、style、HTML label。DIAGRAM_UNSUPPORTED→IMAGE_UNSUPPORTED |
| 未対応配置1種 | subgraph内のLRと外側TBの不一致。parser/文字計測成功後、layoutがDIAGRAM_UNSUPPORTED |
| model/source上限8種 | 原文262145bytes、label2049bytes、node257、link1025、event2049、frame17段、activation17段、group65。DIAGRAM_TOO_LARGE→IMAGE_TOO_LARGE |
| 配置座標1種 | LRの160node。parser/文字計測成功後、layoutがDIAGRAM_TOO_LARGE |
| bitmap辺長1種 | LRの40node、11184×136。layout成功、bitmap生成がIMAGE_TOO_LARGE |
| 初回面積fixture | 16列×16段、3984×9688。面積と辺長の両方を超えるため、面積だけの証明には使わない |
| 訂正した面積fixture | 16列×8段、3984×4592（18,294,528画素）。両辺8192以下、4M画素超過だけでIMAGE_TOO_LARGE |

全直接拒否でbitmapはnull。parserが拒否した入力のmodelもnullだった。
原文サイズ超過はimage_job_diagramがqueue受付前に同期拒否する。ほかの21種と追加面積入力はIMAGE_PENDINGで受け付け、
同じworkerから型付きの拒否を返した。計92要求のうち同期拒否4件、非同期拒否88件である。

## 復帰と観測範囲

入力ごとに明暗、各2回の「拒否→正常要求」を同じadapterへ逐次投入した。
正常図は既存flow/sequenceの2図。入力ごとの末尾コメントを加えて初回のcache keyを分け、2回目は同じkeyを再要求する。
cache hitの内部回数は計測していない。正常画像の内容一致を、内部cache成功の独立証明へ読み替えない。
正常図の4基準画像を各runで作り、全正常画像のメモリbytesと保存後SHAを照合した。
合計92復帰画像と8基準画像の100bitmapを保持し、flow明/sequence暗の2枚を視認した。

拒否/復帰完了後のphaseはIDLE。正常要求は新ticketを持ち、受付済みだった古ticketのpollはIMAGE_CANCELLED・bitmap nullだった。
原文超過ではticketを発行していないため、初期値0へのpollを古ticket拒否の証明へ数えない。
原文超過の同期拒否が、既存の進行中要求を保持するかは今回未試験。

## 全試行と試験側の訂正

恒久原本は`D:/NeNeFolio/agents/diagram-rejections-20261104`、出力は同名outputs、
文書作業木は`D:/NeNeFolio/worktrees/578-diagram-rejections`。

| run | コマンド・結果 |
| --- | --- |
| link-01 | `pwsh -NoProfile -File <agents>/link.ps1`。外部harnessを既存Release3libへlink。子/記録側0 |
| check-01 | `python -X utf8 -B <agents>/check.py check-01`。native子0・stderr0、132JSON行・92bitmap。評価scriptの期待誤りで子/記録側1 |
| 読取再評価 | `python -X utf8 -B <agents>/prepare-followup.py`。失敗6行を原文の同期拒否4行と面積fixture不適合2行だけと照合。全88復帰画像を再hashし、製品再実行なし。check-01-audit.jsonへ理由を別保存 |
| check-02 | `python -X utf8 -B <agents>/check.py check-02 bitmap-area-only.mmd`。訂正した面積入力だけ追加。6JSON行・8bitmap、子/記録側0・stderr0 |

各link/checkはtools/record-run.pyで入力snapshot、raw、終了値、summaryを記録した。全runのrecordingErrorsは空。
check-01の失敗resultと元summary/元期待値を上書きせず、全体を成功へ書き換えない。
image_job_diagramの原文上限guardを読み、同期拒否の理由を確認した。面積だけを検査する目的は維持し、fixtureを縮めて追加実行した。
成功した既存21種の製品入力とexeは不変であり、確認済み復帰を再実行しない。
診断script/fixtureの準備と読取再評価の原本、全入力/ライブラリ/書体/exe/画像/rawを保全対象に含める。

## 規則・限界・完了条件

ARC-001/003/010/012、QLT-008/010/012、GIT-001〜004、ADR0089、開発ワークフロー§8。
これは有限な観測記録で、新しい機械gateの追加ではない。製品/schema/API/版/依存/ADR変更なし。
追加waiverはnone、既存WVR-0001はWinHTTP callbackに限り継続。
任意の不正入力、同時cancel/submit競合、確保失敗、全heap/全書体/DPI、実UI通知/本文保持の保証へ広げない。
処理時間やcache hit率の計測でもない。Store再提出/公開は行わない。
文書リンク/差分/範囲確認、最終head必須CI・正典統合後、#573通常exeのhash不変を照合して再build/data操作を省く。
確定値・保全・整理は`D:/NeNeFolio/agents/diagram-rejections-20261104/integration-completion.json`へ収載する。
