/* 自分で読める本文・台帳・復旧記録の共通byte上限（#227 / ADR0006）。 */
#ifndef NENEFOLIO_PERSISTED_SIZE_LIMIT_H
#define NENEFOLIO_PERSISTED_SIZE_LIMIT_H

#include <stddef.h>

constexpr size_t persisted_size_limit = 16 * 1024 * 1024;

#endif
