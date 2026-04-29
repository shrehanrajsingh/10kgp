#ifndef WIN_STDIN_H
#define WIN_STDIN_H

/* Background stdin line reader for Windows (Winsock select cannot mix CRT stdin). */

int win_stdin_reader_start (void);
void win_stdin_reader_stop (void);

/* Copy one queued line into buf (NUL-terminated). Returns 1 if a line was consumed. */
int win_stdin_try_line (char *buf, size_t cap);

#endif
