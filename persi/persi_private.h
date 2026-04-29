#ifndef PERSI_PRIVATE_H
#define PERSI_PRIVATE_H

#include "persi.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  uint64_t id;
  persi_val_t *vals;
} prow_t;

typedef struct
{
  char name[PERSI_MAX_NAME];
  persi_coldef_t cols[PERSI_MAX_COLS];
  size_t ncol;
  prow_t *rows;
  size_t nrow;
  size_t nrow_cap;
} ptable_t;

struct persi_conn
{
  char *path;
  uint64_t next_id;
  ptable_t *tabs;
  size_t ntab;
  size_t ntab_cap;
};

static inline void
persi__io_write_u8 (FILE *f, uint8_t v)
{
  fwrite (&v, 1, 1, f);
}

static inline void
persi__io_write_u16_le (FILE *f, uint16_t v)
{
  unsigned char b[2];
  b[0] = (unsigned char)(v & 0xffu);
  b[1] = (unsigned char)((v >> 8) & 0xffu);
  fwrite (b, 1, 2, f);
}

static inline void
persi__io_write_u32_le (FILE *f, uint32_t v)
{
  unsigned char b[4];
  b[0] = (unsigned char)(v & 0xffu);
  b[1] = (unsigned char)((v >> 8) & 0xffu);
  b[2] = (unsigned char)((v >> 16) & 0xffu);
  b[3] = (unsigned char)((v >> 24) & 0xffu);
  fwrite (b, 1, 4, f);
}

static inline void
persi__io_write_u64_le (FILE *f, uint64_t v)
{
  for (int i = 0; i < 8; i++)
    {
      unsigned char b = (unsigned char)((v >> (8 * i)) & 0xffu);
      fwrite (&b, 1, 1, f);
    }
}

static inline void
persi__io_write_i32_le (FILE *f, int32_t v)
{
  persi__io_write_u32_le (f, (uint32_t)v);
}

static inline void
persi__io_write_f32_le (FILE *f, float x)
{
  uint32_t u;
  memcpy (&u, &x, sizeof (u));
  persi__io_write_u32_le (f, u);
}

static inline int
persi__io_read_u8 (FILE *f, uint8_t *out)
{
  return fread (out, 1, 1, f) == 1 ? 0 : PERSI_ERR_IO;
}

static inline int
persi__io_read_u16_le (FILE *f, uint16_t *out)
{
  unsigned char b[2];
  if (fread (b, 1, 2, f) != 2)
    return PERSI_ERR_IO;
  *out = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
  return 0;
}

static inline int
persi__io_read_u32_le (FILE *f, uint32_t *out)
{
  unsigned char b[4];
  if (fread (b, 1, 4, f) != 4)
    return PERSI_ERR_IO;
  *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] << 24);
  return 0;
}

static inline int
persi__io_read_u64_le (FILE *f, uint64_t *out)
{
  unsigned char b[8];
  if (fread (b, 1, 8, f) != 8)
    return PERSI_ERR_IO;
  uint64_t v = 0;
  for (int i = 0; i < 8; i++)
    v |= (uint64_t)b[i] << (8 * i);
  *out = v;
  return 0;
}

static inline int
persi__io_read_i32_le (FILE *f, int32_t *out)
{
  uint32_t u;
  int e = persi__io_read_u32_le (f, &u);
  if (e != 0)
    return e;
  memcpy (out, &u, sizeof (*out));
  return 0;
}

static inline int
persi__io_read_f32_le (FILE *f, float *out)
{
  uint32_t u;
  int e = persi__io_read_u32_le (f, &u);
  if (e != 0)
    return e;
  memcpy (out, &u, sizeof (*out));
  return 0;
}

void persi__val_free_cell (persi_val_t *v, int type);
void persi__row_free_vals (persi_val_t *vals, const persi_coldef_t *cols,
                           size_t ncol);
int persi__val_copy_in (persi_val_t *dst, const persi_val_t *src, int type);
int persi__val_eq (const persi_val_t *a, const persi_val_t *b, int type);
int persi__val_write_cell (FILE *f, const persi_val_t *v, int type);
int persi__val_read_cell (FILE *f, persi_val_t *v, int type);

ptable_t *persi__find_table (persi_conn_t *c, const char *name);
void persi__free_conn_tables (persi_conn_t *c);
int persi__format_save (persi_conn_t *c);
int persi__format_load (persi_conn_t *c);
int persi__id_eq_ci (const char *a, const char *b);

#if defined(_WIN32) && defined(_MSC_VER)
#define persi_xstrdup _strdup
#else
#define persi_xstrdup strdup
#endif

#endif
