#ifndef LIBAGS_TABULAR_INTERNAL_H
#define LIBAGS_TABULAR_INTERNAL_H

#include "libags/tabular.h"

typedef struct ags_table_column_internal {
  char *name;
  char *unit;
  char *type;
  char **values;
} ags_table_column_internal;

struct ags_table {
  ags_allocator allocator;
  char *group_name;
  size_t column_count;
  size_t row_count;
  size_t row_capacity;
  ags_table_column_internal *columns;
};

struct ags_numeric_column {
  ags_allocator allocator;
  size_t count;
  double *values;
  unsigned char *is_null;
};

struct ags_geometry_column {
  ags_allocator allocator;
  char *name;
  ags_geometry_encoding encoding;
  size_t row_count;
  size_t invalid_row_count;
  int srid;
  char *crs;
  unsigned char *is_null;
  char **wkt_values;
  unsigned char **wkb_values;
  size_t *wkb_lengths;
};

#endif
