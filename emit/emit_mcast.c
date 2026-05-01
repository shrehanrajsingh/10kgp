#include "emit_mcast.h"
#include "emit.h"
#include "emit_bcast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#endif

static uint32_t g_m_if_addr[EMIT_BCAST_MAX_TARGETS];
static size_t g_m_if_n;

static uint32_t
group_addr_be (void)
{
  uint32_t g = inet_addr (EMIT_MULTICAST_GROUP);
  return g;
}

void
emit_mcast_dest (struct sockaddr_in *dst, uint16_t port_host_order)
{
  memset (dst, 0, sizeof (*dst));
  dst->sin_family = AF_INET;
  dst->sin_port = htons (port_host_order);
  dst->sin_addr.s_addr = group_addr_be ();
}

static void
mem_reset_join (void)
{
  memset (g_m_if_addr, 0, sizeof g_m_if_addr);
  g_m_if_n = 0;
}

static void
push_join_if (uint32_t if_be)
{
  size_t i;

  if (g_m_if_n >= EMIT_BCAST_MAX_TARGETS)
    return;
  for (i = 0; i < g_m_if_n; i++)
    if (g_m_if_addr[i] == if_be)
      return;
  g_m_if_addr[g_m_if_n++] = if_be;
}

#ifdef _WIN32

int
emit_mcast_join (emit_sock_t sock)
{
  uint32_t group_be = group_addr_be ();
  MIB_IPADDRTABLE *table = NULL;
  ULONG sz = 0;
  DWORD err;

  mem_reset_join ();
  if (group_be == INADDR_NONE)
    return -1;

  err = GetIpAddrTable (NULL, &sz, FALSE);
  if (err != ERROR_INSUFFICIENT_BUFFER)
    goto fallback_any;

  table = (MIB_IPADDRTABLE *)malloc (sz);
  if (!table)
    goto fallback_any;

  if (GetIpAddrTable (table, &sz, FALSE) != NO_ERROR)
    {
      free (table);
      goto fallback_any;
    }

  for (DWORD i = 0; i < table->dwNumEntries; i++)
    {
      MIB_IPADDRROW *row = &table->table[i];
      uint32_t addr_be = row->dwAddr;
      uint32_t host_a = ntohl (addr_be);

      if (addr_be == 0 || (host_a & 0xff000000u) == 0x7f000000u)
        continue;

      struct ip_mreq mr;
      memset (&mr, 0, sizeof (mr));
      mr.imr_multiaddr.s_addr = group_be;
      mr.imr_interface.s_addr = addr_be;

      if (setsockopt (sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mr,
                      sizeof (mr))
          == 0)
        push_join_if (addr_be);
    }

  free (table);

fallback_any:
  if (g_m_if_n == 0)
    {
      struct ip_mreq mr;
      memset (&mr, 0, sizeof (mr));
      mr.imr_multiaddr.s_addr = group_be;
      mr.imr_interface.s_addr = htonl (INADDR_ANY);

      if (setsockopt (sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mr,
                      sizeof (mr))
          == 0)
        push_join_if (htonl (INADDR_ANY));
    }

  return g_m_if_n > 0 ? 0 : -1;
}

void
emit_mcast_leave (emit_sock_t sock)
{
  uint32_t group_be = group_addr_be ();

  if (group_be == INADDR_NONE || g_m_if_n == 0)
    {
      mem_reset_join ();
      return;
    }

  for (size_t i = 0; i < g_m_if_n; i++)
    {
      struct ip_mreq mr;
      memset (&mr, 0, sizeof (mr));
      mr.imr_multiaddr.s_addr = group_be;
      mr.imr_interface.s_addr = g_m_if_addr[i];
      setsockopt (sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&mr,
                  sizeof (mr));
    }
  mem_reset_join ();
}

#else

int
emit_mcast_join (emit_sock_t sock)
{
  uint32_t group_be = group_addr_be ();
  struct ip_mreq mr;

  mem_reset_join ();
  if (group_be == INADDR_NONE)
    return -1;

  struct ifaddrs *ifa_list = NULL;

  if (getifaddrs (&ifa_list) != 0)
    goto fallback_any;

  for (struct ifaddrs *ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
        continue;
      if (ifa->ifa_flags & IFF_LOOPBACK)
        continue;

      uint32_t addr_be
          = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;

      memset (&mr, 0, sizeof (mr));
      mr.imr_multiaddr.s_addr = group_be;
      mr.imr_interface.s_addr = addr_be;

      if (setsockopt (sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr)
          == 0)
        push_join_if (addr_be);
    }

  freeifaddrs (ifa_list);

fallback_any:
  if (g_m_if_n == 0)
    {
      memset (&mr, 0, sizeof (mr));
      mr.imr_multiaddr.s_addr = group_be;
      mr.imr_interface.s_addr = htonl (INADDR_ANY);

      if (setsockopt (sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr)
          == 0)
        push_join_if (htonl (INADDR_ANY));
    }

  return g_m_if_n > 0 ? 0 : -1;
}

void
emit_mcast_leave (emit_sock_t sock)
{
  uint32_t group_be = group_addr_be ();
  struct ip_mreq mr;

  if (group_be == INADDR_NONE || g_m_if_n == 0)
    {
      mem_reset_join ();
      return;
    }

  for (size_t i = 0; i < g_m_if_n; i++)
    {
      memset (&mr, 0, sizeof (mr));
      mr.imr_multiaddr.s_addr = group_be;
      mr.imr_interface.s_addr = g_m_if_addr[i];
      setsockopt (sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mr, sizeof mr);
    }
  mem_reset_join ();
}

#endif
