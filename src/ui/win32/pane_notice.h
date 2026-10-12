/* 右ペイン下端の 1 行の印（ADR 0039 の補正 4・補正 5）。立て下げと寿命は主窓が持つ。 */
#ifndef NENEFOLIO_PANE_NOTICE_H
#define NENEFOLIO_PANE_NOTICE_H

enum pane_notice : unsigned char
{
    PANE_NOTICE_NONE,
    /* ノートをごみ箱へ送り終えた（ADR 0039 の補正 4） */
    PANE_NOTICE_TRASH_COMPLETED,
    /* 編集のまま編集で保てないノートへ切り替え、閲覧で開いた（#242・#258） */
    PANE_NOTICE_NOTE_UNEDITABLE
};

#endif
