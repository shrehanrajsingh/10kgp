#ifndef WIN_HEADER_H
#define WIN_HEADER_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef _SSIZE_T_DEFINED
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif

#endif
