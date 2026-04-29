#ifndef PERSI_H
#define PERSI_H

#include "../header.h"
#include <stdint.h>
#include <stdio.h>

enum
{
  PERSI_TYPE_INT = 0,
  PERSI_TYPE_FLOAT = 1,
  PERSI_TYPE_STRING = 2,
  PERSI_TYPE_COUNT,
};

enum
{
  PERSI_OK = 0,
  PERSI_ERR = -1,
  PERSI_ERR_IO = -2,
  PERSI_ERR_NOMEM = -3,
  PERSI_ERR_EXISTS = -4,
  PERSI_ERR_NOTFOUND = -5,
  PERSI_ERR_SCHEMA = -6,
  PERSI_ERR_BOUNDS = -7,
  PERSI_ERR_PARSE = -8,
  PERSI_ERR_FORMAT = -9,
};

#define PERSI_MAX_COLS 16
#define PERSI_MAX_TABLES 64
#define PERSI_MAX_NAME 32
#define PERSI_MAX_STRING 4095

typedef union persi_val
{
  int i_v;
  float f_v;
  struct
  {
    char *v;
    size_t vl;
  } str_v;
} persi_val_t;

typedef struct persi_coldef
{
  char name[PERSI_MAX_NAME];
  int type;
} persi_coldef_t;

typedef struct persi_conn persi_conn_t;

typedef struct persi_result
{
  char col_names[PERSI_MAX_COLS][PERSI_MAX_NAME];
  int col_types[PERSI_MAX_COLS];
  size_t ncol;
  size_t nrow;
  persi_val_t *cells;
} persi_result_t;

typedef void (*persi_row_fn) (void *ud, uint64_t row_id,
                              const persi_val_t *vals, size_t n);

#ifdef __cplusplus
extern "C"
{
#endif

  const char *persi_strerror (int code);

  int persi_open (const char *path, persi_conn_t **out);
  void persi_close (persi_conn_t *c);
  int persi_sync (persi_conn_t *c);

  int persi_create_table (persi_conn_t *c, const char *table,
                          const persi_coldef_t *cols, size_t ncols);

  int persi_insert (persi_conn_t *c, const char *table,
                    const persi_val_t *vals, size_t n, uint64_t *out_row_id);

  int persi_scan_table (persi_conn_t *c, const char *table, persi_row_fn fn,
                        void *ud);

  int persi_scan_where_eq (persi_conn_t *c, const char *table, size_t col_idx,
                           const persi_val_t *needle, persi_row_fn fn,
                           void *ud);

  int persi_update_row (persi_conn_t *c, const char *table, uint64_t row_id,
                        const persi_val_t *vals, size_t n);

  int persi_update_where_eq (persi_conn_t *c, const char *table,
                             size_t set_col, const persi_val_t *new_val,
                             size_t where_col, const persi_val_t *needle);

  int persi_delete_row (persi_conn_t *c, const char *table, uint64_t row_id);

  int persi_delete_where_eq (persi_conn_t *c, const char *table,
                             size_t col_idx, const persi_val_t *needle);

  void persi_result_free (persi_result_t *r);

  int persi_exec_sql (persi_conn_t *c, const char *sql,
                      persi_result_t **out_sel);

#ifdef __cplusplus
}
#endif

#endif
