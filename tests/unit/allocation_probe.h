/* 確保失敗の注入（QLT-009 / C-016）。eng/coverage.py の測定ビルドは core / application を
 * -Dmalloc=folio_probe_malloc -Dcalloc=folio_probe_calloc -Drealloc=folio_probe_realloc で
 * コンパイルし、中核の確保をここへ向ける。正典のビルドでは中核は本物の CRT を呼び、この 3 関数は
 * 誰からも呼ばれない。製品コードにテスト用の窓口は無い（C-008）。 */
#ifndef NENEFOLIO_ALLOCATION_PROBE_H
#define NENEFOLIO_ALLOCATION_PROBE_H

#include <stddef.h>

/* これから数えて nth 回目の確保を失敗させる。0 で解除。 */
void allocation_probe_fail_at(size_t nth);

void *_Nullable folio_probe_malloc(size_t size);
void *_Nullable folio_probe_calloc(size_t count, size_t size);
void *_Nullable folio_probe_realloc(void *_Nullable block, size_t size);

#endif
