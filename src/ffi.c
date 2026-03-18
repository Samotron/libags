#include <stddef.h>
#include <string.h>

#include "libags/ffi.h"
#include "document_internal.h"
#include "tabular_internal.h"

static void clear_string_view(ags_string_view *view) {
  if (view == NULL) {
    return;
  }

  view->data = NULL;
  view->length = 0;
}

static void clear_bytes_view(ags_bytes_view *view) {
  if (view == NULL) {
    return;
  }

  view->data = NULL;
  view->length = 0;
}

static ags_status set_required_string_view(const char *value, ags_string_view *out_view) {
  if (out_view == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (value == NULL) {
    clear_string_view(out_view);
    return AGS_STATUS_NOT_FOUND;
  }

  out_view->data = value;
  out_view->length = strlen(value);
  return AGS_STATUS_OK;
}

static void set_optional_string_view(const char *value, ags_string_view *out_view) {
  if (out_view == NULL) {
    return;
  }

  if (value == NULL) {
    clear_string_view(out_view);
    return;
  }

  out_view->data = value;
  out_view->length = strlen(value);
}

static void set_summary_string_view(const char *value, ags_string_view *out_view) {
  set_optional_string_view(value, out_view);
}

int ags_ffi_supports_abi(uint32_t abi_version) {
  return abi_version == (uint32_t)ags_abi_version();
}

ags_status ags_status_string_view(ags_status status, ags_string_view *out_view) {
  return set_required_string_view(ags_status_string(status), out_view);
}

ags_status ags_version_string_view(ags_string_view *out_view) {
  return set_required_string_view(ags_version_string(), out_view);
}

ags_status ags_document_group_name_view(
  const ags_document *document,
  size_t group_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_document_group_name(document, group_index), out_view);
}

ags_status ags_document_field_name_view(
  const ags_document *document,
  size_t group_index,
  size_t field_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_document_field_name(document, group_index, field_index), out_view);
}

ags_status ags_document_field_unit_view(
  const ags_document *document,
  size_t group_index,
  size_t field_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_document_field_unit(document, group_index, field_index), out_view);
}

ags_status ags_document_field_type_view(
  const ags_document *document,
  size_t group_index,
  size_t field_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_document_field_type(document, group_index, field_index), out_view);
}

ags_status ags_document_cell_value_view(
  const ags_document *document,
  size_t group_index,
  size_t row_index,
  size_t field_index,
  ags_string_view *out_view
) {
  return set_required_string_view(
    ags_document_cell_value(document, group_index, row_index, field_index),
    out_view
  );
}

ags_status ags_table_group_name_view(
  const ags_table *table,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_table_group_name(table), out_view);
}

ags_status ags_table_column_name_view(
  const ags_table *table,
  size_t column_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_table_column_name(table, column_index), out_view);
}

ags_status ags_table_column_unit_view(
  const ags_table *table,
  size_t column_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_table_column_unit(table, column_index), out_view);
}

ags_status ags_table_column_type_view(
  const ags_table *table,
  size_t column_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_table_column_type(table, column_index), out_view);
}

ags_status ags_table_cell_value_view(
  const ags_table *table,
  size_t row_index,
  size_t column_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_table_cell_value(table, row_index, column_index), out_view);
}

ags_status ags_row_cursor_init(
  ags_row_cursor *cursor,
  const ags_document *document,
  size_t group_index
) {
  if (cursor == NULL || document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (group_index >= ags_document_group_count(document)) {
    cursor->document = NULL;
    cursor->group_index = 0;
    cursor->row_index = 0;
    cursor->has_row = 0;
    return AGS_STATUS_NOT_FOUND;
  }

  cursor->document = document;
  cursor->group_index = group_index;
  cursor->row_index = 0;
  cursor->has_row = 0;
  return AGS_STATUS_OK;
}

int ags_row_cursor_is_valid(const ags_row_cursor *cursor) {
  if (cursor == NULL) {
    return 0;
  }

  return cursor->document != NULL && cursor->has_row != 0;
}

ags_status ags_row_cursor_next(ags_row_cursor *cursor) {
  size_t row_count = 0;
  size_t next_index = 0;

  if (cursor == NULL || cursor->document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  row_count = ags_document_group_row_count(cursor->document, cursor->group_index);
  next_index = cursor->has_row != 0 ? cursor->row_index + 1 : 0;
  if (next_index >= row_count) {
    cursor->row_index = row_count;
    cursor->has_row = 0;
    return AGS_STATUS_NOT_FOUND;
  }

  cursor->row_index = next_index;
  cursor->has_row = 1;
  return AGS_STATUS_OK;
}

size_t ags_row_cursor_row_index(const ags_row_cursor *cursor) {
  if (!ags_row_cursor_is_valid(cursor)) {
    return (size_t)-1;
  }

  return cursor->row_index;
}

size_t ags_row_cursor_line_number(const ags_row_cursor *cursor) {
  if (!ags_row_cursor_is_valid(cursor)) {
    return 0;
  }

  return ags_document_row_line_number(cursor->document, cursor->group_index, cursor->row_index);
}

ags_status ags_row_cursor_cell_value_view(
  const ags_row_cursor *cursor,
  size_t field_index,
  ags_string_view *out_view
) {
  if (!ags_row_cursor_is_valid(cursor)) {
    clear_string_view(out_view);
    return AGS_STATUS_NOT_FOUND;
  }

  return ags_document_cell_value_view(
    cursor->document,
    cursor->group_index,
    cursor->row_index,
    field_index,
    out_view
  );
}

ags_status ags_table_get_column_export(
  const ags_table *table,
  size_t column_index,
  ags_table_column_export *out_export
) {
  ags_status status = AGS_STATUS_OK;

  if (table == NULL || out_export == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(out_export, 0, sizeof(*out_export));
  status = ags_table_group_name_view(table, &out_export->group_name);
  if (status != AGS_STATUS_OK) {
    return status;
  }
  status = ags_table_column_name_view(table, column_index, &out_export->column_name);
  if (status != AGS_STATUS_OK) {
    return status;
  }
  status = ags_table_column_unit_view(table, column_index, &out_export->unit);
  if (status != AGS_STATUS_OK) {
    return status;
  }
  status = ags_table_column_type_view(table, column_index, &out_export->type);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  out_export->row_count = ags_table_row_count(table);
  out_export->values = ags_table_column_values(table, column_index);
  return AGS_STATUS_OK;
}

ags_status ags_table_get_summary_export(
  const ags_table *table,
  ags_table_summary_export *out_export
) {
  ags_table_summary summary;
  ags_status status = AGS_STATUS_OK;

  if (table == NULL || out_export == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(out_export, 0, sizeof(*out_export));
  status = ags_table_get_summary(table, &summary);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  out_export->group_index = summary.group_index;
  set_summary_string_view(summary.group_name, &out_export->group_name);
  out_export->column_count = summary.column_count;
  out_export->row_count = summary.row_count;
  out_export->group_line_number = summary.group_line_number;
  out_export->heading_line_number = summary.heading_line_number;
  out_export->unit_line_number = summary.unit_line_number;
  out_export->type_line_number = summary.type_line_number;
  out_export->numeric_column_count = summary.numeric_column_count;
  out_export->geometry_candidate_count = summary.geometry_candidate_count;
  out_export->has_source_metadata = summary.has_source_metadata;
  return AGS_STATUS_OK;
}

ags_status ags_table_collection_get_summary_export(
  const ags_table_collection *collection,
  size_t table_index,
  ags_table_summary_export *out_export
) {
  const ags_table *table = ags_table_collection_get(collection, table_index);

  if (table == NULL || out_export == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  return ags_table_get_summary_export(table, out_export);
}

ags_status ags_table_get_column_summary_export(
  const ags_table *table,
  size_t column_index,
  ags_column_summary_export *out_export
) {
  ags_column_summary summary;
  ags_status status = AGS_STATUS_OK;

  if (table == NULL || out_export == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(out_export, 0, sizeof(*out_export));
  status = ags_table_get_column_summary(table, column_index, &summary);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  out_export->column_index = summary.column_index;
  set_summary_string_view(summary.column_name, &out_export->column_name);
  set_summary_string_view(summary.unit, &out_export->unit);
  set_summary_string_view(summary.type, &out_export->type);
  out_export->column_class = summary.column_class;
  out_export->null_count = summary.null_count;
  out_export->non_null_count = summary.non_null_count;
  out_export->is_numeric = summary.is_numeric;
  out_export->can_derive_geometry = summary.can_derive_geometry;
  return AGS_STATUS_OK;
}

ags_status ags_numeric_column_get_export(
  const ags_numeric_column *numeric,
  ags_numeric_export *out_export
) {
  if (numeric == NULL || out_export == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  out_export->count = numeric->count;
  out_export->values = numeric->values;
  out_export->is_null = numeric->is_null;
  return AGS_STATUS_OK;
}

ags_status ags_geometry_column_get_export(
  const ags_geometry_column *geometry,
  ags_geometry_export *out_export
) {
  if (geometry == NULL || out_export == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(out_export, 0, sizeof(*out_export));
  set_optional_string_view(geometry->name, &out_export->column_name);
  out_export->encoding = geometry->encoding;
  out_export->row_count = geometry->row_count;
  out_export->invalid_row_count = geometry->invalid_row_count;
  out_export->srid = geometry->srid;
  set_optional_string_view(geometry->crs, &out_export->crs);
  out_export->is_null = geometry->is_null;
  out_export->wkt_values = (const char *const *)geometry->wkt_values;
  out_export->wkb_values = (const unsigned char *const *)geometry->wkb_values;
  out_export->wkb_lengths = geometry->wkb_lengths;
  return AGS_STATUS_OK;
}

ags_status ags_geometry_column_wkt_view(
  const ags_geometry_column *geometry,
  size_t row_index,
  ags_string_view *out_view
) {
  return set_required_string_view(ags_geometry_column_wkt(geometry, row_index), out_view);
}

ags_status ags_geometry_column_wkb_view(
  const ags_geometry_column *geometry,
  size_t row_index,
  ags_bytes_view *out_view
) {
  size_t length = 0;
  const unsigned char *data = NULL;

  if (out_view == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  clear_bytes_view(out_view);
  data = ags_geometry_column_wkb(geometry, row_index, &length);
  if (data == NULL) {
    return AGS_STATUS_NOT_FOUND;
  }

  out_view->data = data;
  out_view->length = length;
  return AGS_STATUS_OK;
}

ags_status ags_validation_report_get_diagnostic_export(
  const ags_validation_report *report,
  size_t diagnostic_index,
  ags_validation_diagnostic_export *out_export
) {
  if (report == NULL || out_export == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(out_export, 0, sizeof(*out_export));
  if (diagnostic_index >= ags_validation_report_diagnostic_count(report)) {
    return AGS_STATUS_NOT_FOUND;
  }

  out_export->severity = ags_validation_report_diagnostic_severity(report, diagnostic_index);
  out_export->line_number = ags_validation_report_diagnostic_line_number(report, diagnostic_index);
  set_summary_string_view(ags_validation_report_diagnostic_rule(report, diagnostic_index), &out_export->rule);
  set_summary_string_view(ags_validation_report_diagnostic_message(report, diagnostic_index), &out_export->message);
  set_summary_string_view(ags_validation_report_diagnostic_group(report, diagnostic_index), &out_export->group);
  set_summary_string_view(ags_validation_report_diagnostic_field(report, diagnostic_index), &out_export->field);
  return AGS_STATUS_OK;
}
