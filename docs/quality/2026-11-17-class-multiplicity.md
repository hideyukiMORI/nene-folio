# クラス図の多重度の品質記録 — #916

2026-11-17。開始main `b451e75`（#914統合後）。ADR0107の補足。クラス図の関係の両端の多重度（`A "1" --> "*" B`）を読んで表示する。
作業木E:/NeNeFolio/worktrees/916-class-multiplicity（branch feat/916-class-multiplicity）、対象実行の原本E:/NeNeFolio/outputs/916-class-multiplicity、画像D:/NeNeFolio/agents/916-class-multiplicity。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 変更

- `diagram_link`に`start_text` / `end_text`。`diagram_model_link`が文字の番号を確かめる。既存の3つの読み取り（フローチャート・状態遷移図・ER図）は空の文字を入れる。
- `mermaid_class`: 線の前後の引用符の中を読む（`multiplicity`）。これまで未対応の案内にしていた入力が図になる。
- 配置（`diagram_flow_layout`）: 端の文字の位置（`end_text_centre`）、端に文字のある線を横に並べる（`spread_end`、間隔に文字の幅を足す`end_text_width`）、余白（`measure_end_text`）。
- 描画の変更なし（既存の文字の描画）。

## 試したことと結果

- 最初は段の間隔の単位を文字2行ぶんに広げた。画像で見ると、多重度の無い線まで縦に間延びした（render-r2）。外しても両端の文字は重ならなかった（render-r3）ので外した。
  重ならないことは単体試験（1段の関係で、始点側の文字の下端 ≤ 終点側の文字の上端）で固定した。
- 試験コードで`require`の後のポインタを静的解析が空と見たので、見つからなければ失敗で終わる補助（`first_path`）にした。
- Bashのheredocで書いたPythonが`\n`を潰して置換に失敗した（既知の罠）。Writeで書き直した。失敗した回の結果は使っていない。

## 検証

- 単体（`class_multiplicity`）: 両端とラベルと印の読み取り、片方だけ、線に詰めて書いた形、文字の位置（端のすぐ横・図の内側・両端が重ならない）、端に文字のある矢印が横に並ぶこと。
  拒否: 閉じない引用符・空・同じ側に2つ・終点側の後にクラスが無い。
- 描画（`diagram_render_tests`）: クラス図の見本に多重度を足し、明暗の画像を目で確認（D:/NeNeFolio/agents/916-class-multiplicity/class-light.png, class-dark.png）。
- 対象検証（v1）の結果は下の「結果」。

## 結果

- v1（実装と最初の試験の時点）: `build=0 coverage=0 symbols=0 conformance=0 conformanceBuild=0 ctest=0 diffcheck=0`。ctest 63件すべて成功、分岐カバレッジ91.21%（11377/12474）。
- v1の後に単体試験を1つ足した（`class_multiplicity_sideways`: 左右向きと、上へ戻る関係で、文字が図の内側に収まる）。足した後はビルドと対象の3試験（folio_unit・diagram_render・diagram_status）だけを再実行して成功。全体は最終headの必須CIで確かめる。

## 残るリスク

- 端の文字は地を塗らないので、線の込んだ図では他の線と重なって読みにくいことがある。
- 左右向き（`direction LR`）では文字は線の下側に出る。図の内側に収まることは単体試験で確かめたが、画像で見たのは上下向きだけ。
- 迂回する線（下から上へ戻る関係）の端の文字は、図の内側に収まることだけを確かめた。他の線や箱との重なりは見ていない。

Waivers: none。
