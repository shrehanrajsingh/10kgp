#include "persi.h"
#include "win_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
fail (const char *msg, int code)
{
  fprintf (stderr, "%s: %s\n", msg, persi_strerror (code));
  return 1;
}

static void
print_result (const persi_result_t *r)
{
  for (size_t j = 0; j < r->ncol; j++)
    printf ("%s%s", r->col_names[j], j + 1 < r->ncol ? "\t" : "\n");
  for (size_t i = 0; i < r->nrow; i++)
    {
      for (size_t j = 0; j < r->ncol; j++)
        {
          const persi_val_t *v = &r->cells[i * r->ncol + j];
          switch (r->col_types[j])
            {
            case PERSI_TYPE_INT:
              printf ("%d", v->i_v);
              break;
            case PERSI_TYPE_FLOAT:
              printf ("%g", (double)v->f_v);
              break;
            case PERSI_TYPE_STRING:
              printf ("%.*s", (int)v->str_v.vl, v->str_v.v ? v->str_v.v : "");
              break;
            default:
              printf ("?");
            }
          printf ("%s", j + 1 < r->ncol ? "\t" : "\n");
        }
    }
}

static void
count_rows_cb (void *ud, uint64_t row_id, const persi_val_t *vals, size_t n)
{
  (void)row_id;
  (void)vals;
  (void)n;
  size_t *c = (size_t *)ud;
  (*c)++;
}

int
main (int argc, char **argv)
{
  char path[256];
  if (argc > 1)
    {
      strncpy (path, argv[1], sizeof path - 1);
      path[sizeof path - 1] = '\0';
    }
  else
    strncpy (path, "/tmp/persi_demo.db", sizeof path);

  app_unlink (path);

  persi_conn_t *db = NULL;
  int e = persi_open (path, &db);
  if (e != 0 || !db)
    return fail ("persi_open (new)", e);

  persi_coldef_t cols[] = {
    { .name = "id", .type = PERSI_TYPE_INT },
    { .name = "label", .type = PERSI_TYPE_STRING },
    { .name = "score", .type = PERSI_TYPE_FLOAT },
  };
  e = persi_create_table (db, "items", cols, 3);
  if (e != 0)
    return fail ("persi_create_table", e);

  persi_val_t row1[] = {
    { .i_v = 1 },
    { .str_v = { .v = "alpha", .vl = 5 } },
    { .f_v = 1.5f },
  };
  uint64_t rid = 0;
  e = persi_insert (db, "items", row1, 3, &rid);
  if (e != 0 || rid != 1)
    return fail ("persi_insert", e);

  size_t nscan = 0;
  e = persi_scan_table (db, "items", count_rows_cb, &nscan);
  if (e != 0 || nscan != 1)
    {
      fprintf (stderr, "scan: e=%d nscan=%zu\n", e, nscan);
      persi_close (db);
      return 1;
    }

  e = persi_sync (db);
  if (e != 0)
    return fail ("persi_sync", e);
  persi_close (db);
  db = NULL;

  e = persi_open (path, &db);
  if (e != 0 || !db)
    return fail ("persi_open (reload)", e);

  nscan = 0;
  e = persi_scan_table (db, "items", count_rows_cb, &nscan);
  if (e != 0 || nscan != 1)
    {
      fprintf (stderr, "scan after reload: e=%d n=%zu\n", e, nscan);
      persi_close (db);
      return 1;
    }

  e = persi_exec_sql (db, "INSERT INTO items VALUES (2, 'beta', 2.25);", NULL);
  if (e != 0)
    return fail ("exec_sql insert", e);

  persi_result_t *res = NULL;
  e = persi_exec_sql (db, "SELECT * FROM items;", &res);
  if (e != 0 || !res)
    {
      persi_close (db);
      return fail ("exec_sql select", e);
    }
  if (res->nrow != 2 || res->ncol != 3)
    {
      fprintf (stderr, "expected 2 rows 3 cols, got %zu %zu\n", res->nrow,
               res->ncol);
      persi_result_free (res);
      persi_close (db);
      return 1;
    }
  print_result (res);
  persi_result_free (res);

  e = persi_exec_sql (db, "UPDATE items SET score = 9 WHERE id = 1;", NULL);
  if (e != 0)
    return fail ("exec_sql update", e);

  e = persi_exec_sql (db, "DELETE FROM items WHERE id = 2;", NULL);
  if (e != 0)
    return fail ("exec_sql delete", e);

  e = persi_sync (db);
  if (e != 0)
    return fail ("persi_sync", e);
  persi_close (db);

  e = persi_open (path, &db);
  if (e != 0)
    return fail ("persi_open final", e);

  res = NULL;
  e = persi_exec_sql (db, "SELECT * FROM items;", &res);
  if (e != 0 || !res || res->nrow != 1)
    {
      fprintf (stderr, "final select: nrow=%zu\n", res ? res->nrow : 0);
      if (res)
        persi_result_free (res);
      persi_close (db);
      return 1;
    }
  persi_val_t *v0 = &res->cells[0];
  persi_val_t *v1 = &res->cells[1];
  persi_val_t *v2 = &res->cells[2];
  if (v0->i_v != 1 || v1->str_v.vl != 5
      || strncmp (v1->str_v.v, "alpha", 5) != 0 || v2->f_v != 9.0f)
    {
      fprintf (stderr, "data mismatch after reopen (score=%g)\n",
               (double)v2->f_v);
      persi_result_free (res);
      persi_close (db);
      return 1;
    }
  persi_result_free (res);
  persi_close (db);
  printf ("persi_demo: ok\n");
  return 0;
}
