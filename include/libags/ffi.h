#ifndef LIBAGS_FFI_H
#define LIBAGS_FFI_H

#include <stddef.h>
#include <stdint.h>

#include "libags/document.h"
#include "libags/export.h"
#include "libags/status.h"
#include "libags/tabular.h"
#include "libags/version.h"

LIBAGS_EXTERN_C_BEGIN

/* Borrowed view types remain valid only while the owning libags handle lives unchanged. */
typedef struct ags_string_view {
  const char *data;
  size_t length;
} ags_string_view;

typedef struct ags_bytes_view {
  const unsigned char *data;
  size_t length;
} ags_bytes_view;

typedef struct ags_row_cursor {
  const ags_document *document;
  size_t group_index;
  size_t row_index;
  int has_row;
} ags_row_cursor;

typedef struct ags_table_column_export {
  ags_string_view group_name;
  ags_string_view column_name;
  ags_string_view unit;
  ags_string_view type;
  size_t row_count;
  const char *const *values;
} ags_table_column_export;

typedef struct ags_numeric_export {
  size_t count;
  const double *values;
  const unsigned char *is_null;
} ags_numeric_export;

typedef struct ags_geometry_export {
  ags_string_view column_name;
  ags_geometry_encoding encoding;
  size_t row_count;
  size_t invalid_row_count;
  int srid;
  ags_string_view crs;
  const unsigned char *is_null;
  const char *const *wkt_values;
  const unsigned char *const *wkb_values;
  const size_t *wkb_lengths;
} ags_geometry_export;

LIBAGS_API int ags_ffi_supports_abi(uint32_t abi_version);
LIBAGS_API ags_status ags_status_string_view(ags_status status, ags_string_view *out_view);
LIBAGS_API ags_status ags_version_string_view(ags_string_view *out_view);

LIBAGS_API ags_status ags_document_group_name_view(
  const ags_document *document,
  size_t group_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_document_field_name_view(
  const ags_document *document,
  size_t group_index,
  size_t field_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_document_field_unit_view(
  const ags_document *document,
  size_t group_index,
  size_t field_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_document_field_type_view(
  const ags_document *document,
  size_t group_index,
  size_t field_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_document_cell_value_view(
  const ags_document *document,
  size_t group_index,
  size_t row_index,
  size_t field_index,
  ags_string_view *out_view
);

LIBAGS_API ags_status ags_table_group_name_view(
  const ags_table *table,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_table_column_name_view(
  const ags_table *table,
  size_t column_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_table_column_unit_view(
  const ags_table *table,
  size_t column_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_table_column_type_view(
  const ags_table *table,
  size_t column_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_table_cell_value_view(
  const ags_table *table,
  size_t row_index,
  size_t column_index,
  ags_string_view *out_view
);

LIBAGS_API ags_status ags_row_cursor_init(
  ags_row_cursor *cursor,
  const ags_document *document,
  size_t group_index
);
LIBAGS_API int ags_row_cursor_is_valid(const ags_row_cursor *cursor);
LIBAGS_API ags_status ags_row_cursor_next(ags_row_cursor *cursor);
LIBAGS_API size_t ags_row_cursor_row_index(const ags_row_cursor *cursor);
LIBAGS_API size_t ags_row_cursor_line_number(const ags_row_cursor *cursor);
LIBAGS_API ags_status ags_row_cursor_cell_value_view(
  const ags_row_cursor *cursor,
  size_t field_index,
  ags_string_view *out_view
);

LIBAGS_API ags_status ags_table_get_column_export(
  const ags_table *table,
  size_t column_index,
  ags_table_column_export *out_export
);
LIBAGS_API ags_status ags_numeric_column_get_export(
  const ags_numeric_column *numeric,
  ags_numeric_export *out_export
);
LIBAGS_API ags_status ags_geometry_column_get_export(
  const ags_geometry_column *geometry,
  ags_geometry_export *out_export
);
LIBAGS_API ags_status ags_geometry_column_wkt_view(
  const ags_geometry_column *geometry,
  size_t row_index,
  ags_string_view *out_view
);
LIBAGS_API ags_status ags_geometry_column_wkb_view(
  const ags_geometry_column *geometry,
  size_t row_index,
  ags_bytes_view *out_view
);

LIBAGS_EXTERN_C_END

#endif
