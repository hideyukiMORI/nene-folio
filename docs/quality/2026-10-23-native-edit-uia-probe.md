# 全ノート検索 EDIT の UIA Name 再診断

2026-10-23。関連 Issue: #350（メニュー UIA の検証方法）、#351（既存 Name 未達記録の調査）。
規則: ARC-001、QLT-008/012/013、ADR0056/0058/0060。製品変更・schema/依存/ゲート変更なし、Waivers: none。

## 結論と対象

旧記録の「native UIA でも Name 空・Pane」は、同じ検証プロセス内で managed UIA を初期化した影響だった。
独立した native client は既存製品の全ノート検索欄を Name「すべてのノートを検索」、ControlType Edit、ValuePattern 有りとして取得した。
製品の注釈や provider を追加する根拠はない。今後の native UIA 確認は managed UIA と別プロセスに分ける。

対象は main `12c2a3222fc615ab1ec9ad9afad79eb8899d5a81` の環境に残る 2026-10-19 生成の既存 exe。
SHA256 `6F810B7887FC3D6CAD8C25F37341B3348ACB3810F7DABB9C197BC9813E259C12` を複製し、専用desktopの合成データだけを使った。
環境は Windows 11 Pro 10.0.26300、PowerShell 7.6.6 / .NET 10.0.12、x64。製品は再buildしていない。
恒久証跡は `D:/NeNeFolio/agents/351-surface-research/native-name/` の `report.md`、`product.json`、native Cログ、`synthetic-independent.log`、`tools/`。

## 同じ製品・同じ HWND での比較

| 検証clientの状態 | Name | 型 | ValuePattern |
| --- | --- | --- | --- |
| fresh native COM interop | すべてのノートを検索 | Edit (50004) | true |
| managed assemblyをloadだけ | 同上 | Edit | true |
| `AutomationElement.FromHandle`初回実行後にnativeで読む | 空 | Pane (50033) | false |
| 診断client内だけcallbackを解除して再取得 | 正しい名前へ復帰 | Edit | true |

別プロセスの fresh C23/MTA client は、managed 初期化の前後とも正常だった。主窓rootのdescendantsからも同じ欄を取得した。
default proxy table は17件で、RestoreDefaultTableの前後とも正常。注釈サービスの即Releaseと窓の生存中保持は、独立した合成EDIT試験で両方正常だった。
したがって、注釈サービスの寿命を原因とする仮説も今回の結果では支持されない。

Microsoftの[client-side proxy解説](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-clientsideprovider)と、
.NET公式の[UIA callback登録経路](https://github.com/dotnet/wpf/blob/main/src/Microsoft.DotNet.Wpf/src/UIAutomation/UIAutomationClient/MS/Internal/Automation/UiaCoreApi.cs)はこのclient内の差と整合する。
インストール済みbinaryと公開sourceの完全一致までは検証していない。

## 実行と限界

- `pwsh -NoProfile -File D:/NeNeFolio/outputs/351-native-name/compile.ps1`: 成功。固定 clang-cl 19.1.5 / C23 で診断用プログラムだけをbuild。
- 専用desktop workerによる製品・合成EDITの比較: 成功。PID/HWND/正規化したexeパスを照合し、所有したfixtureをWM_CLOSEで終了、exit 0。前面窓は開始前後で不変。
- I-A/I-Bの過去の観測自体は消さないが、製品のName不具合の証拠としては使わない。
- 物理IME、実スクリーンリーダーの聴取、全OSの一般保証は未確認。標準EDITのTextPatternはこのMSAA proxy経路ではfalseであり、今回のName確認からTextの再実装は要求しない。
