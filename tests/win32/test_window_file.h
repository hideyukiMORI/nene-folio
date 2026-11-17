/* #894: the real-window tests keep the per-machine window file (ADR0108) in memory, so running
 * them never writes `nenefolio-window.json` beside an executable a person uses. */
#ifndef NENEFOLIO_TEST_WINDOW_FILE_H
#define NENEFOLIO_TEST_WINDOW_FILE_H
#include "storage_port.h"
/* Replaces the port's window file with one held in memory. It starts as "open at the initial
 * place, nothing recorded", so every test window opens the same way unless a test records one. */
void test_window_file_attach(struct storage_port *_Nonnull port);
#endif
