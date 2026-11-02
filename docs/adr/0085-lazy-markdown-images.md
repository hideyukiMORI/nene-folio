# ADR 0085 — Markdown原文を保つ画像の遅延表示と持ち出し

- 状態: 受理（2026-11-02、hideの方向指定と実装依頼。製品編集前）
- 日付: 2026-11-02
- Issue: #520
- 規則: ARC-001〜012、C-001〜018、QLT-008/009/012/013

## 利用者との契約

1. Markdown原文・入力中のパスを自動変更せず、画像も自動移動/取り込みしない。
   相対パスの基準はmd所在directory（現在の保存構造ではカテゴリdirectory）。
   `images/`配下の配置を推奨する。外部ローカル画像の存在は保存/閲覧の区切りで控えめに案内し、
   入力ごとに通知しない。HTTPSは配置案内の対象外。
2. 通常のinline画像構文についてPNG/JPEGを表示する。code内・escapeされた画像印は解釈しない。
   HTTPS画像は外部参照で、恒久的に取り込まない。非HTTPS通信・SVG/動画・参照形式画像は追加しない。
3. 本文を先に表示し、画像は代替アイコンと説明を置いて画面付近から取得する。
   不明寸法の時点で最終高さを保証しない。反映時は本文の先頭可視文字と選択を維持して読み位置の移動を抑える。
   失敗しても本文は使え、再試行できる。ノート/モード変更後の旧結果を新しい本文へ反映しない。
4. 表示用は長辺2048画素以下・縦横比維持・拡大なし。圧縮入力16MiB、元画像64M画素、
   文書の表示画像数256を上限とする。表示上限以後は記法を残す。元画像は不変。
   同じ解析で画像参照を4096件まで収集し、ZIPは表示数を超える分も扱う。ZIP全体の参照数も4096件まで。
   セッションcacheは4件・原bytesと表示bitmapの合計32MiBまで。ローカルは再要求時に原bytesを比較し、
   同じ内容のときだけ縮小済み結果を使う。HTTPSはセッション内のURL結果を再利用する。
   paneが再配置用に保持するbitmapは64MiBまで。RichEditの内部copy・WICの作業領域はこの数字に含めない。
   通常dataやZIPへ表示cacheを含めない。
5. Markdown単票の書き出しは従来の本文だけ。カテゴリ/全ノートZIPは参照する`images/`内のファイルを同梱する。
   それ以外のローカル画像は出力時の明示選択で同梱し、必要な参照調整は出力コピー内だけ。
   HTTPSは常にURLのまま。重複名・同名の異なる実体を上書きしない。読み取れない必要画像は黙って欠落させない。
   ZIPの原子的な新規公開・128MiB/65534件上限・既存出力保持はADR0082の同じ経路を維持する。

## 正典経路と所有者

- 既存のblock/inline/table parserが画像を一度だけ解釈する。`markdown_rtf`はRTFと同時に
  不変の画像参照一覧を生成する。別のMarkdown parser、UIでのMarkdown再解釈、保存用ASTは作らない。
- 画像参照はcoreの型でHTTPS/ローカル・推奨配置・原文内の宛先範囲を表す。
  書き出しも同じ解析結果を使用する。画像ファイルを読むのは型付きportのadapterだけ。
- native表示は同じRichEditの画像blobを使う。最初のRTFに小さな代替画像を置き、
  取得後は`ITextRange2::InsertImage`でその1文字範囲だけ置き換える。全本文を再投入しない。
  metadata順とnativeの画像位置を対応し、置換前後で本文/hidden field文字位置を保つ。
  状態遷移・HWND・TOM・選択・スクロールはUIスレッドだけが扱う。
  hidden markerと直後の画像文字をnative書式でも確認し、原文に同じ文字列があっても誤対応させない。
  幅/倍率変更では保持bitmapを一件ずつ同じInsertImageへ戻し、表セルの内幅は既存native row queryで得る。
- `image_port`を合成rootからapplicationへ与える。要求は不変参照から必要な値を複写する。
  `image_adapter`がroot、要求/結果、Windows通信・画像復号の資源とキャッシュを所有する。
  UIはapplication経由で要求/結果を受け、native位置と画面への反映だけを所有する。
  選択中ノートのカテゴリ移動と全カテゴリ改名で画像基準の改訂を進め、旧要求を取消する。
  改名未完了中は新しい要求を拒み、完了時に再準備する。VIEWだけを再投入し、選択と先頭論理行を戻す。
  EDITの未保存本文/Undoには触れず、画像/パスも移動・変更しない。
  ローカルは通常ドライブのfileだけとし、UNC/ネットワークドライブ/device pathとreparse pointを通る参照を拒む。
  最終file handleとimages directoryを照合し、読込と出力の実体判定を`image_file`へ集約する。

## C-013の限定変更

画像のI/O・WIC復号/縮小だけは`adapters/win32`が所有する単一workerで実行する。
UI/application/coreの状態やHWND/TOMをworkerへ渡さない。コピーした要求と完了結果のみを同期境界で渡す。
同時実行は一件、キュー/結果も有界。取消で旧要求を失効させ、終了ではworkerを回収してからadapterを破棄する。
通信はHTTPSのみ、OSの証明書検証を維持し、通信待機・総時間・bytesを制限する。
WinHTTPは非同期requestを画像workerから開始/終了し、OS callbackはadapter内の通知状態とeventだけを扱う。
request作成後に非NULL contextとHANDLE_CLOSING通知を登録し、最後の通知までbuffer/context/eventを保持する。
UIから通信handleを閉じない。workerが25ms刻みで取消と取得開始後10秒の累積予算を確認し、
予算を超えたらrequestのcloseを開始する。10秒は取消開始の予算であり、OSの最終通知/資源回収の厳密な期限ではない。
少量ずつ届くheadersは同期受信の段階timeoutだけでは総時間を制限できず、同期handleの別threadからのcloseも
競合を生むため採らない。[WinHttpCloseHandle](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpclosehandle)。
固定5引数のWINHTTP_STATUS_CALLBACKだけはC-012の引数4上限を超える。
WVR-0001をこの単関数へ限定し、callbackは通知の受渡しだけ・60行/ネスト3以内を維持する。
この許可を検索や他の機能の背景化へ広げない。入れ子message pumpは引き続き禁止する。

根拠となる製品編集前の診断:

- `D:/NeNeFolio/outputs/520-markdown-images/native-probe-09`:
  実Msfteditで23checks成功。PNG代替画像→BMP streamの画像差替え、native raw/hidden field不変、
  選択保持、リンクURL保持、200×100相当の画像行高を確認。
  読取専用のままではRTF画像が落ち、InsertImageはE_ACCESSDENIEDとなる。
  反映の同期区間だけreadonlyを解除し、Undoを停止/復帰し、modifiedを戻す必要がある。
  `GetURL`はrangeを拡張するため、置換前に厳密な1文字rangeへ戻す。
- `wic-probe-01`: 285799bytesの合成8192×8192 PNGをWICで2048×2048 BGRAへ復号/縮小する同期呼出しが125ms。
  圧縮bytesの小ささだけではUI占有時間を制限できない。この操作をUI idleへ置く案は採らない。
- `native-probe-01`のSDK呼出し表記エラー、02の失敗時stdout未flush、03〜08の診断途中結果も保持する。
  OLEオブジェクト数は画像blob数ではなく、初期の判定には使えなかった。

OS同梱WinHTTP・WICをadapterの宣言済みplatformLibrariesへ加える。配布する実行時依存は増やさない。
ADR0002/0070の画像埋込み非要件・ネットワーク取得なしを上記範囲に限り置き換える。

## 検証

構文境界・原文保持・上限/確保失敗をcore単体、取消/旧結果拒否をapplicationの偽portで固定する。
実RichEditの位置/画像/縮小/リンク/検索/表/ミニマップ、遅延中の入力、モード変更/終了を対象native検証で確認する。
実ファイル/HTTPS取得・欠損/不正/大画像・ZIP独立読取と全元hash保持を隔離fixtureで確認する。
成功済み検証は入力が変わらなければ再利用する。物理IME・実reader・全OS保証とは区別する。

一次資料: [InsertImage](https://learn.microsoft.com/en-us/windows/win32/api/tom/nf-tom-itextrange2-insertimage)、
[WIC縮小](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-bitmapsources-howto-scale)。
Waiver: [WVR-0001](../waivers/WVR-0001-winhttp-callback-abi.md)（WinHTTPの固定5引数callbackだけ）。
