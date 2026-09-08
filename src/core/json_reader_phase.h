/* json_reader の文法上の位置。閉じた集合なので enum で持つ（C-002）。 */
#ifndef NENEFOLIO_JSON_READER_PHASE_H
#define NENEFOLIO_JSON_READER_PHASE_H

enum json_reader_phase : unsigned char
{
    JSON_READER_PHASE_VALUE,       /* 値を待つ。配列が空なら ] も許す */
    JSON_READER_PHASE_KEY,         /* キーを待つ。オブジェクトが空なら } も許す */
    JSON_READER_PHASE_AFTER_VALUE, /* , か閉じ括弧を待つ */
    JSON_READER_PHASE_DONE,        /* 最上位の値を読み終えた。空白と終端だけ許す */
    JSON_READER_PHASE_ENDED,       /* 終端を返した */
    JSON_READER_PHASE_FAILED,      /* 不正または記憶不足。以後は同じ字句を返す */
};

#endif
