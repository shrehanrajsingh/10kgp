#include "persi_private.h"
#include <stdlib.h>
#include <string.h>

void
persi__val_free_cell (persi_val_t *v, int type)
{
  if (type == PERSI_TYPE_STRING && v->str_v.v)
    {
      free (v->str_v.v);
      v->str_v.v = NULL;
      v->str_v.vl = 0;
    }
}

void
persi__row_free_vals (persi_val_t *vals, const persi_coldef_t *cols,
                      size_t ncol)
{
  if (!vals)
    return;
  for (size_t i = 0; i < ncol; i++)
    persi__val_free_cell (&vals[i], cols[i].type);
  free (vals);
}

int
persi__val_copy_in (persi_val_t *dst, const persi_val_t *src, int type)
{
  switch (type)
    {
    case PERSI_TYPE_INT:
      dst->i_v = src->i_v;
      return 0;
    case PERSI_TYPE_FLOAT:
      dst->f_v = src->f_v;
      return 0;
    case PERSI_TYPE_STRING:
      {
        size_t n = src->str_v.vl;
        if (n > PERSI_MAX_STRING)
          return PERSI_ERR_SCHEMA;
        char *p = (char *)malloc (n + 1);
        if (!p)
          return PERSI_ERR_NOMEM;
        if (n && src->str_v.v)
          memcpy (p, src->str_v.v, n);
        p[n] = '\0';
        dst->str_v.v = p;
        dst->str_v.vl = n;
        return 0;
      }
    default:
      return PERSI_ERR_SCHEMA;
    }
}

int
persi__val_eq (const persi_val_t *a, const persi_val_t *b, int type)
{
  switch (type)
    {
    case PERSI_TYPE_INT:
      return a->i_v == b->i_v;
    case PERSI_TYPE_FLOAT:
      return a->f_v == b->f_v;
    case PERSI_TYPE_STRING:
      if (a->str_v.vl != b->str_v.vl)
        return 0;
      if (a->str_v.vl == 0)
        return 1;
      return memcmp (a->str_v.v, b->str_v.v, a->str_v.vl) == 0;
    default:
      return 0;
    }
}

int
persi__val_write_cell (FILE *f, const persi_val_t *v, int type)
{
  switch (type)
    {
    case PERSI_TYPE_INT:
      persi__io_write_i32_le (f, (int32_t)v->i_v);
      return 0;
    case PERSI_TYPE_FLOAT:
      persi__io_write_f32_le (f, v->f_v);
      return 0;
    case PERSI_TYPE_STRING:
      {
        uint32_t len = (uint32_t)v->str_v.vl;
        if (len > PERSI_MAX_STRING)
          return PERSI_ERR_SCHEMA;
        persi__io_write_u32_le (f, len);
        if (len && fwrite (v->str_v.v, 1, len, f) != len)
          return PERSI_ERR_IO;
        return 0;
      }
    default:
      return PERSI_ERR_SCHEMA;
    }
}

int
persi__val_read_cell (FILE *f, persi_val_t *v, int type)
{
  switch (type)
    {
    case PERSI_TYPE_INT:
      {
        int32_t x;
        int e = persi__io_read_i32_le (f, &x);
        if (e != 0)
          return e;
        v->i_v = (int)x;
        return 0;
      }
    case PERSI_TYPE_FLOAT:
      return persi__io_read_f32_le (f, &v->f_v);
    case PERSI_TYPE_STRING:
      {
        uint32_t len;
        int e = persi__io_read_u32_le (f, &len);
        if (e != 0)
          return e;
        if (len > PERSI_MAX_STRING)
          return PERSI_ERR_FORMAT;
        v->str_v.vl = len;
        if (len == 0)
          {
            v->str_v.v = persi_xstrdup ("");
            if (!v->str_v.v)
              return PERSI_ERR_NOMEM;
            return 0;
          }
        v->str_v.v = (char *)malloc (len + 1);
        if (!v->str_v.v)
          return PERSI_ERR_NOMEM;
        if (fread (v->str_v.v, 1, len, f) != len)
          {
            free (v->str_v.v);
            v->str_v.v = NULL;
            return PERSI_ERR_IO;
          }
        v->str_v.v[len] = '\0';
        return 0;
      }
    default:
      return PERSI_ERR_FORMAT;
    }
}
