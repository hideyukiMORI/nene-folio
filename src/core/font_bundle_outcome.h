/* 同梱の書体を登録した結果（C-005 / ADR 0036 の決定 3）。登録は adapters/win32 の font_bundle が
 * 行い、判定は core の font_bundle_verdict が決める。窓（ui）が起動時に知らせるかを決めるのにも
 * 使うので、adapters ではなく core に置く（ui は adapters に依存できない・ARC-002）。 */
#ifndef NENEFOLIO_FONT_BUNDLE_OUTCOME_H
#define NENEFOLIO_FONT_BUNDLE_OUTCOME_H

enum font_bundle_outcome : unsigned char
{
    /* 見つけた書体のファイルをすべて登録した */
    FONT_BUNDLE_READY,
    /* exe の隣の fonts\ が無いか、*.otf / *.ttf が 1 つも無い */
    FONT_BUNDLE_MISSING,
    /* 1 つ以上を登録できなかった。登録できた分は残す */
    FONT_BUNDLE_PARTIAL,
    /* 登録を覚える入れ物を確保できなかった。登録した分は戻した */
    FONT_BUNDLE_NO_MEMORY
};

#endif
