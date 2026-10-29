# ADR 0072 — native表の字下げと残幅を既存container位置へ揃える

- 状態: 受理（設計。完成候補の結果は品質記録へ）
- 日付: 2026-10-29
- Issue: #410
- 規則: ARC-001/003/004/008/010/011/012、C-002/003/005/008/011/012/014/016/017、QLT-008/009/012/013
- 関連: ADR0069/0070/0071、FR-005/006/011

## 文脈

#404で表の所属は順序付きcontainer ownerへ揃えたが、RTFの`trleft0`とpane全幅の分配が残り、
引用やリストの表が本文左端へ戻る。coreだけに字下げを足すと、幅を引かないnative再配置で右側がはみ出す。
#405の商/余り分配は維持し、coreの意味解釈とUIの実寸配置の境界を同時に直す。

## 決定

1. 表の字下げは`markdown_containers_indent`が返す既存本文位置だけを正とする。
   blocksはheader/bodyの描画時に同じownerの位置をtableへ渡し、UIへMarkdownを再解釈させない。
   所属と終了境界、markerの専用段落一回表示はADR0071を維持する。

2. `markdown_table_row`の第4引数を、表示kindと`indent_twips`だけを持つ借用入力記述
   `markdown_table_format`にする。引数は4つ以内とし、永続状態やsetter群を作らない。
   構文の`markdown_line.indent`へ表示twipを詰める方法は、単位と意味を混同するため採らない。
   table境界はkind・借用行・delimiter・列数・字下げ範囲を検証し、不整合は既存のINVALID_INPUTで返す。

3. 製品がnative表として出す範囲を、列数1〜63・字下げ0〜32767twipとする。
   32767はこの製品の保守的な対応上限であり、RichEditの絶対上限を宣言する値ではない。
   `markdown_table`の一つの純粋な対応範囲判定を、blocksのnative/literal選択と直接row入力検証で共有する。
   上限を超える字下げも63列超と同じ既存literal表経路へ渡し、元の全行/記号を保持する。
   字下げを黙って0や上限へ丸めず、文書全体の変換失敗にもしない。literalの原文markerと番号の扱いはADR0071と同じ。

4. row開始は`trleft`へ字下げを出し、各`cellx`は字下げ＋従来の初期右端として出す。
   初期幅9000と各列の割算は変えない。列数上限により各幅は142以上、上限字下げでも加算結果は41767以下。
   native表示の実幅はRTFの固定値ではなく既存の投入後/幅変更/倍率変更の配置で確定する。

5. UIの既存`note_pane_table.layout_row`は、native rowの`dxIndent`をpane有効幅から引く。
   負のpane幅または負のnative indentは型付き失敗へ渡し、減算は幅>indentの場合だけ行う。
   それ以外の残幅は0とする。残幅を#405の同じ商/余り式で配り、合計と最大差1を守る。
   更新後は列幅だけでなく行indentと列数もreadbackで照合する。第2の行indexや幅cacheは作らない。
   native row/cellの他属性、raw本文、選択、dirty、focus、非空Undo/Redoは既存の配置経路で保持する。

6. 字下げがpane幅以上の場合は列幅0で構造を保持し、後の幅/倍率変更で同じ行を再配置する。
   この場合と極狭幅の多列表で文字が読める保証は追加しない。最小幅・水平bar・新操作・別viewerを混ぜない。
   原文は保存/編集の正本のままで、RTF表の構造文字を保存しない。64列超や過大字下げのliteralも同じ原文から生成する。

7. APIの入力記述と対応範囲、MARKDOWNの利用者向け制限、正本文書と品質記録を同じ変更で更新する。
   schema・設定・実行時依存・検索・品質ゲートの閾値/除外は変更しない。Waivers: none。

## 実装前の根拠

既存の非製品診断は2列・0/440/480/920twip・600px100%/320px150%でindent/残幅/位置と本文状態の保持を確認した。
新しい境界診断ではtrleft65535まで保持し、残幅0でもraw/選択/dirty/非空Undo/Redoを保持して幅復帰できた。
RTF importは100未満/31680超のcell幅を補正するが、今回の9000/63以上の初期幅はその範囲内にある。
LONG_MAX近傍はstream error0でもnative row queryが失敗したため、整数型の最大値だけを根拠に対応範囲を広げない。
最終v4診断では32767＋9000/2列は4500×2、32767/63列は142×63を正確に保持した。
どちらも残幅0への更新後にindent/列数/raw/選択/dirty/非空Undo/Redoを保持し、29 checks/0 failuresだった。
設計サナが生logとexport/import値を照合してこの対応範囲を受理した。非製品診断は完成時の製品検証とは区別する。
証跡は`D:/NeNeFolio/agents/table-indent-20261029/`と`table-indent-boundary-20261029/`。

## 完成時の検証

対象unitでroot/quote/list/混合container、header/body、table後の復帰、kind/借用入力/列数/字下げの境界、
上限超literalの全原文保持を確認する。変換の確保失敗は既存測定経路を使い公開out不変を確認する。
実製品coreとnative配置関数で代表2列の幅/倍率変更、表の位置と後続段落、raw/選択/dirty/非空Undo/Redoを確認する。
代表画像を目視し、合成入力/専用desktopの確認を物理入力や全OS環境へ広げない。
成功済みで関係入力不変の#404/#405結果を再利用し、最終headの必須CIを統合条件にする。

## 参考

- [TABLEROWPARMS](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-tablerowparms)
- [TABLECELLPARMS](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-tablecellparms)
- [EM_SETTABLEPARMS](https://learn.microsoft.com/en-us/windows/win32/controls/em-settableparms)
