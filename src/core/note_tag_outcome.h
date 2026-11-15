#ifndef NENEFOLIO_NOTE_TAG_OUTCOME_H
#define NENEFOLIO_NOTE_TAG_OUTCOME_H
enum note_tag_outcome : unsigned char
{
    NOTE_TAG_READY,
    NOTE_TAG_EMPTY,
    NOTE_TAG_INVALID,
    NOTE_TAG_TOO_LONG,
    NOTE_TAG_OUT_OF_MEMORY
};
#endif
