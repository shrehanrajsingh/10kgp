#include "emit.h"
#include "emit_bcast.h"

static int g_socket_fd = -1;
static uint16_t g_port = 0;
static char g_peer_id[EMIT_PEER_ID_LEN] = { 0 };

int
emit_init (uint16_t port)
{
  g_port = port;

  /* Create UDP socket */
  g_socket_fd = socket (AF_INET, SOCK_DGRAM, 0);
  if (g_socket_fd < 0)
    {
      perror ("socket");
      return -1;
    }

  /* Allow broadcast */
  int broadcast = 1;
  if (setsockopt (g_socket_fd, SOL_SOCKET, SO_BROADCAST, &broadcast,
                  sizeof (broadcast))
      < 0)
    {
      perror ("setsockopt SO_BROADCAST");
      close (g_socket_fd);
      g_socket_fd = -1;
      return -1;
    }

  /* Allow address reuse */
  int reuse = 1;
  if (setsockopt (g_socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                  sizeof (reuse))
      < 0)
    {
      perror ("setsockopt SO_REUSEADDR");
      close (g_socket_fd);
      g_socket_fd = -1;
      return -1;
    }

  /* Bind to port */
  struct sockaddr_in bind_addr = { 0 };
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = htonl (INADDR_ANY);
  bind_addr.sin_port = htons (port);

  if (bind (g_socket_fd, (struct sockaddr *)&bind_addr, sizeof (bind_addr))
      < 0)
    {
      perror ("bind");
      close (g_socket_fd);
      g_socket_fd = -1;
      return -1;
    }

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
  if (g_socket_fd < 0)
    {
      errno = ENOTCONN;
      return -1;
    }

  /* Packet format: [peer_id] */
  char packet[EMIT_PACKET_SIZE] = { 0 };
  strncpy (packet, g_peer_id, sizeof (packet) - 1);

  struct sockaddr_in targets[EMIT_BCAST_MAX_TARGETS];
  size_t nt = emit_bcast_collect (targets, EMIT_BCAST_MAX_TARGETS);
  int any_ok = 0;

  for (size_t i = 0; i < nt; i++)
    {
      targets[i].sin_port = htons (g_port);
      ssize_t sent = sendto (g_socket_fd, packet, strlen (packet), 0,
                             (struct sockaddr *)&targets[i],
                             sizeof (targets[i]));
      if (sent >= 0)
        any_ok = 1;
    }

  if (!any_ok)
    {
      perror ("sendto");
      return -1;
    }

  return 0;
}

int
emit_listen (peer_callback_t callback, void *user_data, int timeout_ms)
{
  if (g_socket_fd < 0)
    {
      errno = ENOTCONN;
      return -1;
    }

  fd_set read_fds;
  struct timeval tv;

  FD_ZERO (&read_fds);
  FD_SET (g_socket_fd, &read_fds);

  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int ready = select (g_socket_fd + 1, &read_fds, NULL, NULL, &tv);
  if (ready < 0)
    {
      perror ("select");
      return -1;
    }

  if (ready == 0)
    {
      /* Timeout - no peers detected */
      return 0;
    }

  /* Drain queued datagrams (Wi‑Fi / stacks may batch broadcasts). */
  for (;;)
    {
      char buffer[EMIT_PACKET_SIZE];
      struct sockaddr_in from_addr;
      socklen_t from_len = sizeof (from_addr);

      ssize_t recv_len = recvfrom (g_socket_fd, buffer, sizeof (buffer) - 1,
                                   MSG_DONTWAIT, (struct sockaddr *)&from_addr,
                                   &from_len);
      if (recv_len < 0)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            break;
          perror ("recvfrom");
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
  if (g_socket_fd >= 0)
    {
      close (g_socket_fd);
      g_socket_fd = -1;
    }
  g_port = 0;
  memset (g_peer_id, 0, sizeof (g_peer_id));
}

int
emit_get_socket_fd (void)
{
  return g_socket_fd;
}
