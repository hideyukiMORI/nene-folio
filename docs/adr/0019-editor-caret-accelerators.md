# ADR 0019 — 編集本文のCtrl＋hjklをRichEdit自身の移動へ結ぶ

- 状態: 受理
- 日付: 2026-09-13
- Issue: #51
- 規則: ARC-001 / ARC-002 / ARC-004 / ARC-005 / ARC-007 / ARC-011、C-002 / C-005 / C-006 / C-012 / C-017、QLT-012 / QLT-013

## 文脈

hideのHHKBでは左Altが英字直接入力で、Alt自体はマウスとの組合せになっている。
編集中の移動をCtrl+h/j/k/lで行う案について続行指示を受けた。WM_CHARだけでCtrl+Hを受けるとBackspaceと同じ文字値になる。
Ctrlを押したまま矢印のキー通知へ置き換えるだけでは、RichEditがCtrl＋矢印として扱う可能性がある。

## 決定

1. 編集本文のCtrl+h/j/k/lを左／下／上／右に割り当てる。通常の文字、Backspace、Enter、索引や入力面、閲覧中は従来の処理へ渡す。
   Shift/Altを併用する組合せは追加割当の対象外。Ctrl+Hによる置換は採用しない。
2. 合成ルートのメッセージループでTranslateMessageの前にfolio_windowへ翻訳を依頼する。
   主窓は宛先が本文、modeがEDIT、IME変換中ではないことを確認し、note_paneのTranslateAcceleratorWへ渡す。
   処理済みなら元の鍵を再翻訳・dispatchしない。修飾キーの状態を自己追跡せず、GetKeyStateやSetKeyboardStateも使わない。
3. note_paneは一つのキーと方向の登録表からHACCELを作る。アクセラレータ由来のWM_COMMANDだけを、同じ表で検証して閉じたcaret_commandへ写す。
   キャレットと選択範囲は既存RichEditが唯一の所有者であり、applicationに状態・操作IDを増やさない。通常の矢印と同じく本文編集部品の操作である。
4. RichEditのITextSelectionを使い、左右はMoveLeft/Right(tomCharacter,1,tomMove)、上下はMoveUp/Down(tomLine,1,tomMove)。
   UTF-16位置・折り返し・選択の折畳みを独自実装せず、本文の再設定・保存・Undo履歴の操作を行わない。
   端で動かない場合は正常。TOMの失敗HRESULTは警告音とし、元のキーを文字入力として再実行しない。
5. note_paneがHACCELとITextSelectionの参照を所有する。EM_GETOLEINTERFACEとQueryInterface/GetSelectionで取得し、中間参照は取得関数内で解放する。
   QueryInterfaceのvoid**は既存CreateDIBSectionと同じWin32境界に限り受け、直後にSDKの型へ変換する。業務状態へvoid*を持ち込まない。
   destroyで選択参照とHACCELを解放し、Msftedit.dllを最後に解放する。準備失敗は既存NOTE_PANE_NOT_CREATEDで起動失敗を知らせる。
6. TOMは既存Msftedit.dllの機能。ITextDocumentのGUIDはSDKのtom.hとMicrosoftの「Use TOM GUIDs」の値を、note_pane内のconst IID一か所で定義する。
   固定SDKのuuid.libではIID_ITextDocumentが未解決になることをビルドで確認したため、不要なuuidのリンク追加は行わない。
   外部実行時依存・プラットフォームライブラリ・ポート・保存スキーマ・ゲートの閾値や除外は増やさない。
7. 画面のキー説明と操作表へ対応を記載する。項目追加時は最小窓で候補の高さを確保する。新規・保存・改名・検索は別単位。

## 検証と根拠

ビルド／静的解析／単体と、Win32部品の測定および実キー・IMEの実機確認を区別する。
Backspace/Enterと修飾なしのhjkl、本文不変とUndo、選択あり・行折返し・端の移動、IMEと他区画の除外を確認する。

- [TranslateAcceleratorW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translateacceleratorw): 処理済みならTranslateMessageへ渡さない。WM_COMMANDのHIWORDは1。
- [ITextSelection](https://learn.microsoft.com/en-us/windows/win32/api/tom/nn-tom-itextselection): UnitとExtendで通常の矢印相当の移動を指定する。
- [MoveDown](https://learn.microsoft.com/en-us/windows/win32/api/tom/nf-tom-itextselection-movedown): tomLineは下矢印、tomMoveは選択を折り畳む。
- [Use TOM GUIDs](https://learn.microsoft.com/en-us/windows/win32/controls/use-tom-guids): ITextDocumentのGUID定義。

Waivers: none
