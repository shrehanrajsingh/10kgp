#include "persi_private.h"
#include <stdlib.h>
#include <string.h>

int
persi__format_save (persi_conn_t *c)
{
  FILE *f = fopen (c->path, "wb");
  if (!f)
    return PERSI_ERR_IO;

  const char magic[5] = { 'P', 'E', 'R', 'S', 'I' };
  if (fwrite (magic, 1, 5, f) != 5)
    goto ioerr;
  persi__io_write_u16_le (f, 1);
  persi__io_write_u16_le (f, 0);

  persi__io_write_u32_le (f, (uint32_t)c->ntab);

  for (size_t t = 0; t < c->ntab; t++)
    {
      ptable_t *tb = &c->tabs[t];
      char zname[PERSI_MAX_NAME] = { 0 };
      strncpy (zname, tb->name, PERSI_MAX_NAME - 1);
      if (fwrite (zname, 1, PERSI_MAX_NAME, f) != PERSI_MAX_NAME)
        goto ioerr;
      persi__io_write_u32_le (f, (uint32_t)tb->ncol);
      for (size_t j = 0; j < tb->ncol; j++)
        {
          char zc[PERSI_MAX_NAME] = { 0 };
          strncpy (zc, tb->cols[j].name, PERSI_MAX_NAME - 1);
          if (fwrite (zc, 1, PERSI_MAX_NAME, f) != PERSI_MAX_NAME)
            goto ioerr;
          persi__io_write_u8 (f, (uint8_t)tb->cols[j].type);
          persi__io_write_u8 (f, 0);
          persi__io_write_u8 (f, 0);
          persi__io_write_u8 (f, 0);
        }
    }

  for (size_t t = 0; t < c->ntab; t++)
    {
      ptable_t *tb = &c->tabs[t];
      persi__io_write_u64_le (f, (uint64_t)tb->nrow);
      for (size_t r = 0; r < tb->nrow; r++)
        {
          persi__io_write_u64_le (f, tb->rows[r].id);
          for (size_t j = 0; j < tb->ncol; j++)
            {
              int e = persi__val_write_cell (f, &tb->rows[r].vals[j],
                                             tb->cols[j].type);
              if (e != 0)
                {
                  fclose (f);
                  return e;
                }
            }
        }
    }

  if (fflush (f) != 0)
    goto ioerr;
  if (fclose (f) != 0)
    return PERSI_ERR_IO;
  return 0;

ioerr:
  fclose (f);
  return PERSI_ERR_IO;
}

int
persi__format_load (persi_conn_t *c)
{
  FILE *f = fopen (c->path, "rb");
  if (!f)
    return PERSI_ERR_IO;

  char magic[5];
  if (fread (magic, 1, 5, f) != 5)
    {
      fclose (f);
      return PERSI_ERR_FORMAT;
    }
  if (memcmp (magic, "PERSI", 5) != 0)
    {
      fclose (f);
      return PERSI_ERR_FORMAT;
    }
  uint16_t ver, pad;
  if (persi__io_read_u16_le (f, &ver) != 0
      || persi__io_read_u16_le (f, &pad) != 0)
    {
      fclose (f);
      return PERSI_ERR_IO;
    }
  if (ver != 1)
    {
      fclose (f);
      return PERSI_ERR_FORMAT;
    }

  uint32_t ntab;
  if (persi__io_read_u32_le (f, &ntab) != 0)
    {
      fclose (f);
      return PERSI_ERR_IO;
    }
  if (ntab > PERSI_MAX_TABLES)
    {
      fclose (f);
      return PERSI_ERR_FORMAT;
    }

  persi__free_conn_tables (c);
  c->ntab = ntab;
  c->ntab_cap = ntab > 0 ? ntab : 4;
  c->tabs = (ptable_t *)calloc (c->ntab_cap, sizeof (ptable_t));
  if (!c->tabs)
    {
      fclose (f);
      return PERSI_ERR_NOMEM;
    }

  uint64_t max_id = 0;

  for (uint32_t t = 0; t < ntab; t++)
    {
      ptable_t *tb = &c->tabs[t];
      if (fread (tb->name, 1, PERSI_MAX_NAME, f) != PERSI_MAX_NAME)
        goto bad;
      tb->name[PERSI_MAX_NAME - 1] = '\0';
      uint32_t ncol;
      if (persi__io_read_u32_le (f, &ncol) != 0)
        goto bad;
      if (ncol == 0 || ncol > PERSI_MAX_COLS)
        goto bad;
      tb->ncol = ncol;
      for (uint32_t j = 0; j < ncol; j++)
        {
          if (fread (tb->cols[j].name, 1, PERSI_MAX_NAME, f) != PERSI_MAX_NAME)
            goto bad;
          tb->cols[j].name[PERSI_MAX_NAME - 1] = '\0';
          uint8_t ty, p1, p2, p3;
          if (persi__io_read_u8 (f, &ty) != 0
              || persi__io_read_u8 (f, &p1) != 0
              || persi__io_read_u8 (f, &p2) != 0
              || persi__io_read_u8 (f, &p3) != 0)
            goto bad;
          (void)p1;
          (void)p2;
          (void)p3;
          if (ty >= (uint8_t)PERSI_TYPE_COUNT)
            goto bad;
          tb->cols[j].type = (int)ty;
        }
    }

  for (uint32_t t = 0; t < ntab; t++)
    {
      ptable_t *tb = &c->tabs[t];
      uint64_t nrow;
      if (persi__io_read_u64_le (f, &nrow) != 0)
        goto bad;
      tb->nrow = (size_t)nrow;
      tb->nrow_cap = tb->nrow ? tb->nrow : 4;
      tb->rows = (prow_t *)calloc (tb->nrow_cap, sizeof (prow_t));
      if (!tb->rows)
        goto bad;
      for (uint64_t r = 0; r < nrow; r++)
        {
          uint64_t rid;
          if (persi__io_read_u64_le (f, &rid) != 0)
            goto bad;
          tb->rows[r].id = rid;
          if (rid > max_id)
            max_id = rid;
          tb->rows[r].vals
              = (persi_val_t *)calloc (tb->ncol, sizeof (persi_val_t));
          if (!tb->rows[r].vals)
            goto bad;
          for (size_t j = 0; j < tb->ncol; j++)
            {
              int e = persi__val_read_cell (f, &tb->rows[r].vals[j],
                                            tb->cols[j].type);
              if (e != 0)
                goto bad;
            }
        }
    }

  fclose (f);
  c->next_id = max_id + 1;
  if (c->next_id == 0)
    c->next_id = 1;
  return 0;

bad:
  fclose (f);
  persi__free_conn_tables (c);
  c->next_id = 1;
  return PERSI_ERR_FORMAT;
}
