#include "persi_private.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int
persi__id_eq_ci (const char *a, const char *b)
{
  if (!a || !b)
    return 0;
  for (; *a || *b; a++, b++)
    {
      unsigned char ca = (unsigned char)*a;
      unsigned char cb = (unsigned char)*b;
      if (tolower (ca) != tolower (cb))
        return 0;
    }
  return 1;
}

ptable_t *
persi__find_table (persi_conn_t *c, const char *name)
{
  for (size_t i = 0; i < c->ntab; i++)
    if (strcmp (c->tabs[i].name, name) == 0)
      return &c->tabs[i];
  return NULL;
}

void
persi__free_conn_tables (persi_conn_t *c)
{
  if (!c->tabs)
    return;
  for (size_t t = 0; t < c->ntab; t++)
    {
      ptable_t *tb = &c->tabs[t];
      for (size_t r = 0; r < tb->nrow; r++)
        persi__row_free_vals (tb->rows[r].vals, tb->cols, tb->ncol);
      free (tb->rows);
      tb->rows = NULL;
      tb->nrow = tb->nrow_cap = 0;
    }
  free (c->tabs);
  c->tabs = NULL;
  c->ntab = c->ntab_cap = 0;
}

const char *
persi_strerror (int code)
{
  switch (code)
    {
    case PERSI_OK:
      return "ok";
    case PERSI_ERR:
      return "generic error";
    case PERSI_ERR_IO:
      return "i/o error";
    case PERSI_ERR_NOMEM:
      return "out of memory";
    case PERSI_ERR_EXISTS:
      return "already exists";
    case PERSI_ERR_NOTFOUND:
      return "not found";
    case PERSI_ERR_SCHEMA:
      return "schema mismatch";
    case PERSI_ERR_BOUNDS:
      return "out of bounds";
    case PERSI_ERR_PARSE:
      return "parse error";
    case PERSI_ERR_FORMAT:
      return "invalid file format";
    default:
      return "unknown";
    }
}

int
persi_open (const char *path, persi_conn_t **out)
{
  if (!path || !out)
    return PERSI_ERR;
  persi_conn_t *c = (persi_conn_t *)calloc (1, sizeof (*c));
  if (!c)
    return PERSI_ERR_NOMEM;
  c->path = persi_xstrdup (path);
  if (!c->path)
    {
      free (c);
      return PERSI_ERR_NOMEM;
    }
  c->next_id = 1;
  FILE *test = fopen (path, "rb");
  if (!test)
    {
      *out = c;
      return 0;
    }
  fseek (test, 0, SEEK_END);
  long sz = ftell (test);
  fclose (test);
  if (sz <= 0)
    {
      *out = c;
      return 0;
    }
  int e = persi__format_load (c);
  if (e != 0)
    {
      free (c->path);
      free (c);
      return e;
    }
  *out = c;
  return 0;
}

void
persi_close (persi_conn_t *c)
{
  if (!c)
    return;
  persi__free_conn_tables (c);
  free (c->path);
  free (c);
}

int
persi_sync (persi_conn_t *c)
{
  if (!c)
    return PERSI_ERR;
  return persi__format_save (c);
}

int
persi_create_table (persi_conn_t *c, const char *table,
                    const persi_coldef_t *cols, size_t ncols)
{
  if (!c || !table || !cols || ncols == 0 || ncols > PERSI_MAX_COLS)
    return PERSI_ERR_SCHEMA;
  if (persi__find_table (c, table))
    return PERSI_ERR_EXISTS;
  if (c->ntab >= PERSI_MAX_TABLES)
    return PERSI_ERR_BOUNDS;
  for (size_t i = 0; i < ncols; i++)
    {
      if (cols[i].type < 0 || cols[i].type >= PERSI_TYPE_COUNT)
        return PERSI_ERR_SCHEMA;
      if (cols[i].name[0] == '\0')
        return PERSI_ERR_SCHEMA;
    }
  if (c->ntab >= c->ntab_cap)
    {
      size_t ncap = c->ntab_cap ? c->ntab_cap * 2 : 4;
      ptable_t *nt = (ptable_t *)realloc (c->tabs, ncap * sizeof (ptable_t));
      if (!nt)
        return PERSI_ERR_NOMEM;
      memset (nt + c->ntab_cap, 0, (ncap - c->ntab_cap) * sizeof (ptable_t));
      c->tabs = nt;
      c->ntab_cap = ncap;
    }
  ptable_t *tb = &c->tabs[c->ntab];
  memset (tb, 0, sizeof (*tb));
  strncpy (tb->name, table, PERSI_MAX_NAME - 1);
  tb->ncol = ncols;
  memcpy (tb->cols, cols, ncols * sizeof (persi_coldef_t));
  c->ntab++;
  return 0;
}

int
persi_insert (persi_conn_t *c, const char *table, const persi_val_t *vals,
              size_t n, uint64_t *out_row_id)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (n != tb->ncol)
    return PERSI_ERR_SCHEMA;
  if (tb->nrow >= tb->nrow_cap)
    {
      size_t ncap = tb->nrow_cap ? tb->nrow_cap * 2 : 8;
      prow_t *nr = (prow_t *)realloc (tb->rows, ncap * sizeof (prow_t));
      if (!nr)
        return PERSI_ERR_NOMEM;
      memset (nr + tb->nrow_cap, 0, (ncap - tb->nrow_cap) * sizeof (prow_t));
      tb->rows = nr;
      tb->nrow_cap = ncap;
    }
  persi_val_t *nv = (persi_val_t *)calloc (tb->ncol, sizeof (persi_val_t));
  if (!nv)
    return PERSI_ERR_NOMEM;
  for (size_t j = 0; j < tb->ncol; j++)
    {
      int e = persi__val_copy_in (&nv[j], &vals[j], tb->cols[j].type);
      if (e != 0)
        {
          persi__row_free_vals (nv, tb->cols, tb->ncol);
          return e;
        }
    }
  uint64_t id = c->next_id++;
  tb->rows[tb->nrow].id = id;
  tb->rows[tb->nrow].vals = nv;
  tb->nrow++;
  if (out_row_id)
    *out_row_id = id;
  return 0;
}

int
persi_scan_table (persi_conn_t *c, const char *table, persi_row_fn fn,
                  void *ud)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  for (size_t r = 0; r < tb->nrow; r++)
    fn (ud, tb->rows[r].id, tb->rows[r].vals, tb->ncol);
  return 0;
}

int
persi_scan_where_eq (persi_conn_t *c, const char *table, size_t col_idx,
                     const persi_val_t *needle, persi_row_fn fn, void *ud)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (col_idx >= tb->ncol)
    return PERSI_ERR_BOUNDS;
  int ty = tb->cols[col_idx].type;
  for (size_t r = 0; r < tb->nrow; r++)
    {
      if (persi__val_eq (&tb->rows[r].vals[col_idx], needle, ty))
        fn (ud, tb->rows[r].id, tb->rows[r].vals, tb->ncol);
    }
  return 0;
}

int
persi_update_row (persi_conn_t *c, const char *table, uint64_t row_id,
                  const persi_val_t *vals, size_t n)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (n != tb->ncol)
    return PERSI_ERR_SCHEMA;
  for (size_t r = 0; r < tb->nrow; r++)
    {
      if (tb->rows[r].id != row_id)
        continue;
      persi_val_t *nv = (persi_val_t *)calloc (tb->ncol, sizeof (persi_val_t));
      if (!nv)
        return PERSI_ERR_NOMEM;
      for (size_t j = 0; j < tb->ncol; j++)
        {
          int e = persi__val_copy_in (&nv[j], &vals[j], tb->cols[j].type);
          if (e != 0)
            {
              persi__row_free_vals (nv, tb->cols, tb->ncol);
              return e;
            }
        }
      persi__row_free_vals (tb->rows[r].vals, tb->cols, tb->ncol);
      tb->rows[r].vals = nv;
      return 0;
    }
  return PERSI_ERR_NOTFOUND;
}

int
persi_update_where_eq (persi_conn_t *c, const char *table, size_t set_col,
                       const persi_val_t *new_val, size_t where_col,
                       const persi_val_t *needle)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (set_col >= tb->ncol || where_col >= tb->ncol)
    return PERSI_ERR_BOUNDS;
  int wty = tb->cols[where_col].type;
  int sty = tb->cols[set_col].type;
  for (size_t r = 0; r < tb->nrow; r++)
    {
      if (!persi__val_eq (&tb->rows[r].vals[where_col], needle, wty))
        continue;
      persi__val_free_cell (&tb->rows[r].vals[set_col], sty);
      int e = persi__val_copy_in (&tb->rows[r].vals[set_col], new_val, sty);
      if (e != 0)
        return e;
    }
  return 0;
}

int
persi_delete_row (persi_conn_t *c, const char *table, uint64_t row_id)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  for (size_t r = 0; r < tb->nrow; r++)
    {
      if (tb->rows[r].id != row_id)
        continue;
      persi__row_free_vals (tb->rows[r].vals, tb->cols, tb->ncol);
      memmove (&tb->rows[r], &tb->rows[r + 1],
               (tb->nrow - 1 - r) * sizeof (prow_t));
      tb->nrow--;
      memset (&tb->rows[tb->nrow], 0, sizeof (prow_t));
      return 0;
    }
  return PERSI_ERR_NOTFOUND;
}

int
persi_delete_where_eq (persi_conn_t *c, const char *table, size_t col_idx,
                       const persi_val_t *needle)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (col_idx >= tb->ncol)
    return PERSI_ERR_BOUNDS;
  int ty = tb->cols[col_idx].type;
  size_t w = 0;
  for (size_t r = 0; r < tb->nrow; r++)
    {
      if (persi__val_eq (&tb->rows[r].vals[col_idx], needle, ty))
        persi__row_free_vals (tb->rows[r].vals, tb->cols, tb->ncol);
      else
        {
          if (w != r)
            tb->rows[w] = tb->rows[r];
          w++;
        }
    }
  for (size_t r = w; r < tb->nrow; r++)
    memset (&tb->rows[r], 0, sizeof (prow_t));
  tb->nrow = w;
  return 0;
}

void
persi_result_free (persi_result_t *r)
{
  if (!r)
    return;
  if (r->cells && r->ncol > 0 && r->nrow > 0)
    {
      for (size_t row = 0; row < r->nrow; row++)
        for (size_t j = 0; j < r->ncol; j++)
          persi__val_free_cell (&r->cells[row * r->ncol + j], r->col_types[j]);
      free (r->cells);
    }
  free (r);
}
