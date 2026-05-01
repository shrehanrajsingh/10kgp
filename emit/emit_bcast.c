#include "emit_bcast.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#else

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>

#endif

static void
push_unique (struct sockaddr_in *out, size_t *n, size_t max_out, uint32_t addr_be)
{
  size_t i;

  if (*n >= max_out || addr_be == 0)
    return;
  for (i = 0; i < *n; i++)
    if (out[i].sin_addr.s_addr == addr_be)
      return;
  memset (&out[*n], 0, sizeof (out[*n]));
  out[*n].sin_family = AF_INET;
  out[*n].sin_addr.s_addr = addr_be;
  (*n)++;
}

size_t
emit_bcast_collect (struct sockaddr_in *out, size_t max_out)
{
  size_t n = 0;

  if (max_out == 0)
    return 0;

#ifdef _WIN32

  {
    MIB_IPADDRTABLE *table = NULL;
    ULONG sz = 0;
    DWORD err;

    err = GetIpAddrTable (NULL, &sz, FALSE);
    if (err != ERROR_INSUFFICIENT_BUFFER)
      goto limited_only;

    table = (MIB_IPADDRTABLE *)malloc (sz);
    if (!table)
      goto limited_only;

    if (GetIpAddrTable (table, &sz, FALSE) != NO_ERROR)
      {
        free (table);
        goto limited_only;
      }

    for (DWORD i = 0; i < table->dwNumEntries; i++)
      {
        MIB_IPADDRROW *row = &table->table[i];
        uint32_t addr_be = row->dwAddr;
        uint32_t mask_be = row->dwMask;
        uint32_t host_a = ntohl (addr_be);
        uint32_t bcast_be;

        if (addr_be == 0)
          continue;
        /* Skip 127.0.0.0/8 */
        if ((host_a & 0xff000000u) == 0x7f000000u)
          continue;

        if (row->dwBCastAddr != 0)
          bcast_be = row->dwBCastAddr;
        else
          {
            uint32_t mask_h = ntohl (mask_be);
            if (mask_h == 0 || mask_h == 0xffffffffu)
              continue;
            bcast_be = htonl (host_a | (~mask_h & 0xffffffffu));
          }

        push_unique (out, &n, max_out, bcast_be);
      }

    free (table);
  }

limited_only:
  push_unique (out, &n, max_out, htonl (INADDR_BROADCAST));

#else

  {
    struct ifaddrs *ifa_list = NULL;
    struct ifaddrs *ifa;

    if (getifaddrs (&ifa_list) != 0)
      goto posix_limited;

    for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next)
      {
        uint32_t addr_be;
        uint32_t bcast_be = 0;

        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
          continue;
        if (ifa->ifa_flags & IFF_LOOPBACK)
          continue;

        addr_be = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;

        if (ifa->ifa_broadaddr)
          bcast_be = ((struct sockaddr_in *)ifa->ifa_broadaddr)->sin_addr.s_addr;
        else if (ifa->ifa_netmask)
          {
            uint32_t mask_be = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
            uint32_t mask_h = ntohl (mask_be);
            uint32_t host_a = ntohl (addr_be);

            if (mask_h == 0 || mask_h == 0xffffffffu)
              continue;
            bcast_be = htonl (host_a | (~mask_h & 0xffffffffu));
          }

        if (bcast_be != 0)
          push_unique (out, &n, max_out, bcast_be);
      }

    freeifaddrs (ifa_list);
  }

posix_limited:
  push_unique (out, &n, max_out, htonl (INADDR_BROADCAST));

#endif

  /* Guaranteed limited broadcast if stack permits */
  if (n == 0)
    push_unique (out, &n, max_out, htonl (INADDR_BROADCAST));

  return n;
}
