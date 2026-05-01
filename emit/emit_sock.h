#ifndef EMIT_SOCK_H
#define EMIT_SOCK_H

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET emit_sock_t;
#else
typedef int emit_sock_t;
#endif

#endif
