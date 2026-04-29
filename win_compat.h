#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

#ifdef _WIN32
#include <io.h>
#define app_unlink _unlink
#else
#include <unistd.h>
#define app_unlink unlink
#endif

#endif
