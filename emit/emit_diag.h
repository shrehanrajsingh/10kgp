#ifndef EMIT_DIAG_H
#define EMIT_DIAG_H

#include <stdlib.h>

static inline int
emit_diag_trace (void)
{
  const char *e = getenv ("10KGDP_TRACE");
  return e != NULL && e[0] != '\0' && e[0] != '0';
}

#endif
