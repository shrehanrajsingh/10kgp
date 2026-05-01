#include "emit.h"
#include "emit_bcast.h"
#include "emit_diag.h"
#include "emit_lan_scan.h"
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
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#endif

#ifndef _WIN32
#include <time.h>
#else
#include <windows.h>
#endif

#define EMIT_LAN_DEDUP_CAP 1024

static void
sweep_throttle (void)
{
#ifndef _WIN32
  struct timespec ts = { 0, 100000L }; /* 100 µs: avoids bursting peers' UDP queues */
  (void)nanosleep (&ts, NULL);
#else
  Sleep (0);
#endif
}

static int
dedup_push (uint32_t *tab, size_t *n, uint32_t addr_be)
{
  size_t i;

  if (*n >= EMIT_LAN_DEDUP_CAP)
    return 0;
  for (i = 0; i < *n; i++)
    if (tab[i] == addr_be)
      return 0;
  tab[(*n)++] = addr_be;
  return 1;
}

static int
is_local_addr (const uint32_t *locals, size_t nloc, uint32_t cand_be)
{
  size_t i;

  for (i = 0; i < nloc; i++)
    if (locals[i] == cand_be)
      return 1;
  return 0;
}

#ifdef _WIN32

size_t
emit_lan_sweep_send (emit_sock_t sock, uint16_t port_host_order,
                     const char *pkt, size_t pkt_len)
{
  uint32_t dedup[EMIT_LAN_DEDUP_CAP];
  uint32_t locals[64];
  size_t nd = 0;
  size_t nloc = 0;
  size_t ok = 0;
  MIB_IPADDRTABLE *table = NULL;
  ULONG sz = 0;

  memset (dedup, 0, sizeof dedup);
  memset (locals, 0, sizeof locals);

  if (GetIpAddrTable (NULL, &sz, FALSE) != ERROR_INSUFFICIENT_BUFFER)
    return 0;

  table = (MIB_IPADDRTABLE *)malloc (sz);
  if (!table)
    return 0;

  if (GetIpAddrTable (table, &sz, FALSE) != NO_ERROR)
    {
      free (table);
      return 0;
    }

  for (DWORD i = 0; i < table->dwNumEntries; i++)
    {
      uint32_t addr_be = table->table[i].dwAddr;
      uint32_t host_a = ntohl (addr_be);

      if (addr_be == 0 || (host_a & 0xff000000u) == 0x7f000000u)
        continue;
      if (nloc < 64)
        locals[nloc++] = addr_be;
    }

  for (DWORD i = 0; i < table->dwNumEntries; i++)
    {
      MIB_IPADDRROW *row = &table->table[i];
      uint32_t addr_be = row->dwAddr;
      uint32_t mask_be = row->dwMask;
      uint32_t mask_h = ntohl (mask_be);
      uint32_t inv = (~mask_h) & 0xffffffffu;

      if (addr_be == 0 || inv == 0 || inv > 254u)
        continue;

      uint32_t addr_h = ntohl (addr_be);
      uint32_t net_h = addr_h & mask_h;
      uint32_t bc_h = net_h | inv;

      for (uint32_t ip_h = net_h + 1; ip_h < bc_h; ip_h++)
        {
          uint32_t ip_be = htonl (ip_h);

          if (is_local_addr (locals, nloc, ip_be))
            continue;
          if (!dedup_push (dedup, &nd, ip_be))
            continue;

          struct sockaddr_in dst = { 0 };
          dst.sin_family = AF_INET;
          dst.sin_port = htons (port_host_order);
          dst.sin_addr.s_addr = ip_be;

          if (sendto (sock, pkt, (int)pkt_len, 0, (struct sockaddr *)&dst,
                      sizeof (dst))
              != SOCKET_ERROR)
            ok++;
          sweep_throttle ();
        }
    }

  free (table);

  if (emit_diag_trace ())
    fprintf (stderr, "10kgp: LAN sweep sent %zu unicast probes\n", ok);

  return ok;
}

#else

size_t
emit_lan_sweep_send (emit_sock_t sock, uint16_t port_host_order,
                     const char *pkt, size_t pkt_len)
{
  uint32_t dedup[EMIT_LAN_DEDUP_CAP];
  uint32_t locals[64];
  size_t nd = 0;
  size_t nloc = 0;
  size_t ok = 0;
  struct ifaddrs *ifa_list = NULL;

  memset (dedup, 0, sizeof dedup);
  memset (locals, 0, sizeof locals);

  if (getifaddrs (&ifa_list) != 0)
    return 0;

  for (struct ifaddrs *ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
        continue;
      if (ifa->ifa_flags & IFF_LOOPBACK)
        continue;

      uint32_t addr_be = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
      if (nloc < 64)
        locals[nloc++] = addr_be;
    }

  for (struct ifaddrs *ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
        continue;
      if (ifa->ifa_flags & IFF_LOOPBACK)
        continue;
      if (!ifa->ifa_netmask)
        continue;

      uint32_t addr_be = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
      uint32_t mask_be = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
      uint32_t mask_h = ntohl (mask_be);
      uint32_t inv = (~mask_h) & 0xffffffffu;

      if (inv == 0 || inv > 254u)
        continue;

      uint32_t addr_h = ntohl (addr_be);
      uint32_t net_h = addr_h & mask_h;
      uint32_t bc_h = net_h | inv;

      for (uint32_t ip_h = net_h + 1; ip_h < bc_h; ip_h++)
        {
          uint32_t ip_be = htonl (ip_h);

          if (is_local_addr (locals, nloc, ip_be))
            continue;
          if (!dedup_push (dedup, &nd, ip_be))
            continue;

          struct sockaddr_in dst = { 0 };
          dst.sin_family = AF_INET;
          dst.sin_port = htons (port_host_order);
          dst.sin_addr.s_addr = ip_be;

          ssize_t s = sendto (sock, pkt, pkt_len, 0, (struct sockaddr *)&dst,
                              sizeof (dst));
          if (s >= 0)
            ok++;
          sweep_throttle ();
        }
    }

  freeifaddrs (ifa_list);

  if (emit_diag_trace ())
    fprintf (stderr, "10kgp: LAN sweep sent %zu unicast probes\n", ok);

  return ok;
}

#endif
