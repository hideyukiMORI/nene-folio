/* text_buffer の生成と完了の結果（C-005）。 */
#ifndef NENEFOLIO_TEXT_BUFFER_OUTCOME_H
#define NENEFOLIO_TEXT_BUFFER_OUTCOME_H

enum text_buffer_outcome : unsigned char
{
    TEXT_BUFFER_ACCEPTED,
    TEXT_BUFFER_OUT_OF_MEMORY
};

#endif
