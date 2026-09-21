/* 登録表の操作が受ける引数の種類（ADR 0026 の決定 8）。
 * takes_name() の真偽を一般化したもので、閉じた集合なので操作が増えたら switch が落ちる。 */
#ifndef NENEFOLIO_FOLIO_ARGUMENT_KIND_H
#define NENEFOLIO_FOLIO_ARGUMENT_KIND_H

enum folio_argument_kind : unsigned char
{
    FOLIO_ARGUMENT_NONE,   /* 引数を取らない。余計な語があれば操作として解けない */
    FOLIO_ARGUMENT_NAME,   /* ノート名（途中と末尾の空白を保ち、名前型で検証する・ADR 0020） */
    FOLIO_ARGUMENT_OPTION, /* 設定の語（空白で切った 1 語の完全一致・folio_command_parse_option） */
    /* `/パターン/置換/[g]`（区切りで分ける・folio_command_parse_substitute・ADR 0028 の決定 8(b)）
     */
    FOLIO_ARGUMENT_SUBSTITUTE
};

#endif
