#include "win_stdin.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define WIN_STDIN_QUEUE 64
#define WIN_STDIN_LINE 512

static CRITICAL_SECTION s_lock;
static int s_lock_inited;
static char s_queue[WIN_STDIN_QUEUE][WIN_STDIN_LINE];
static size_t s_qhead;
static size_t s_qtail;
static size_t s_qlen;
static HANDLE s_thread = NULL;
static volatile LONG s_stop;

static DWORD WINAPI
reader_thread (void *arg)
{
  (void)arg;
  char buf[WIN_STDIN_LINE];

  while (InterlockedCompareExchange (&s_stop, 0, 0) == 0)
    {
      if (fgets (buf, (int)sizeof (buf), stdin) == NULL)
        break;

      EnterCriticalSection (&s_lock);
      if (s_qlen < WIN_STDIN_QUEUE)
        {
          memcpy (s_queue[s_qtail], buf, sizeof (s_queue[s_qtail]));
          s_queue[s_qtail][WIN_STDIN_LINE - 1] = '\0';
          s_qtail = (s_qtail + 1) % WIN_STDIN_QUEUE;
          s_qlen++;
        }
      LeaveCriticalSection (&s_lock);
    }
  return 0;
}

int
win_stdin_reader_start (void)
{
  if (s_thread != NULL)
    return 0;

  if (!s_lock_inited)
    {
      InitializeCriticalSection (&s_lock);
      s_lock_inited = 1;
    }

  s_qhead = s_qtail = s_qlen = 0;
  InterlockedExchange (&s_stop, 0);

  s_thread = CreateThread (NULL, 0, reader_thread, NULL, 0, NULL);
  if (s_thread == NULL)
    return -1;
  return 0;
}

void
win_stdin_reader_stop (void)
{
  InterlockedExchange (&s_stop, 1);
  if (s_thread != NULL)
    {
      WaitForSingleObject (s_thread, 50);
      CloseHandle (s_thread);
      s_thread = NULL;
    }
}

int
win_stdin_try_line (char *buf, size_t cap)
{
  if (!buf || cap == 0)
    return 0;

  int got = 0;
  EnterCriticalSection (&s_lock);
  if (s_qlen > 0)
    {
      strncpy (buf, s_queue[s_qhead], cap - 1);
      buf[cap - 1] = '\0';
      s_qhead = (s_qhead + 1) % WIN_STDIN_QUEUE;
      s_qlen--;
      got = 1;
    }
  LeaveCriticalSection (&s_lock);
  return got;
}
