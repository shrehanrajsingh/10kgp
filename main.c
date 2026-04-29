#include "main.h"
#include <stdio.h>

#ifdef _WIN32
#include "win_stdin.h"
#endif

#define PEER_TIMEOUT_SEC 30
#define PULSE_INTERVAL_SEC 5
#define MAX_ALIASES 32
#define ALIAS_NAME_LEN 16
#define ALIAS_CMD_LEN 32

/* Alias structure */
typedef struct
{
  char name[ALIAS_NAME_LEN];
  char command[ALIAS_CMD_LEN];
} alias_t;

/* Peer storage */
static peer_info_t g_peers[EMIT_MAX_PEERS];
static size_t g_peer_count = 0;
static time_t g_last_pulse = 0;
static char g_my_peer_id[EMIT_PEER_ID_LEN] = { 0 };

/* Alias storage */
static alias_t g_aliases[MAX_ALIASES];
static size_t g_alias_count = 0;
static int g_resolving_alias = 0;

/* Forward declarations */
static void add_peer (const peer_info_t *peer);
static size_t get_online_count (void);
static void print_online_peers (void);
static void cleanup_stale_peers (void);
static void print_help (void);
static void clear_screen (void);
static int find_alias (const char *name);
static void cmd_alias (const char *args);
static void cmd_show_aliases (void);
static void cmd_clear_alias (const char *alias_name);

/* Callback when a peer is detected */
static void
on_peer_detected (const peer_info_t *peer, void *user_data)
{
  (void)user_data;
  add_peer (peer);
}

/* Add or update peer in our list */
static void
add_peer (const peer_info_t *peer)
{
  /* Check if peer already exists (by IP address) */
  for (size_t i = 0; i < g_peer_count; i++)
    {
      if (g_peers[i].ip_address.s_addr == peer->ip_address.s_addr
          && g_peers[i].port == peer->port)
        {
          /* Update existing peer */
          strncpy (g_peers[i].peer_id, peer->peer_id, EMIT_PEER_ID_LEN - 1);
          g_peers[i].timestamp = time (NULL);
          return;
        }
    }

  /* Add new peer if space available */
  if (g_peer_count < EMIT_MAX_PEERS)
    {
      g_peers[g_peer_count] = *peer;
      g_peers[g_peer_count].timestamp = time (NULL);
      g_peer_count++;
    }
}

/* Get count of peers seen within timeout period */
static size_t
get_online_count (void)
{
  time_t now = time (NULL);
  size_t count = 0;

  for (size_t i = 0; i < g_peer_count; i++)
    {
      if (now - g_peers[i].timestamp < PEER_TIMEOUT_SEC)
        {
          count++;
        }
    }

  return count;
}

/* Remove peers not seen within timeout */
static void
cleanup_stale_peers (void)
{
  time_t now = time (NULL);
  size_t write_idx = 0;

  for (size_t i = 0; i < g_peer_count; i++)
    {
      if (now - g_peers[i].timestamp < PEER_TIMEOUT_SEC)
        {
          g_peers[write_idx] = g_peers[i];
          write_idx++;
        }
    }

  g_peer_count = write_idx;
}

/* Print all online peers */
static void
print_online_peers (void)
{
  size_t online = get_online_count ();

  if (online == 0)
    {
      printf ("No peers online.\n");
      return;
    }

  printf ("Online peers (%zu):\n", online);
  printf ("%-6s %-32s %-16s %-6s %s\n", "No.", "Peer ID", "IP Address", "Port",
          "Last Seen");
  printf ("-------------------------------------------------------------------"
          "---\n");

  time_t now = time (NULL);
  size_t display_num = 1;

  for (size_t i = 0; i < g_peer_count; i++)
    {
      if (now - g_peers[i].timestamp < PEER_TIMEOUT_SEC)
        {
          char ip_str[INET_ADDRSTRLEN];
          inet_ntop (AF_INET, &g_peers[i].ip_address, ip_str, sizeof (ip_str));

          int age = (int)(now - g_peers[i].timestamp);
          printf ("%-6zu %-32s %-16s %-6u %ds ago\n", display_num++,
                  g_peers[i].peer_id, ip_str, g_peers[i].port, age);
        }
    }
}

/* Print help message */
static void
print_help (void)
{
  printf ("\n");
  printf ("Available commands:\n");
  printf ("  /online              - List all online peers\n");
  printf ("  /count               - Show number of online peers\n");
  printf ("  /refresh             - Send a broadcast pulse now\n");
  printf ("  /clear               - Clear the screen\n");
  printf ("  /alias <cmd> <alias> - Create alias (e.g. /alias online o)\n");
  printf ("  /show-alias          - List all aliases\n");
  printf ("  /clear-alias <alias> - Remove an alias\n");
  printf ("  /help                - Show this help message\n");
  printf ("  /exit                - Exit the application\n");
  printf ("\nAliases are session-only and cleared on exit.\n");
  printf ("\n");
}

/* Clear screen */
static void
clear_screen (void)
{
  printf ("\033[2J\033[H");
  fflush (stdout);
}

/* Find alias index by name, returns -1 if not found */
static int
find_alias (const char *name)
{
  for (size_t i = 0; i < g_alias_count; i++)
    {
      if (strcmp (g_aliases[i].name, name) == 0)
        return (int)i;
    }
  return -1;
}

/* Add or update alias: args is "<command> <alias_name>" */
static void
cmd_alias (const char *args)
{
  char command[ALIAS_CMD_LEN] = { 0 };
  char alias_name[ALIAS_NAME_LEN] = { 0 };

  if (sscanf (args, "%31s %15s", command, alias_name) != 2)
    {
      printf ("Usage: /alias <command> <alias>\n");
      return;
    }

  int idx = find_alias (alias_name);
  if (idx >= 0)
    {
      strncpy (g_aliases[idx].command, command, ALIAS_CMD_LEN - 1);
      printf ("Alias '/%s' updated -> '/%s'.\n", alias_name, command);
    }
  else if (g_alias_count < MAX_ALIASES)
    {
      strncpy (g_aliases[g_alias_count].name, alias_name, ALIAS_NAME_LEN - 1);
      strncpy (g_aliases[g_alias_count].command, command, ALIAS_CMD_LEN - 1);
      g_alias_count++;
      printf ("Alias '/%s' -> '/%s' created.\n", alias_name, command);
    }
  else
    {
      printf ("Alias limit reached (%d).\n", MAX_ALIASES);
    }
}

/* Print all defined aliases */
static void
cmd_show_aliases (void)
{
  if (g_alias_count == 0)
    {
      printf ("No aliases defined.\n");
      return;
    }

  printf ("Aliases (%zu):\n", g_alias_count);
  printf ("  %-16s -> %s\n", "Alias", "Command");
  printf ("  ----------------    -------\n");
  for (size_t i = 0; i < g_alias_count; i++)
    {
      printf ("  /%-15s -> /%s\n", g_aliases[i].name, g_aliases[i].command);
    }
}

/* Remove a specific alias by name */
static void
cmd_clear_alias (const char *alias_name)
{
  int idx = find_alias (alias_name);
  if (idx < 0)
    {
      printf ("Alias '%s' not found.\n", alias_name);
      return;
    }

  for (size_t i = (size_t)idx; i < g_alias_count - 1; i++)
    g_aliases[i] = g_aliases[i + 1];
  g_alias_count--;

  printf ("Alias '/%s' cleared.\n", alias_name);
}

/* Trim whitespace from input */
static char *
trim (char *str)
{
  /* Trim leading space */
  while (*str == ' ' || *str == '\t')
    str++;

  /* Trim trailing newline/whitespace */
  size_t len = strlen (str);
  while (len > 0
         && (str[len - 1] == '\n' || str[len - 1] == '\r'
             || str[len - 1] == ' ' || str[len - 1] == '\t'))
    {
      str[--len] = '\0';
    }

  return str;
}

/* Parse and execute command, returns 0 to continue, 1 to exit */
static int
process_command (const char *input)
{
  char cmd[64] = { 0 };
  strncpy (cmd, input, sizeof (cmd) - 1);

  char *trimmed = trim (cmd);

  /* Empty input - just refresh display */
  if (strlen (trimmed) == 0)
    {
      return 0;
    }

  if (strcmp (trimmed, "/exit") == 0 || strcmp (trimmed, "/quit") == 0)
    {
      printf ("Goodbye!\n");
      return 1;
    }

  if (strcmp (trimmed, "/help") == 0 || strcmp (trimmed, "/?") == 0)
    {
      print_help ();
      return 0;
    }

  if (strcmp (trimmed, "/online") == 0 || strcmp (trimmed, "/peers") == 0)
    {
      print_online_peers ();
      return 0;
    }

  if (strcmp (trimmed, "/count") == 0)
    {
      printf ("Online peers: %zu\n", get_online_count ());
      return 0;
    }

  if (strcmp (trimmed, "/refresh") == 0)
    {
      printf ("Sending broadcast pulse...\n");
      emit_pulse ();
      g_last_pulse = time (NULL);
      return 0;
    }

  if (strcmp (trimmed, "/clear") == 0)
    {
      clear_screen ();
      return 0;
    }

  if (strncmp (trimmed, "/alias ", 7) == 0)
    {
      cmd_alias (trimmed + 7);
      return 0;
    }

  if (strcmp (trimmed, "/show-alias") == 0)
    {
      cmd_show_aliases ();
      return 0;
    }

  if (strncmp (trimmed, "/clear-alias ", 13) == 0)
    {
      cmd_clear_alias (trimmed + 13);
      return 0;
    }

  /* Resolve alias: /xyz looks up "xyz" in alias table */
  if (!g_resolving_alias && trimmed[0] == '/')
    {
      const char *alias_input = trimmed + 1;
      char alias_verb[ALIAS_NAME_LEN] = { 0 };
      const char *sp = strchr (alias_input, ' ');
      size_t vlen = sp ? (size_t)(sp - alias_input) : strlen (alias_input);
      if (vlen < ALIAS_NAME_LEN)
        strncpy (alias_verb, alias_input, vlen);

      int idx = find_alias (alias_verb);
      if (idx >= 0)
        {
          char resolved[64] = { 0 };
          snprintf (resolved, sizeof (resolved), "/%s",
                    g_aliases[idx].command);
          g_resolving_alias = 1;
          int ret = process_command (resolved);
          g_resolving_alias = 0;
          return ret;
        }
    }

  /* Unknown command */
  printf ("Unknown command: %s. Type /help for available commands.\n",
          trimmed);
  return 0;
}

int
main (int argc, char **argv)
{
  (void)argc;
  (void)argv;

  printf ("=== Peer Discovery ===\n\n");

  /* Get peer ID from user */
  printf ("Enter your peer ID: ");
  fflush (stdout);

  if (fgets (g_my_peer_id, sizeof (g_my_peer_id), stdin) == NULL)
    {
      fprintf (stderr, "Failed to read peer ID\n");
      return 1;
    }

  /* Remove trailing newline */
  size_t len = strlen (g_my_peer_id);
  if (len > 0 && g_my_peer_id[len - 1] == '\n')
    {
      g_my_peer_id[len - 1] = '\0';
    }

  if (strlen (g_my_peer_id) == 0)
    {
      fprintf (stderr, "Peer ID cannot be empty\n");
      return 1;
    }

  /* Initialize emitter */
  if (emit_init (9999) < 0)
    {
      fprintf (stderr, "Failed to initialize emitter\n");
      return 1;
    }

  emit_set_peer_id (g_my_peer_id);

  printf ("\nPeer discovery started. Type /help for commands.\n\n");

#ifdef _WIN32
  if (win_stdin_reader_start () != 0)
    {
      fprintf (stderr, "Failed to start stdin reader\n");
      emit_cleanup ();
      return 1;
    }
#endif

  /* Send initial pulse */
  emit_pulse ();
  g_last_pulse = time (NULL);

  int running = 1;
  char input[256] = { 0 };
  int awaiting_input = 1; /* Track if we're waiting for user input */

  while (running)
    {
      /* Cleanup stale peers periodically */
      cleanup_stale_peers ();

      /* Send periodic pulse */
      time_t now = time (NULL);
      if (now - g_last_pulse >= PULSE_INTERVAL_SEC)
        {
          emit_pulse ();
          g_last_pulse = now;
        }

      /* Print prompt only when awaiting new input */
      if (awaiting_input)
        {
          printf ("> ");
          fflush (stdout);
          awaiting_input = 0;
        }

      /* Wait for network (and stdin on POSIX); Windows stdin via win_stdin */
      fd_set read_fds;
      struct timeval tv;
      int sock = emit_get_socket_fd ();

      FD_ZERO (&read_fds);
#ifdef _WIN32
      if (sock >= 0)
        FD_SET ((SOCKET)(intptr_t)sock, &read_fds);
#else
      FD_SET (STDIN_FILENO, &read_fds);
      if (sock >= 0)
        FD_SET (sock, &read_fds);
#endif

      tv.tv_sec = 1; /* 1 second timeout for pulse interval check */
      tv.tv_usec = 0;

#ifdef _WIN32
      int nfds = 0; /* ignored on Winsock */
#else
      int nfds = sock >= 0 ? sock + 1 : STDIN_FILENO + 1;
#endif

      int ready = select (nfds, &read_fds, NULL, NULL, &tv);

      if (ready < 0)
        {
#ifdef _WIN32
          int w = WSAGetLastError ();
          if (w == WSAEINTR)
            continue;
          fprintf (stderr, "select: winsock error %d\n", w);
#else
          if (errno == EINTR)
            continue; /* Interrupted, continue loop */
          perror ("select");
#endif
          break;
        }

      if (ready > 0)
        {
#ifdef _WIN32
          if (sock >= 0 && FD_ISSET ((SOCKET)(intptr_t)sock, &read_fds))
#else
          if (sock >= 0 && FD_ISSET (sock, &read_fds))
#endif
            {
              emit_listen (on_peer_detected, NULL, 0);
            }

#ifndef _WIN32
          if (FD_ISSET (STDIN_FILENO, &read_fds))
            {
              if (fgets (input, sizeof (input), stdin) != NULL)
                {
                  running = !process_command (input);
                  awaiting_input = 1;
                }
            }
#endif
        }

#ifdef _WIN32
      if (win_stdin_try_line (input, sizeof (input)))
        {
          running = !process_command (input);
          awaiting_input = 1;
        }
#endif
    }

#ifdef _WIN32
  win_stdin_reader_stop ();
#endif
  emit_cleanup ();
  return 0;
}
