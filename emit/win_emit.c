#include "emit.h"
#include "emit_bcast.h"
#include "win_emit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SOCKET g_sock = INVALID_SOCKET;
static uint16_t g_port = 0;
static char g_peer_id[EMIT_PEER_ID_LEN] = { 0 };
static int g_wsa_refcount = 0;

static void
emit_sock_err (const char *msg, int wsa)
{
  fprintf (stderr, "%s: winsock error %d\n", msg, wsa);
}

static void
map_wsa_to_errno (int wsa)
{
  switch (wsa)
    {
    case WSAEWOULDBLOCK:
      errno = EWOULDBLOCK;
      break;
    case WSAEINTR:
      errno = EINTR;
      break;
    case WSAENOTSOCK:
      errno = ENOTSOCK;
      break;
    case WSAEINVAL:
      errno = EINVAL;
      break;
    default:
      errno = EIO;
      break;
    }
}

int
emit_init (uint16_t port)
{
  g_port = port;

  if (g_wsa_refcount == 0)
    {
      WSADATA wsa;
      if (WSAStartup (MAKEWORD (2, 2), &wsa) != 0)
        {
          fprintf (stderr, "WSAStartup failed\n");
          return -1;
        }
    }

  g_sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (g_sock == INVALID_SOCKET)
    {
      int w = WSAGetLastError ();
      emit_sock_err ("socket", w);
      if (g_wsa_refcount == 0)
        WSACleanup ();
      return -1;
    }

  int broadcast = 1;
  if (setsockopt (g_sock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast,
                  sizeof (broadcast))
      == SOCKET_ERROR)
    {
      int w = WSAGetLastError ();
      emit_sock_err ("setsockopt SO_BROADCAST", w);
      closesocket (g_sock);
      g_sock = INVALID_SOCKET;
      if (g_wsa_refcount == 0)
        WSACleanup ();
      return -1;
    }

  int reuse = 1;
  if (setsockopt (g_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
                  sizeof (reuse))
      == SOCKET_ERROR)
    {
      int w = WSAGetLastError ();
      emit_sock_err ("setsockopt SO_REUSEADDR", w);
      closesocket (g_sock);
      g_sock = INVALID_SOCKET;
      if (g_wsa_refcount == 0)
        WSACleanup ();
      return -1;
    }

  struct sockaddr_in bind_addr = { 0 };
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = htonl (INADDR_ANY);
  bind_addr.sin_port = htons (port);

  if (bind (g_sock, (struct sockaddr *)&bind_addr, sizeof (bind_addr))
      == SOCKET_ERROR)
    {
      int w = WSAGetLastError ();
      emit_sock_err ("bind", w);
      closesocket (g_sock);
      g_sock = INVALID_SOCKET;
      if (g_wsa_refcount == 0)
        WSACleanup ();
      return -1;
    }

  g_wsa_refcount++;
  return 0;
}

void
emit_set_peer_id (const char *id)
{
  strncpy (g_peer_id, id ? id : "", EMIT_PEER_ID_LEN - 1);
  g_peer_id[EMIT_PEER_ID_LEN - 1] = '\0';
}

int
emit_pulse (void)
{
  if (g_sock == INVALID_SOCKET)
    {
      errno = ENOTCONN;
      return -1;
    }

  char packet[EMIT_PACKET_SIZE] = { 0 };
  strncpy (packet, g_peer_id, sizeof (packet) - 1);

  struct sockaddr_in targets[EMIT_BCAST_MAX_TARGETS];
  size_t nt = emit_bcast_collect (targets, EMIT_BCAST_MAX_TARGETS);
  int any_ok = 0;

  for (size_t i = 0; i < nt; i++)
    {
      targets[i].sin_port = htons (g_port);
      int sent = sendto (g_sock, packet, (int)strlen (packet), 0,
                         (struct sockaddr *)&targets[i],
                         sizeof (targets[i]));
      if (sent != SOCKET_ERROR)
        any_ok = 1;
    }

  if (!any_ok)
    {
      int w = WSAGetLastError ();
      emit_sock_err ("sendto", w);
      map_wsa_to_errno (w);
      return -1;
    }

  return 0;
}

int
emit_listen (peer_callback_t callback, void *user_data, int timeout_ms)
{
  if (g_sock == INVALID_SOCKET)
    {
      errno = ENOTCONN;
      return -1;
    }

  fd_set read_fds;
  struct timeval tv;

  FD_ZERO (&read_fds);
  FD_SET (g_sock, &read_fds);

  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int ready = select (0, &read_fds, NULL, NULL, &tv);
  if (ready == SOCKET_ERROR)
    {
      int w = WSAGetLastError ();
      emit_sock_err ("select", w);
      map_wsa_to_errno (w);
      return -1;
    }

  if (ready == 0)
    return 0;

  for (;;)
    {
      char buffer[EMIT_PACKET_SIZE];
      struct sockaddr_in from_addr;
      int from_len = (int)sizeof (from_addr);

      int recv_len = recvfrom (g_sock, buffer, (int)sizeof (buffer) - 1,
                               MSG_DONTWAIT, (struct sockaddr *)&from_addr,
                               &from_len);
      if (recv_len == SOCKET_ERROR)
        {
          int w = WSAGetLastError ();
          if (w == WSAEWOULDBLOCK || w == WSAEINTR)
            break;
          emit_sock_err ("recvfrom", w);
          map_wsa_to_errno (w);
          return -1;
        }

      buffer[recv_len] = '\0';

      peer_info_t peer;
      memset (&peer, 0, sizeof (peer));
      strncpy (peer.peer_id, buffer, EMIT_PEER_ID_LEN - 1);
      peer.ip_address = from_addr.sin_addr;
      peer.port = ntohs (from_addr.sin_port);
      peer.timestamp = time (NULL);

      if (callback)
        callback (&peer, user_data);
    }

  return 0;
}

void
emit_cleanup (void)
{
  if (g_sock != INVALID_SOCKET)
    {
      closesocket (g_sock);
      g_sock = INVALID_SOCKET;

      if (g_wsa_refcount > 0)
        {
          g_wsa_refcount--;
          if (g_wsa_refcount == 0)
            WSACleanup ();
        }
    }
  g_port = 0;
  memset (g_peer_id, 0, sizeof (g_peer_id));
}

int
emit_get_socket_fd (void)
{
  if (g_sock == INVALID_SOCKET)
    return -1;
  return (int)(intptr_t)g_sock;
}
