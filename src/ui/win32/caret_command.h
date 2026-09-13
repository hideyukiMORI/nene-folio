/* RichEditが所有するキャレットの移動。文字入力や業務の保存操作とは分ける（ADR 0019）。 */
#ifndef NENEFOLIO_CARET_COMMAND_H
#define NENEFOLIO_CARET_COMMAND_H

enum caret_command : unsigned short
{
    CARET_COMMAND_LEFT = 1,
    CARET_COMMAND_DOWN,
    CARET_COMMAND_UP,
    CARET_COMMAND_RIGHT
};

#endif
