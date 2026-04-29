#include "persi.h"
#include "persi_private.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void
skip_ws (const char **p)
{
  while (**p && isspace ((unsigned char)**p))
    (*p)++;
}

static int
try_kw (const char **p, const char *kw)
{
  skip_ws (p);
  const char *s = *p;
  for (const char *k = kw; *k; k++, s++)
    {
      if (tolower ((unsigned char)*s) != tolower ((unsigned char)*k))
        return 0;
    }
  if (*s && (isalnum ((unsigned char)*s) || *s == '_'))
    return 0;
  *p = s;
  return 1;
}

static int
read_sql_ident (const char **p, char *buf, size_t buflen)
{
  skip_ws (p);
  if (!**p || (!isalpha ((unsigned char)**p) && **p != '_'))
    return PERSI_ERR_PARSE;
  size_t i = 0;
  while (isalnum ((unsigned char)**p) || **p == '_')
    {
      if (i + 1 >= buflen)
        return PERSI_ERR_PARSE;
      buf[i++] = *(*p)++;
    }
  buf[i] = '\0';
  return 0;
}

static int
col_name_index_ci (ptable_t *tb, const char *name)
{
  for (size_t i = 0; i < tb->ncol; i++)
    if (persi__id_eq_ci (tb->cols[i].name, name))
      return (int)i;
  return -1;
}

static int
parse_literal (const char **p, int type_hint, persi_val_t *out)
{
  memset (out, 0, sizeof (*out));
  skip_ws (p);
  if (type_hint == PERSI_TYPE_STRING)
    {
      if (**p != '\'')
        return PERSI_ERR_PARSE;
      (*p)++;
      char buf[PERSI_MAX_STRING + 1];
      size_t n = 0;
      while (**p && **p != '\'')
        {
          if (n >= PERSI_MAX_STRING)
            return PERSI_ERR_PARSE;
          buf[n++] = *(*p)++;
        }
      if (**p != '\'')
        return PERSI_ERR_PARSE;
      (*p)++;
      char *d = (char *)malloc (n + 1);
      if (!d)
        return PERSI_ERR_NOMEM;
      memcpy (d, buf, n);
      d[n] = '\0';
      out->str_v.v = d;
      out->str_v.vl = n;
      return 0;
    }
  const char *start = *p;
  if (**p == '-')
    (*p)++;
  if (!isdigit ((unsigned char)**p))
    return PERSI_ERR_PARSE;
  while (isdigit ((unsigned char)**p))
    (*p)++;
  int is_float = 0;
  if (**p == '.')
    {
      is_float = 1;
      (*p)++;
      while (isdigit ((unsigned char)**p))
        (*p)++;
    }
  if (**p && (isalnum ((unsigned char)**p) || **p == '_'))
    return PERSI_ERR_PARSE;
  char tmp[64];
  size_t len = (size_t)(*p - start);
  if (len == 0 || len >= sizeof (tmp))
    return PERSI_ERR_PARSE;
  memcpy (tmp, start, len);
  tmp[len] = '\0';
  if (type_hint == PERSI_TYPE_INT && is_float)
    return PERSI_ERR_PARSE;
  if (type_hint == PERSI_TYPE_FLOAT || is_float)
    out->f_v = strtof (tmp, NULL);
  else
    out->i_v = (int)strtol (tmp, NULL, 10);
  return 0;
}

static int
build_select_result (persi_conn_t *c, const char *table, persi_result_t **out)
{
  ptable_t *tb = persi__find_table (c, table);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  persi_result_t *res = (persi_result_t *)calloc (1, sizeof (*res));
  if (!res)
    return PERSI_ERR_NOMEM;
  res->ncol = tb->ncol;
  res->nrow = tb->nrow;
  for (size_t j = 0; j < tb->ncol; j++)
    {
      memcpy (res->col_names[j], tb->cols[j].name, PERSI_MAX_NAME);
      res->col_types[j] = tb->cols[j].type;
    }
  if (tb->nrow == 0 || tb->ncol == 0)
    {
      *out = res;
      return 0;
    }
  res->cells
      = (persi_val_t *)calloc (tb->nrow * tb->ncol, sizeof (persi_val_t));
  if (!res->cells)
    {
      free (res);
      return PERSI_ERR_NOMEM;
    }
  for (size_t r = 0; r < tb->nrow; r++)
    for (size_t j = 0; j < tb->ncol; j++)
      {
        int e = persi__val_copy_in (&res->cells[r * tb->ncol + j],
                                    &tb->rows[r].vals[j], tb->cols[j].type);
        if (e != 0)
          {
            persi_result_free (res);
            return e;
          }
      }
  *out = res;
  return 0;
}

static int
sql_create_table (persi_conn_t *c, const char **p)
{
  if (!try_kw (p, "table"))
    return PERSI_ERR_PARSE;
  char tname[PERSI_MAX_NAME];
  if (read_sql_ident (p, tname, sizeof tname) != 0)
    return PERSI_ERR_PARSE;
  skip_ws (p);
  if (**p != '(')
    return PERSI_ERR_PARSE;
  (*p)++;
  persi_coldef_t defs[PERSI_MAX_COLS];
  size_t nd = 0;
  for (;;)
    {
      skip_ws (p);
      if (**p == ')')
        {
          (*p)++;
          break;
        }
      if (nd >= PERSI_MAX_COLS)
        return PERSI_ERR_BOUNDS;
      if (**p == ')')
        return PERSI_ERR_PARSE;
      char cname[PERSI_MAX_NAME];
      if (read_sql_ident (p, cname, sizeof cname) != 0)
        return PERSI_ERR_PARSE;
      char tyname[16];
      if (read_sql_ident (p, tyname, sizeof tyname) != 0)
        return PERSI_ERR_PARSE;
      int ty = -1;
      if (persi__id_eq_ci (tyname, "int"))
        ty = PERSI_TYPE_INT;
      else if (persi__id_eq_ci (tyname, "float"))
        ty = PERSI_TYPE_FLOAT;
      else if (persi__id_eq_ci (tyname, "string"))
        ty = PERSI_TYPE_STRING;
      else
        return PERSI_ERR_PARSE;
      memset (&defs[nd], 0, sizeof (defs[nd]));
      strncpy (defs[nd].name, cname, PERSI_MAX_NAME - 1);
      defs[nd].type = ty;
      nd++;
      skip_ws (p);
      if (**p == ',')
        {
          (*p)++;
          continue;
        }
      if (**p == ')')
        {
          (*p)++;
          break;
        }
      return PERSI_ERR_PARSE;
    }
  skip_ws (p);
  if (**p == ';')
    (*p)++;
  return persi_create_table (c, tname, defs, nd);
}

static int
sql_insert (persi_conn_t *c, const char **p)
{
  if (!try_kw (p, "into"))
    return PERSI_ERR_PARSE;
  char tname[PERSI_MAX_NAME];
  if (read_sql_ident (p, tname, sizeof tname) != 0)
    return PERSI_ERR_PARSE;
  ptable_t *tb = persi__find_table (c, tname);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (!try_kw (p, "values"))
    return PERSI_ERR_PARSE;
  skip_ws (p);
  if (**p != '(')
    return PERSI_ERR_PARSE;
  (*p)++;
  persi_val_t vals[PERSI_MAX_COLS];
  memset (vals, 0, sizeof (vals));
  for (size_t j = 0; j < tb->ncol; j++)
    {
      if (j > 0)
        {
          skip_ws (p);
          if (**p != ',')
            return PERSI_ERR_PARSE;
          (*p)++;
        }
      int e = parse_literal (p, tb->cols[j].type, &vals[j]);
      if (e != 0)
        {
          for (size_t k = 0; k < j; k++)
            persi__val_free_cell (&vals[k], tb->cols[k].type);
          return e;
        }
    }
  skip_ws (p);
  if (**p != ')')
    {
      for (size_t k = 0; k < tb->ncol; k++)
        persi__val_free_cell (&vals[k], tb->cols[k].type);
      return PERSI_ERR_PARSE;
    }
  (*p)++;
  skip_ws (p);
  if (**p == ';')
    (*p)++;
  int e = persi_insert (c, tname, vals, tb->ncol, NULL);
  for (size_t k = 0; k < tb->ncol; k++)
    persi__val_free_cell (&vals[k], tb->cols[k].type);
  return e;
}

static int
sql_update (persi_conn_t *c, const char **p)
{
  char tname[PERSI_MAX_NAME];
  if (read_sql_ident (p, tname, sizeof tname) != 0)
    return PERSI_ERR_PARSE;
  ptable_t *tb = persi__find_table (c, tname);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (!try_kw (p, "set"))
    return PERSI_ERR_PARSE;
  char scol[PERSI_MAX_NAME];
  if (read_sql_ident (p, scol, sizeof scol) != 0)
    return PERSI_ERR_PARSE;
  skip_ws (p);
  if (**p != '=')
    return PERSI_ERR_PARSE;
  (*p)++;
  int si = col_name_index_ci (tb, scol);
  if (si < 0)
    return PERSI_ERR_PARSE;
  persi_val_t newv;
  memset (&newv, 0, sizeof (newv));
  int e = parse_literal (p, tb->cols[si].type, &newv);
  if (e != 0)
    return e;
  if (!try_kw (p, "where"))
    {
      persi__val_free_cell (&newv, tb->cols[si].type);
      return PERSI_ERR_PARSE;
    }
  char wcol[PERSI_MAX_NAME];
  if (read_sql_ident (p, wcol, sizeof wcol) != 0)
    {
      persi__val_free_cell (&newv, tb->cols[si].type);
      return PERSI_ERR_PARSE;
    }
  skip_ws (p);
  if (**p != '=')
    {
      persi__val_free_cell (&newv, tb->cols[si].type);
      return PERSI_ERR_PARSE;
    }
  (*p)++;
  int wi = col_name_index_ci (tb, wcol);
  if (wi < 0)
    {
      persi__val_free_cell (&newv, tb->cols[si].type);
      return PERSI_ERR_PARSE;
    }
  persi_val_t needle;
  memset (&needle, 0, sizeof (needle));
  e = parse_literal (p, tb->cols[wi].type, &needle);
  if (e != 0)
    {
      persi__val_free_cell (&newv, tb->cols[si].type);
      return e;
    }
  e = persi_update_where_eq (c, tname, (size_t)si, &newv, (size_t)wi, &needle);
  persi__val_free_cell (&newv, tb->cols[si].type);
  persi__val_free_cell (&needle, tb->cols[wi].type);
  skip_ws (p);
  if (**p == ';')
    (*p)++;
  return e;
}

static int
sql_delete (persi_conn_t *c, const char **p)
{
  if (!try_kw (p, "from"))
    return PERSI_ERR_PARSE;
  char tname[PERSI_MAX_NAME];
  if (read_sql_ident (p, tname, sizeof tname) != 0)
    return PERSI_ERR_PARSE;
  ptable_t *tb = persi__find_table (c, tname);
  if (!tb)
    return PERSI_ERR_NOTFOUND;
  if (!try_kw (p, "where"))
    return PERSI_ERR_PARSE;
  char wcol[PERSI_MAX_NAME];
  if (read_sql_ident (p, wcol, sizeof wcol) != 0)
    return PERSI_ERR_PARSE;
  skip_ws (p);
  if (**p != '=')
    return PERSI_ERR_PARSE;
  (*p)++;
  int wi = col_name_index_ci (tb, wcol);
  if (wi < 0)
    return PERSI_ERR_PARSE;
  persi_val_t needle;
  memset (&needle, 0, sizeof (needle));
  int e = parse_literal (p, tb->cols[wi].type, &needle);
  if (e != 0)
    return e;
  e = persi_delete_where_eq (c, tname, (size_t)wi, &needle);
  persi__val_free_cell (&needle, tb->cols[wi].type);
  skip_ws (p);
  if (**p == ';')
    (*p)++;
  return e;
}

static int
sql_select (persi_conn_t *c, const char **p, persi_result_t **out_sel)
{
  skip_ws (p);
  if (**p != '*')
    return PERSI_ERR_PARSE;
  (*p)++;
  if (!try_kw (p, "from"))
    return PERSI_ERR_PARSE;
  char tname[PERSI_MAX_NAME];
  if (read_sql_ident (p, tname, sizeof tname) != 0)
    return PERSI_ERR_PARSE;
  skip_ws (p);
  if (**p == ';')
    (*p)++;
  if (!out_sel)
    return PERSI_ERR_PARSE;
  return build_select_result (c, tname, out_sel);
}

int
persi_exec_sql (persi_conn_t *c, const char *sql, persi_result_t **out_sel)
{
  if (!c || !sql)
    return PERSI_ERR;
  const char *p = sql;
  skip_ws (&p);
  if (!*p)
    return PERSI_ERR_PARSE;

  if (try_kw (&p, "create"))
    return sql_create_table (c, &p);
  if (try_kw (&p, "insert"))
    return sql_insert (c, &p);
  if (try_kw (&p, "update"))
    return sql_update (c, &p);
  if (try_kw (&p, "delete"))
    return sql_delete (c, &p);
  if (try_kw (&p, "select"))
    return sql_select (c, &p, out_sel);

  return PERSI_ERR_PARSE;
}
