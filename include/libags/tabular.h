#ifndef LIBAGS_TABULAR_H
#define LIBAGS_TABULAR_H

#include <stddef.h>

#include "libags/allocator.h"
#include "libags/dictionary.h"
#include "libags/document.h"
#include "libags/export.h"
#include "libags/status.h"

LIBAGS_EXTERN_C_BEGIN

typedef struct ags_table ags_table;
typedef struct ags_numeric_column ags_numeric_column;
typedef struct ags_geometry_column ags_geometry_column;

typedef enum ags_duplicate_heading_policy {
  AGS_DUPLICATE_HEADING_REJECT = 0,
  AGS_DUPLICATE_HEADING_RENAME = 1,
  AGS_DUPLICATE_HEADING_CALLBACK = 2
} ags_duplicate_heading_policy;

typedef ags_status (*ags_duplicate_heading_resolver_fn)(
  void *user_data,
  const char *group_name,
  const char *original_name,
  size_t duplicate_index,
  const ags_allocator *allocator,
  char **out_heading_name
);

typedef struct ags_table_options {
  size_t struct_size;
  const ags_allocator *allocator;
  ags_duplicate_heading_policy duplicate_heading_policy;
  ags_duplicate_heading_resolver_fn duplicate_heading_resolver;
  void *duplicate_heading_user_data;
} ags_table_options;

typedef enum ags_sort_strategy {
  AGS_SORT_INPUT = 0,
  AGS_SORT_ALPHABETICAL = 1,
  AGS_SORT_DICTIONARY = 2,
  AGS_SORT_HIERARCHICAL = 3
} ags_sort_strategy;

typedef struct ags_sort_options {
  size_t struct_size;
  const ags_allocator *allocator;
  ags_sort_strategy strategy;
  const char *dictionary_version;
  const ags_document *dictionary_document;
} ags_sort_options;

typedef enum ags_geometry_encoding {
  AGS_GEOMETRY_WKT = 0,
  AGS_GEOMETRY_WKB = 1
} ags_geometry_encoding;

typedef enum ags_invalid_coordinate_policy {
  AGS_INVALID_COORDINATES_FAIL = 0,
  AGS_INVALID_COORDINATES_WARN = 1,
  AGS_INVALID_COORDINATES_NULL = 2
} ags_invalid_coordinate_policy;

typedef struct ags_geometry_options {
  size_t struct_size;
  const ags_allocator *allocator;
  const char *geometry_column_name;
  ags_geometry_encoding encoding;
  const char *easting_column_name;
  const char *northing_column_name;
  int srid;
  const char *crs;
  ags_invalid_coordinate_policy invalid_coordinate_policy;
} ags_geometry_options;

LIBAGS_API ags_status ags_table_options_init(ags_table_options *options);
LIBAGS_API ags_status ags_sort_options_init(ags_sort_options *options);
LIBAGS_API ags_status ags_geometry_options_init(ags_geometry_options *options);

LIBAGS_API ags_status ags_table_create(
  const char *group_name,
  size_t column_count,
  const char *const *column_names,
  const char *const *units,
  const char *const *types,
  const ags_table_options *options,
  ags_table **out_table
);
LIBAGS_API ags_status ags_table_append_row(
  ags_table *table,
  const char *const *values,
  size_t value_count
);
LIBAGS_API ags_status ags_table_from_group(
  const ags_document *document,
  size_t group_index,
  const ags_table_options *options,
  ags_table **out_table
);
LIBAGS_API ags_status ags_document_from_tables(
  const ags_table *const *tables,
  size_t table_count,
  const ags_document_options *options,
  ags_document **out_document
);
LIBAGS_API void ags_table_destroy(ags_table *table);
LIBAGS_API const ags_allocator *ags_table_allocator(const ags_table *table);
LIBAGS_API const char *ags_table_group_name(const ags_table *table);
LIBAGS_API size_t ags_table_column_count(const ags_table *table);
LIBAGS_API size_t ags_table_row_count(const ags_table *table);
LIBAGS_API const char *ags_table_column_name(const ags_table *table, size_t column_index);
LIBAGS_API const char *ags_table_column_unit(const ags_table *table, size_t column_index);
LIBAGS_API const char *ags_table_column_type(const ags_table *table, size_t column_index);
LIBAGS_API const char *ags_table_cell_value(
  const ags_table *table,
  size_t row_index,
  size_t column_index
);
LIBAGS_API const char *const *ags_table_column_values(
  const ags_table *table,
  size_t column_index
);

LIBAGS_API ags_status ags_table_column_to_numeric(
  const ags_table *table,
  size_t column_index,
  ags_numeric_column **out_numeric
);
LIBAGS_API ags_status ags_table_column_from_numeric(
  ags_table *table,
  size_t column_index,
  const ags_numeric_column *numeric
);
LIBAGS_API void ags_numeric_column_destroy(ags_numeric_column *numeric);
LIBAGS_API size_t ags_numeric_column_count(const ags_numeric_column *numeric);
LIBAGS_API int ags_numeric_column_is_null(const ags_numeric_column *numeric, size_t row_index);
LIBAGS_API double ags_numeric_column_value(const ags_numeric_column *numeric, size_t row_index);
LIBAGS_API ags_status ags_numeric_column_set_value(
  ags_numeric_column *numeric,
  size_t row_index,
  double value
);
LIBAGS_API ags_status ags_numeric_column_set_null(
  ags_numeric_column *numeric,
  size_t row_index,
  int is_null
);

LIBAGS_API ags_status ags_document_sort_groups(
  const ags_document *document,
  const ags_sort_options *options,
  ags_document **out_document
);

LIBAGS_API ags_status ags_table_derive_geometry(
  const ags_table *table,
  const ags_geometry_options *options,
  ags_geometry_column **out_geometry
);
LIBAGS_API void ags_geometry_column_destroy(ags_geometry_column *geometry);
LIBAGS_API const char *ags_geometry_column_name(const ags_geometry_column *geometry);
LIBAGS_API ags_geometry_encoding ags_geometry_column_encoding(const ags_geometry_column *geometry);
LIBAGS_API size_t ags_geometry_column_row_count(const ags_geometry_column *geometry);
LIBAGS_API size_t ags_geometry_column_invalid_row_count(const ags_geometry_column *geometry);
LIBAGS_API int ags_geometry_column_srid(const ags_geometry_column *geometry);
LIBAGS_API const char *ags_geometry_column_crs(const ags_geometry_column *geometry);
LIBAGS_API int ags_geometry_column_is_null(const ags_geometry_column *geometry, size_t row_index);
LIBAGS_API const char *ags_geometry_column_wkt(
  const ags_geometry_column *geometry,
  size_t row_index
);
LIBAGS_API const unsigned char *ags_geometry_column_wkb(
  const ags_geometry_column *geometry,
  size_t row_index,
  size_t *out_length
);

LIBAGS_EXTERN_C_END

#endif
