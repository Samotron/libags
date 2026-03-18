#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libags/tabular.h"
#include "dictionary_internal.h"
#include "document_internal.h"
#include "tabular_internal.h"

typedef enum ags_numeric_kind {
  AGS_NUMERIC_NONE = 0,
  AGS_NUMERIC_DP = 1,
  AGS_NUMERIC_SCI = 2,
  AGS_NUMERIC_SF = 3
} ags_numeric_kind;

typedef struct ags_sort_entry {
  size_t source_index;
  size_t dictionary_order;
  int dictionary_known;
  int hierarchy_depth;
} ags_sort_entry;

typedef struct ags_suffix_pair {
  const char *easting_suffix;
  const char *northing_suffix;
} ags_suffix_pair;

static ags_status pick_allocator_from_table_options(
  const ags_table_options *options,
  ags_allocator *out_allocator
) {
  const ags_allocator *allocator = NULL;

  if (out_allocator == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  allocator = ags_default_allocator();
  if (options != NULL) {
    if (options->struct_size != sizeof(*options)) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }

    if (options->allocator != NULL) {
      if (!ags_allocator_is_valid(options->allocator)) {
        return AGS_STATUS_INVALID_ARGUMENT;
      }
      allocator = options->allocator;
    }
  }

  memcpy(out_allocator, allocator, sizeof(*out_allocator));
  return AGS_STATUS_OK;
}

static ags_status pick_allocator_from_sort_options(
  const ags_sort_options *options,
  ags_allocator *out_allocator
) {
  const ags_allocator *allocator = NULL;

  if (out_allocator == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  allocator = ags_default_allocator();
  if (options != NULL) {
    if (options->struct_size != sizeof(*options)) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }

    if (options->allocator != NULL) {
      if (!ags_allocator_is_valid(options->allocator)) {
        return AGS_STATUS_INVALID_ARGUMENT;
      }
      allocator = options->allocator;
    }
  }

  memcpy(out_allocator, allocator, sizeof(*out_allocator));
  return AGS_STATUS_OK;
}

static ags_status pick_allocator_from_geometry_options(
  const ags_geometry_options *options,
  ags_allocator *out_allocator
) {
  const ags_allocator *allocator = NULL;

  if (out_allocator == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  allocator = ags_default_allocator();
  if (options != NULL) {
    if (options->struct_size != sizeof(*options)) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }

    if (options->allocator != NULL) {
      if (!ags_allocator_is_valid(options->allocator)) {
        return AGS_STATUS_INVALID_ARGUMENT;
      }
      allocator = options->allocator;
    }
  }

  memcpy(out_allocator, allocator, sizeof(*out_allocator));
  return AGS_STATUS_OK;
}

ags_status ags_table_options_init(ags_table_options *options) {
  if (options == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  options->struct_size = sizeof(*options);
  options->allocator = NULL;
  options->duplicate_heading_policy = AGS_DUPLICATE_HEADING_REJECT;
  options->duplicate_heading_resolver = NULL;
  options->duplicate_heading_user_data = NULL;
  return AGS_STATUS_OK;
}

ags_status ags_sort_options_init(ags_sort_options *options) {
  if (options == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  options->struct_size = sizeof(*options);
  options->allocator = NULL;
  options->strategy = AGS_SORT_INPUT;
  options->dictionary_version = NULL;
  options->dictionary_document = NULL;
  return AGS_STATUS_OK;
}

ags_status ags_geometry_options_init(ags_geometry_options *options) {
  if (options == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  options->struct_size = sizeof(*options);
  options->allocator = NULL;
  options->geometry_column_name = NULL;
  options->encoding = AGS_GEOMETRY_WKT;
  options->easting_column_name = NULL;
  options->northing_column_name = NULL;
  options->srid = 0;
  options->crs = NULL;
  options->invalid_coordinate_policy = AGS_INVALID_COORDINATES_FAIL;
  return AGS_STATUS_OK;
}

static char *copy_string_or_empty(const ags_allocator *allocator, const char *value) {
  const char *actual = value == NULL ? "" : value;
  return ags_strndup_alloc(allocator, actual, strlen(actual));
}

static char *copy_optional_string(const ags_allocator *allocator, const char *value) {
  if (value == NULL) {
    return NULL;
  }

  return ags_strndup_alloc(allocator, value, strlen(value));
}

static int table_has_column_name(
  const ags_table *table,
  size_t resolved_count,
  const char *column_name
) {
  size_t index = 0;

  for (index = 0; index < resolved_count; ++index) {
    if (strcmp(table->columns[index].name, column_name) == 0) {
      return 1;
    }
  }

  return 0;
}

static size_t duplicate_heading_index(
  size_t current_index,
  const char *const *column_names
) {
  size_t index = 0;
  size_t duplicate_index = 1;

  for (index = 0; index < current_index; ++index) {
    if (strcmp(column_names[index], column_names[current_index]) == 0) {
      duplicate_index += 1;
    }
  }

  return duplicate_index;
}

static ags_status build_renamed_heading(
  const ags_allocator *allocator,
  const char *original_name,
  size_t duplicate_index,
  char **out_heading_name
) {
  char buffer[128];
  int written = 0;
  char *copy = NULL;

  if (allocator == NULL || original_name == NULL || out_heading_name == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  written = snprintf(buffer, sizeof(buffer), "%s_%zu", original_name, duplicate_index);
  if (written < 0 || (size_t)written >= sizeof(buffer)) {
    return AGS_STATUS_INTERNAL_ERROR;
  }

  copy = ags_strndup_alloc(allocator, buffer, (size_t)written);
  if (copy == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  *out_heading_name = copy;
  return AGS_STATUS_OK;
}

static ags_status resolve_duplicate_heading(
  ags_table *table,
  const ags_table_options *options,
  const char *group_name,
  const char *original_name,
  size_t duplicate_index,
  size_t resolved_count,
  char **out_heading_name
) {
  ags_duplicate_heading_policy policy = AGS_DUPLICATE_HEADING_REJECT;
  size_t suffix_index = duplicate_index;
  ags_status status = AGS_STATUS_OK;
  char *candidate = NULL;

  if (table == NULL || original_name == NULL || out_heading_name == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (options != NULL) {
    policy = options->duplicate_heading_policy;
  }

  if (policy == AGS_DUPLICATE_HEADING_REJECT) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (policy == AGS_DUPLICATE_HEADING_CALLBACK) {
    if (options == NULL || options->duplicate_heading_resolver == NULL) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }

    status = options->duplicate_heading_resolver(
      options->duplicate_heading_user_data,
      group_name,
      original_name,
      duplicate_index,
      &table->allocator,
      out_heading_name
    );
    if (status != AGS_STATUS_OK) {
      return status;
    }
    if (*out_heading_name == NULL || (*out_heading_name)[0] == '\0') {
      ags_dealloc(&table->allocator, *out_heading_name);
      *out_heading_name = NULL;
      return AGS_STATUS_INVALID_ARGUMENT;
    }
    if (table_has_column_name(table, resolved_count, *out_heading_name)) {
      ags_dealloc(&table->allocator, *out_heading_name);
      *out_heading_name = NULL;
      return AGS_STATUS_INVALID_ARGUMENT;
    }
    return AGS_STATUS_OK;
  }

  do {
    ags_dealloc(&table->allocator, candidate);
    candidate = NULL;
    status = build_renamed_heading(&table->allocator, original_name, suffix_index, &candidate);
    if (status != AGS_STATUS_OK) {
      return status;
    }
    suffix_index += 1;
  } while (table_has_column_name(table, resolved_count, candidate));

  *out_heading_name = candidate;
  return AGS_STATUS_OK;
}

static void ags_table_clear(ags_table *table) {
  size_t column_index = 0;
  size_t row_index = 0;

  if (table == NULL) {
    return;
  }

  ags_dealloc(&table->allocator, table->group_name);

  for (column_index = 0; column_index < table->column_count; ++column_index) {
    ags_dealloc(&table->allocator, table->columns[column_index].name);
    ags_dealloc(&table->allocator, table->columns[column_index].unit);
    ags_dealloc(&table->allocator, table->columns[column_index].type);

    for (row_index = 0; row_index < table->row_count; ++row_index) {
      ags_dealloc(&table->allocator, table->columns[column_index].values[row_index]);
    }
    ags_dealloc(&table->allocator, table->columns[column_index].values);
  }

  ags_dealloc(&table->allocator, table->columns);
  memset(table, 0, sizeof(*table));
}

void ags_table_destroy(ags_table *table) {
  ags_allocator allocator;

  if (table == NULL) {
    return;
  }

  memcpy(&allocator, &table->allocator, sizeof(allocator));
  ags_table_clear(table);
  ags_dealloc(&allocator, table);
}

static ags_status ags_table_reserve_rows(ags_table *table, size_t required) {
  size_t new_capacity = 0;
  size_t column_index = 0;

  if (table == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (required <= table->row_capacity) {
    return AGS_STATUS_OK;
  }

  new_capacity = table->row_capacity == 0 ? 4 : table->row_capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  for (column_index = 0; column_index < table->column_count; ++column_index) {
    char **values = ags_realloc_buffer(
      &table->allocator,
      table->columns[column_index].values,
      new_capacity * sizeof(*values)
    );
    if (values == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }

    memset(
      values + table->row_capacity,
      0,
      (new_capacity - table->row_capacity) * sizeof(*values)
    );
    table->columns[column_index].values = values;
  }

  table->row_capacity = new_capacity;
  return AGS_STATUS_OK;
}

ags_status ags_table_create(
  const char *group_name,
  size_t column_count,
  const char *const *column_names,
  const char *const *units,
  const char *const *types,
  const ags_table_options *options,
  ags_table **out_table
) {
  ags_allocator allocator;
  ags_table *table = NULL;
  ags_status status = AGS_STATUS_OK;
  size_t column_index = 0;

  if (group_name == NULL || out_table == NULL || (column_count > 0 && column_names == NULL)) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_table = NULL;

  status = pick_allocator_from_table_options(options, &allocator);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  table = ags_alloc(&allocator, sizeof(*table));
  if (table == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(table, 0, sizeof(*table));
  memcpy(&table->allocator, &allocator, sizeof(table->allocator));
  table->group_name = copy_string_or_empty(&table->allocator, group_name);
  if (table->group_name == NULL) {
    ags_table_destroy(table);
    return AGS_STATUS_NO_MEMORY;
  }

  if (column_count > 0) {
    table->columns = ags_alloc(&table->allocator, column_count * sizeof(*table->columns));
    if (table->columns == NULL) {
      ags_table_destroy(table);
      return AGS_STATUS_NO_MEMORY;
    }
    memset(table->columns, 0, column_count * sizeof(*table->columns));
  }
  table->column_count = column_count;

  for (column_index = 0; column_index < column_count; ++column_index) {
    const char *column_name = column_names[column_index];
    char *resolved_name = NULL;

    if (column_name == NULL || column_name[0] == '\0') {
      ags_table_destroy(table);
      return AGS_STATUS_INVALID_ARGUMENT;
    }

    if (table_has_column_name(table, column_index, column_name)) {
      status = resolve_duplicate_heading(
        table,
        options,
        group_name,
        column_name,
        duplicate_heading_index(column_index, column_names),
        column_index,
        &resolved_name
      );
      if (status != AGS_STATUS_OK) {
        ags_table_destroy(table);
        return status;
      }
    } else {
      resolved_name = copy_string_or_empty(&table->allocator, column_name);
      if (resolved_name == NULL) {
        ags_table_destroy(table);
        return AGS_STATUS_NO_MEMORY;
      }
    }

    table->columns[column_index].name = resolved_name;
    table->columns[column_index].unit = copy_string_or_empty(
      &table->allocator,
      units == NULL ? NULL : units[column_index]
    );
    table->columns[column_index].type = copy_string_or_empty(
      &table->allocator,
      types == NULL ? NULL : types[column_index]
    );
    if (table->columns[column_index].unit == NULL ||
        table->columns[column_index].type == NULL) {
      ags_table_destroy(table);
      return AGS_STATUS_NO_MEMORY;
    }
  }

  *out_table = table;
  return AGS_STATUS_OK;
}

ags_status ags_table_append_row(
  ags_table *table,
  const char *const *values,
  size_t value_count
) {
  size_t column_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (table == NULL || value_count != table->column_count || (value_count > 0 && values == NULL)) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = ags_table_reserve_rows(table, table->row_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (column_index = 0; column_index < table->column_count; ++column_index) {
    table->columns[column_index].values[table->row_count] = copy_string_or_empty(
      &table->allocator,
      values[column_index]
    );
    if (table->columns[column_index].values[table->row_count] == NULL) {
      size_t cleanup_index = 0;
      for (cleanup_index = 0; cleanup_index <= column_index; ++cleanup_index) {
        ags_dealloc(&table->allocator, table->columns[cleanup_index].values[table->row_count]);
        table->columns[cleanup_index].values[table->row_count] = NULL;
      }
      return AGS_STATUS_NO_MEMORY;
    }
  }

  table->row_count += 1;
  return AGS_STATUS_OK;
}

const ags_allocator *ags_table_allocator(const ags_table *table) {
  if (table == NULL) {
    return NULL;
  }

  return &table->allocator;
}

const char *ags_table_group_name(const ags_table *table) {
  if (table == NULL) {
    return NULL;
  }

  return table->group_name;
}

size_t ags_table_column_count(const ags_table *table) {
  if (table == NULL) {
    return 0;
  }

  return table->column_count;
}

size_t ags_table_row_count(const ags_table *table) {
  if (table == NULL) {
    return 0;
  }

  return table->row_count;
}

const char *ags_table_column_name(const ags_table *table, size_t column_index) {
  if (table == NULL || column_index >= table->column_count) {
    return NULL;
  }

  return table->columns[column_index].name;
}

const char *ags_table_column_unit(const ags_table *table, size_t column_index) {
  if (table == NULL || column_index >= table->column_count) {
    return NULL;
  }

  return table->columns[column_index].unit;
}

const char *ags_table_column_type(const ags_table *table, size_t column_index) {
  if (table == NULL || column_index >= table->column_count) {
    return NULL;
  }

  return table->columns[column_index].type;
}

const char *ags_table_cell_value(
  const ags_table *table,
  size_t row_index,
  size_t column_index
) {
  if (table == NULL || row_index >= table->row_count || column_index >= table->column_count) {
    return NULL;
  }

  return table->columns[column_index].values[row_index];
}

const char *const *ags_table_column_values(
  const ags_table *table,
  size_t column_index
) {
  if (table == NULL || column_index >= table->column_count) {
    return NULL;
  }

  return (const char *const *)table->columns[column_index].values;
}

ags_status ags_table_from_group(
  const ags_document *document,
  size_t group_index,
  const ags_table_options *options,
  ags_table **out_table
) {
  const ags_group_internal *group = NULL;
  const char **names = NULL;
  const char **units = NULL;
  const char **types = NULL;
  ags_table *table = NULL;
  ags_status status = AGS_STATUS_OK;
  size_t field_index = 0;
  size_t row_index = 0;

  if (document == NULL || out_table == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_table = NULL;
  group = ags_document_get_group(document, group_index);
  if (group == NULL) {
    return AGS_STATUS_NOT_FOUND;
  }

  if (group->field_count > 0) {
    names = ags_alloc(&document->allocator, group->field_count * sizeof(*names));
    units = ags_alloc(&document->allocator, group->field_count * sizeof(*units));
    types = ags_alloc(&document->allocator, group->field_count * sizeof(*types));
    if (names == NULL || units == NULL || types == NULL) {
      ags_dealloc(&document->allocator, names);
      ags_dealloc(&document->allocator, units);
      ags_dealloc(&document->allocator, types);
      return AGS_STATUS_NO_MEMORY;
    }
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    names[field_index] = group->fields[field_index].name;
    units[field_index] = group->fields[field_index].unit;
    types[field_index] = group->fields[field_index].type;
  }

  status = ags_table_create(
    group->name,
    group->field_count,
    names,
    units,
    types,
    options,
    &table
  );
  ags_dealloc(&document->allocator, names);
  ags_dealloc(&document->allocator, units);
  ags_dealloc(&document->allocator, types);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (row_index = 0; row_index < group->row_count; ++row_index) {
    status = ags_table_append_row(table, (const char *const *)group->rows[row_index].values, group->field_count);
    if (status != AGS_STATUS_OK) {
      ags_table_destroy(table);
      return status;
    }
  }

  *out_table = table;
  return AGS_STATUS_OK;
}

static ags_status document_append_group_from_table(
  ags_document *document,
  const ags_table *table
) {
  ags_group_internal *group = NULL;
  size_t field_index = 0;
  size_t row_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (document == NULL || table == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (ags_document_find_group_index(document, table->group_name) != (size_t)-1) {
    return AGS_STATUS_PARSE_ERROR;
  }

  status = ags_document_reserve_groups(document, document->group_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  group = &document->groups[document->group_count];
  memset(group, 0, sizeof(*group));
  group->name = copy_string_or_empty(&document->allocator, table->group_name);
  if (group->name == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  group->field_count = table->column_count;
  if (group->field_count > 0) {
    group->fields = ags_alloc(&document->allocator, group->field_count * sizeof(*group->fields));
    if (group->fields == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    memset(group->fields, 0, group->field_count * sizeof(*group->fields));
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    group->fields[field_index].name = copy_string_or_empty(&document->allocator, table->columns[field_index].name);
    group->fields[field_index].unit = copy_string_or_empty(&document->allocator, table->columns[field_index].unit);
    group->fields[field_index].type = copy_string_or_empty(&document->allocator, table->columns[field_index].type);
    if (group->fields[field_index].name == NULL ||
        group->fields[field_index].unit == NULL ||
        group->fields[field_index].type == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  group->row_count = table->row_count;
  group->row_capacity = table->row_count;
  if (group->row_count > 0) {
    group->rows = ags_alloc(&document->allocator, group->row_count * sizeof(*group->rows));
    if (group->rows == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    memset(group->rows, 0, group->row_count * sizeof(*group->rows));
  }

  for (row_index = 0; row_index < group->row_count; ++row_index) {
    group->rows[row_index].values = ags_alloc(
      &document->allocator,
      group->field_count * sizeof(*group->rows[row_index].values)
    );
    if (group->rows[row_index].values == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    memset(group->rows[row_index].values, 0, group->field_count * sizeof(*group->rows[row_index].values));

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      group->rows[row_index].values[field_index] = copy_string_or_empty(
        &document->allocator,
        table->columns[field_index].values[row_index]
      );
      if (group->rows[row_index].values[field_index] == NULL) {
        return AGS_STATUS_NO_MEMORY;
      }
    }
  }

  document->group_count += 1;
  return AGS_STATUS_OK;
}

ags_status ags_document_from_tables(
  const ags_table *const *tables,
  size_t table_count,
  const ags_document_options *options,
  ags_document **out_document
) {
  ags_document *document = NULL;
  size_t table_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (out_document == NULL || (table_count > 0 && tables == NULL)) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_document = NULL;
  status = ags_document_create(options, &document);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (table_index = 0; table_index < table_count; ++table_index) {
    status = document_append_group_from_table(document, tables[table_index]);
    if (status != AGS_STATUS_OK) {
      ags_document_destroy(document);
      return status;
    }
  }

  *out_document = document;
  return AGS_STATUS_OK;
}

static int parse_numeric_kind(const char *type, ags_numeric_kind *out_kind, int *out_precision) {
  char *end = NULL;
  unsigned long precision = 0;

  if (out_kind == NULL || out_precision == NULL) {
    return 0;
  }

  *out_kind = AGS_NUMERIC_NONE;
  *out_precision = 0;

  if (type == NULL || type[0] == '\0') {
    return 0;
  }

  if (strlen(type) > 2 && strcmp(type + strlen(type) - 2, "DP") == 0) {
    precision = strtoul(type, &end, 10);
    if (end != type && strcmp(end, "DP") == 0) {
      *out_kind = AGS_NUMERIC_DP;
      *out_precision = (int)precision;
      return 1;
    }
  }

  if (strlen(type) > 3 && strcmp(type + strlen(type) - 3, "SCI") == 0) {
    precision = strtoul(type, &end, 10);
    if (end != type && strcmp(end, "SCI") == 0) {
      *out_kind = AGS_NUMERIC_SCI;
      *out_precision = (int)precision;
      return 1;
    }
  }

  if (strlen(type) > 2 && strcmp(type + strlen(type) - 2, "SF") == 0) {
    precision = strtoul(type, &end, 10);
    if (end != type && strcmp(end, "SF") == 0) {
      *out_kind = AGS_NUMERIC_SF;
      *out_precision = (int)precision;
      return 1;
    }
  }

  return 0;
}

static int parse_double_exact(const char *text, double *out_value) {
  char *end = NULL;
  double value = 0.0;

  if (text == NULL || out_value == NULL || text[0] == '\0') {
    return 0;
  }

  value = strtod(text, &end);
  if (end == text || *end != '\0') {
    return 0;
  }

  *out_value = value;
  return 1;
}

ags_status ags_table_column_to_numeric(
  const ags_table *table,
  size_t column_index,
  ags_numeric_column **out_numeric
) {
  ags_numeric_column *numeric = NULL;
  ags_numeric_kind kind = AGS_NUMERIC_NONE;
  int precision = 0;
  size_t row_index = 0;

  if (table == NULL || out_numeric == NULL || column_index >= table->column_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_numeric = NULL;

  if (!parse_numeric_kind(table->columns[column_index].type, &kind, &precision)) {
    (void)kind;
    (void)precision;
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  numeric = ags_alloc(&table->allocator, sizeof(*numeric));
  if (numeric == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }
  memset(numeric, 0, sizeof(*numeric));
  memcpy(&numeric->allocator, &table->allocator, sizeof(numeric->allocator));
  numeric->count = table->row_count;

  if (numeric->count > 0) {
    numeric->values = ags_alloc(&numeric->allocator, numeric->count * sizeof(*numeric->values));
    numeric->is_null = ags_alloc(&numeric->allocator, numeric->count * sizeof(*numeric->is_null));
    if (numeric->values == NULL || numeric->is_null == NULL) {
      ags_numeric_column_destroy(numeric);
      return AGS_STATUS_NO_MEMORY;
    }
    memset(numeric->is_null, 0, numeric->count * sizeof(*numeric->is_null));
  }

  for (row_index = 0; row_index < numeric->count; ++row_index) {
    const char *value = table->columns[column_index].values[row_index];

    if (value == NULL || value[0] == '\0') {
      numeric->is_null[row_index] = 1;
      numeric->values[row_index] = 0.0;
      continue;
    }

    if (!parse_double_exact(value, &numeric->values[row_index])) {
      ags_numeric_column_destroy(numeric);
      return AGS_STATUS_PARSE_ERROR;
    }
  }

  *out_numeric = numeric;
  return AGS_STATUS_OK;
}

static ags_status format_numeric_value(
  const ags_allocator *allocator,
  ags_numeric_kind kind,
  int precision,
  double value,
  char **out_text
) {
  char buffer[128];
  int written = 0;

  if (allocator == NULL || out_text == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (precision < 0 || precision > 64) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (kind == AGS_NUMERIC_DP) {
    written = snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
  } else if (kind == AGS_NUMERIC_SCI) {
    written = snprintf(buffer, sizeof(buffer), "%.*E", precision, value);
  } else if (kind == AGS_NUMERIC_SF) {
    written = snprintf(buffer, sizeof(buffer), "%.*G", precision, value);
  } else {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (written < 0 || (size_t)written >= sizeof(buffer)) {
    return AGS_STATUS_INTERNAL_ERROR;
  }

  *out_text = ags_strndup_alloc(allocator, buffer, (size_t)written);
  if (*out_text == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  return AGS_STATUS_OK;
}

ags_status ags_table_column_from_numeric(
  ags_table *table,
  size_t column_index,
  const ags_numeric_column *numeric
) {
  ags_numeric_kind kind = AGS_NUMERIC_NONE;
  int precision = 0;
  size_t row_index = 0;

  if (table == NULL || numeric == NULL || column_index >= table->column_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (numeric->count != table->row_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (!parse_numeric_kind(table->columns[column_index].type, &kind, &precision)) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  for (row_index = 0; row_index < table->row_count; ++row_index) {
    char *replacement = NULL;
    ags_status status = AGS_STATUS_OK;

    if (numeric->is_null[row_index] != 0) {
      replacement = copy_string_or_empty(&table->allocator, "");
      if (replacement == NULL) {
        return AGS_STATUS_NO_MEMORY;
      }
    } else {
      status = format_numeric_value(
        &table->allocator,
        kind,
        precision,
        numeric->values[row_index],
        &replacement
      );
      if (status != AGS_STATUS_OK) {
        return status;
      }
    }

    ags_dealloc(&table->allocator, table->columns[column_index].values[row_index]);
    table->columns[column_index].values[row_index] = replacement;
  }

  return AGS_STATUS_OK;
}

void ags_numeric_column_destroy(ags_numeric_column *numeric) {
  if (numeric == NULL) {
    return;
  }

  ags_dealloc(&numeric->allocator, numeric->values);
  ags_dealloc(&numeric->allocator, numeric->is_null);
  ags_dealloc(&numeric->allocator, numeric);
}

size_t ags_numeric_column_count(const ags_numeric_column *numeric) {
  if (numeric == NULL) {
    return 0;
  }

  return numeric->count;
}

int ags_numeric_column_is_null(const ags_numeric_column *numeric, size_t row_index) {
  if (numeric == NULL || row_index >= numeric->count) {
    return 1;
  }

  return numeric->is_null[row_index] != 0;
}

double ags_numeric_column_value(const ags_numeric_column *numeric, size_t row_index) {
  if (numeric == NULL || row_index >= numeric->count) {
    return 0.0;
  }

  return numeric->values[row_index];
}

ags_status ags_numeric_column_set_value(
  ags_numeric_column *numeric,
  size_t row_index,
  double value
) {
  if (numeric == NULL || row_index >= numeric->count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  numeric->values[row_index] = value;
  numeric->is_null[row_index] = 0;
  return AGS_STATUS_OK;
}

ags_status ags_numeric_column_set_null(
  ags_numeric_column *numeric,
  size_t row_index,
  int is_null
) {
  if (numeric == NULL || row_index >= numeric->count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  numeric->is_null[row_index] = is_null != 0 ? 1 : 0;
  if (numeric->is_null[row_index] != 0) {
    numeric->values[row_index] = 0.0;
  }
  return AGS_STATUS_OK;
}

static ags_status document_append_group_from_source(
  ags_document *document,
  const ags_group_internal *source_group
) {
  ags_group_internal *group = NULL;
  size_t field_index = 0;
  size_t row_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (document == NULL || source_group == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = ags_document_reserve_groups(document, document->group_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  group = &document->groups[document->group_count];
  memset(group, 0, sizeof(*group));
  group->name = copy_string_or_empty(&document->allocator, source_group->name);
  if (group->name == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  group->group_line_number = source_group->group_line_number;
  group->heading_line_number = source_group->heading_line_number;
  group->unit_line_number = source_group->unit_line_number;
  group->type_line_number = source_group->type_line_number;
  group->field_count = source_group->field_count;

  if (group->field_count > 0) {
    group->fields = ags_alloc(&document->allocator, group->field_count * sizeof(*group->fields));
    if (group->fields == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    memset(group->fields, 0, group->field_count * sizeof(*group->fields));
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    group->fields[field_index].name = copy_string_or_empty(&document->allocator, source_group->fields[field_index].name);
    group->fields[field_index].unit = copy_string_or_empty(&document->allocator, source_group->fields[field_index].unit);
    group->fields[field_index].type = copy_string_or_empty(&document->allocator, source_group->fields[field_index].type);
    if (group->fields[field_index].name == NULL ||
        group->fields[field_index].unit == NULL ||
        group->fields[field_index].type == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  group->row_count = source_group->row_count;
  group->row_capacity = source_group->row_count;
  if (group->row_count > 0) {
    group->rows = ags_alloc(&document->allocator, group->row_count * sizeof(*group->rows));
    if (group->rows == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    memset(group->rows, 0, group->row_count * sizeof(*group->rows));
  }

  for (row_index = 0; row_index < group->row_count; ++row_index) {
    group->rows[row_index].line_number = source_group->rows[row_index].line_number;
    group->rows[row_index].values = ags_alloc(
      &document->allocator,
      group->field_count * sizeof(*group->rows[row_index].values)
    );
    if (group->rows[row_index].values == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    memset(group->rows[row_index].values, 0, group->field_count * sizeof(*group->rows[row_index].values));

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      group->rows[row_index].values[field_index] = copy_string_or_empty(
        &document->allocator,
        source_group->rows[row_index].values[field_index]
      );
      if (group->rows[row_index].values[field_index] == NULL) {
        return AGS_STATUS_NO_MEMORY;
      }
    }
  }

  document->group_count += 1;
  return AGS_STATUS_OK;
}

static ags_status clone_document_with_order(
  const ags_document *source_document,
  const size_t *ordered_indices,
  size_t ordered_count,
  const ags_allocator *allocator,
  ags_document **out_document
) {
  ags_document_options options;
  ags_document *document = NULL;
  size_t ordered_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (source_document == NULL ||
      allocator == NULL ||
      out_document == NULL ||
      (ordered_count > 0 && ordered_indices == NULL)) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = ags_document_options_init(&options);
  if (status != AGS_STATUS_OK) {
    return status;
  }
  options.allocator = allocator;

  status = ags_document_create(&options, &document);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (ordered_index = 0; ordered_index < ordered_count; ++ordered_index) {
    const ags_group_internal *group = ags_document_get_group(source_document, ordered_indices[ordered_index]);
    status = document_append_group_from_source(document, group);
    if (status != AGS_STATUS_OK) {
      ags_document_destroy(document);
      return status;
    }
  }

  *out_document = document;
  return AGS_STATUS_OK;
}

static int compare_alphabetical(
  const ags_document *document,
  size_t left_index,
  size_t right_index
) {
  const ags_group_internal *left = ags_document_get_group(document, left_index);
  const ags_group_internal *right = ags_document_get_group(document, right_index);
  int comparison = strcmp(left->name, right->name);

  if (comparison != 0) {
    return comparison;
  }

  if (left_index < right_index) {
    return -1;
  }

  if (left_index > right_index) {
    return 1;
  }

  return 0;
}

static int group_depth_recursive(
  const ags_effective_dictionary *dictionary,
  const char *group_name,
  const char *const *stack,
  size_t stack_count
) {
  const ags_dict_entry_view *entry = NULL;
  const ags_dict_entry_view *parent_entry = NULL;
  size_t stack_index = 0;
  const char *next_stack[32];

  if (dictionary == NULL || group_name == NULL || stack_count >= (sizeof(next_stack) / sizeof(next_stack[0]))) {
    return 0;
  }

  for (stack_index = 0; stack_index < stack_count; ++stack_index) {
    if (strcmp(stack[stack_index], group_name) == 0) {
      return 0;
    }
  }

  entry = ags_effective_dictionary_find_group(dictionary, group_name);
  if (entry == NULL || entry->pgrp == NULL || entry->pgrp[0] == '\0' || strcmp(entry->pgrp, "-") == 0) {
    return 0;
  }

  parent_entry = ags_effective_dictionary_find_group(dictionary, entry->pgrp);
  if (parent_entry == NULL) {
    return 1;
  }

  for (stack_index = 0; stack_index < stack_count; ++stack_index) {
    next_stack[stack_index] = stack[stack_index];
  }
  next_stack[stack_count] = group_name;

  return 1 + group_depth_recursive(dictionary, entry->pgrp, next_stack, stack_count + 1);
}

static void insertion_sort_alphabetical(
  const ags_document *document,
  size_t *indices,
  size_t count
) {
  size_t index = 0;

  for (index = 1; index < count; ++index) {
    size_t cursor = index;
    size_t value = indices[index];

    while (cursor > 0 &&
           compare_alphabetical(document, value, indices[cursor - 1]) < 0) {
      indices[cursor] = indices[cursor - 1];
      cursor -= 1;
    }
    indices[cursor] = value;
  }
}

static void insertion_sort_dictionary(
  ags_sort_entry *entries,
  size_t count
) {
  size_t index = 0;

  for (index = 1; index < count; ++index) {
    ags_sort_entry value = entries[index];
    size_t cursor = index;

    while (cursor > 0) {
      ags_sort_entry previous = entries[cursor - 1];
      int should_move = 0;

      if (value.dictionary_known != previous.dictionary_known) {
        should_move = value.dictionary_known > previous.dictionary_known;
      } else if (value.dictionary_known != 0 &&
                 value.dictionary_order != previous.dictionary_order) {
        should_move = value.dictionary_order < previous.dictionary_order;
      } else {
        should_move = value.source_index < previous.source_index;
      }

      if (!should_move) {
        break;
      }

      entries[cursor] = entries[cursor - 1];
      cursor -= 1;
    }

    entries[cursor] = value;
  }
}

static void insertion_sort_hierarchical(
  ags_sort_entry *entries,
  size_t count
) {
  size_t index = 0;

  for (index = 1; index < count; ++index) {
    ags_sort_entry value = entries[index];
    size_t cursor = index;

    while (cursor > 0) {
      ags_sort_entry previous = entries[cursor - 1];
      int should_move = 0;

      if (value.hierarchy_depth != previous.hierarchy_depth) {
        should_move = value.hierarchy_depth < previous.hierarchy_depth;
      } else if (value.dictionary_known != previous.dictionary_known) {
        should_move = value.dictionary_known > previous.dictionary_known;
      } else if (value.dictionary_known != 0 &&
                 value.dictionary_order != previous.dictionary_order) {
        should_move = value.dictionary_order < previous.dictionary_order;
      } else {
        should_move = value.source_index < previous.source_index;
      }

      if (!should_move) {
        break;
      }

      entries[cursor] = entries[cursor - 1];
      cursor -= 1;
    }

    entries[cursor] = value;
  }
}

ags_status ags_document_sort_groups(
  const ags_document *document,
  const ags_sort_options *options,
  ags_document **out_document
) {
  ags_allocator allocator;
  ags_status status = AGS_STATUS_OK;
  size_t *ordered_indices = NULL;
  size_t group_index = 0;
  ags_document *loaded_dictionary = NULL;
  const ags_document *standard_dictionary = NULL;
  const char *resolved_version = NULL;
  ags_document_options document_options;
  ags_effective_dictionary effective_dictionary;
  int use_dictionary = 0;
  ags_sort_strategy strategy = AGS_SORT_INPUT;

  if (document == NULL || out_document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_document = NULL;

  status = pick_allocator_from_sort_options(options, &allocator);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  if (options != NULL) {
    strategy = options->strategy;
  }

  if (document->group_count > 0) {
    ordered_indices = ags_alloc(&allocator, document->group_count * sizeof(*ordered_indices));
    if (ordered_indices == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    ordered_indices[group_index] = group_index;
  }

  if (strategy == AGS_SORT_ALPHABETICAL) {
    insertion_sort_alphabetical(document, ordered_indices, document->group_count);
  } else if (strategy == AGS_SORT_DICTIONARY || strategy == AGS_SORT_HIERARCHICAL) {
    ags_sort_entry *entries = NULL;

    use_dictionary = 1;
    memset(&effective_dictionary, 0, sizeof(effective_dictionary));

    if (options != NULL && options->dictionary_document != NULL) {
      standard_dictionary = options->dictionary_document;
    } else {
      status = ags_dictionary_resolve_version(
        document,
        options == NULL ? NULL : options->dictionary_version,
        &resolved_version
      );
      if (status != AGS_STATUS_OK) {
        ags_dealloc(&allocator, ordered_indices);
        return status;
      }

      status = ags_document_options_init(&document_options);
      if (status != AGS_STATUS_OK) {
        ags_dealloc(&allocator, ordered_indices);
        return status;
      }
      document_options.allocator = &allocator;

      status = ags_dictionary_load_bundled(resolved_version, &document_options, &loaded_dictionary);
      if (status != AGS_STATUS_OK) {
        ags_dealloc(&allocator, ordered_indices);
        return status;
      }
      standard_dictionary = loaded_dictionary;
    }

    status = ags_effective_dictionary_build(&effective_dictionary, &allocator, standard_dictionary, document);
    if (status != AGS_STATUS_OK) {
      ags_document_destroy(loaded_dictionary);
      ags_dealloc(&allocator, ordered_indices);
      return status;
    }

    if (document->group_count > 0) {
      entries = ags_alloc(&allocator, document->group_count * sizeof(*entries));
      if (entries == NULL) {
        ags_effective_dictionary_destroy(&effective_dictionary);
        ags_document_destroy(loaded_dictionary);
        ags_dealloc(&allocator, ordered_indices);
        return AGS_STATUS_NO_MEMORY;
      }
    }

    for (group_index = 0; group_index < document->group_count; ++group_index) {
      const ags_group_internal *group = ags_document_get_group(document, group_index);
      const ags_dict_entry_view *entry = ags_effective_dictionary_find_group(&effective_dictionary, group->name);

      entries[group_index].source_index = group_index;
      entries[group_index].dictionary_known = entry != NULL;
      entries[group_index].dictionary_order = entry == NULL ? 0 : entry->order_index;
      entries[group_index].hierarchy_depth = group_depth_recursive(&effective_dictionary, group->name, NULL, 0);
    }

    if (strategy == AGS_SORT_DICTIONARY) {
      insertion_sort_dictionary(entries, document->group_count);
    } else {
      insertion_sort_hierarchical(entries, document->group_count);
    }

    for (group_index = 0; group_index < document->group_count; ++group_index) {
      ordered_indices[group_index] = entries[group_index].source_index;
    }

    ags_dealloc(&allocator, entries);
    ags_effective_dictionary_destroy(&effective_dictionary);
    ags_document_destroy(loaded_dictionary);
  }

  status = clone_document_with_order(
    document,
    ordered_indices,
    document->group_count,
    &allocator,
    out_document
  );
  ags_dealloc(&allocator, ordered_indices);
  (void)use_dictionary;
  return status;
}

static int ends_with(const char *value, const char *suffix) {
  size_t value_length = 0;
  size_t suffix_length = 0;

  if (value == NULL || suffix == NULL) {
    return 0;
  }

  value_length = strlen(value);
  suffix_length = strlen(suffix);
  if (value_length < suffix_length) {
    return 0;
  }

  return strcmp(value + value_length - suffix_length, suffix) == 0;
}

static int geometry_suffix_pair_matches(
  const char *easting_name,
  const char *northing_name,
  const char *easting_suffix,
  const char *northing_suffix
) {
  size_t easting_length = 0;
  size_t northing_length = 0;
  size_t easting_suffix_length = 0;
  size_t northing_suffix_length = 0;

  if (!ends_with(easting_name, easting_suffix) || !ends_with(northing_name, northing_suffix)) {
    return 0;
  }

  easting_length = strlen(easting_name);
  northing_length = strlen(northing_name);
  easting_suffix_length = strlen(easting_suffix);
  northing_suffix_length = strlen(northing_suffix);

  if ((easting_length - easting_suffix_length) != (northing_length - northing_suffix_length)) {
    return 0;
  }

  return strncmp(
    easting_name,
    northing_name,
    easting_length - easting_suffix_length
  ) == 0;
}

static ags_status find_geometry_column_indices(
  const ags_table *table,
  const ags_geometry_options *options,
  size_t *out_easting_index,
  size_t *out_northing_index
) {
  static const ags_suffix_pair suffix_pairs[] = {
    {"_EAST", "_NORT"},
    {"_EAST", "_NORTH"},
    {"_EASTING", "_NORTHING"},
    {"_NATE", "_NATN"}
  };
  size_t column_index = 0;
  size_t other_index = 0;
  size_t pair_index = 0;

  if (table == NULL || out_easting_index == NULL || out_northing_index == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (options != NULL &&
      options->easting_column_name != NULL &&
      options->northing_column_name != NULL) {
    *out_easting_index = (size_t)-1;
    *out_northing_index = (size_t)-1;

    for (column_index = 0; column_index < table->column_count; ++column_index) {
      if (strcmp(table->columns[column_index].name, options->easting_column_name) == 0) {
        *out_easting_index = column_index;
      }
      if (strcmp(table->columns[column_index].name, options->northing_column_name) == 0) {
        *out_northing_index = column_index;
      }
    }

    if (*out_easting_index == (size_t)-1 || *out_northing_index == (size_t)-1) {
      return AGS_STATUS_NOT_FOUND;
    }
    return AGS_STATUS_OK;
  }

  for (column_index = 0; column_index < table->column_count; ++column_index) {
    for (other_index = 0; other_index < table->column_count; ++other_index) {
      if (column_index == other_index) {
        continue;
      }

      for (pair_index = 0; pair_index < (sizeof(suffix_pairs) / sizeof(suffix_pairs[0])); ++pair_index) {
        if (geometry_suffix_pair_matches(
              table->columns[column_index].name,
              table->columns[other_index].name,
              suffix_pairs[pair_index].easting_suffix,
              suffix_pairs[pair_index].northing_suffix
            )) {
          *out_easting_index = column_index;
          *out_northing_index = other_index;
          return AGS_STATUS_OK;
        }
      }
    }
  }

  return AGS_STATUS_NOT_FOUND;
}

static void write_uint32_le(unsigned char *buffer, unsigned int value) {
  buffer[0] = (unsigned char)(value & 0xffU);
  buffer[1] = (unsigned char)((value >> 8U) & 0xffU);
  buffer[2] = (unsigned char)((value >> 16U) & 0xffU);
  buffer[3] = (unsigned char)((value >> 24U) & 0xffU);
}

static void write_double_le(unsigned char *buffer, double value) {
  union {
    double numeric;
    unsigned char bytes[sizeof(double)];
  } bits;
  unsigned int probe = 1;
  size_t byte_index = 0;
  int little_endian = *((unsigned char *)&probe) == 1;

  bits.numeric = value;

  for (byte_index = 0; byte_index < sizeof(double); ++byte_index) {
    buffer[byte_index] = bits.bytes[little_endian ? byte_index : (sizeof(double) - 1 - byte_index)];
  }
}

static ags_status build_wkt_point(
  const ags_allocator *allocator,
  double easting,
  double northing,
  char **out_text
) {
  char buffer[128];
  int written = 0;

  if (allocator == NULL || out_text == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  written = snprintf(buffer, sizeof(buffer), "POINT (%.15g %.15g)", easting, northing);
  if (written < 0 || (size_t)written >= sizeof(buffer)) {
    return AGS_STATUS_INTERNAL_ERROR;
  }

  *out_text = ags_strndup_alloc(allocator, buffer, (size_t)written);
  if (*out_text == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  return AGS_STATUS_OK;
}

static ags_status build_wkb_point(
  const ags_allocator *allocator,
  double easting,
  double northing,
  unsigned char **out_bytes,
  size_t *out_length
) {
  unsigned char *buffer = NULL;

  if (allocator == NULL || out_bytes == NULL || out_length == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  buffer = ags_alloc(allocator, 21);
  if (buffer == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  buffer[0] = 1;
  write_uint32_le(buffer + 1, 1U);
  write_double_le(buffer + 5, easting);
  write_double_le(buffer + 13, northing);

  *out_bytes = buffer;
  *out_length = 21;
  return AGS_STATUS_OK;
}

void ags_geometry_column_destroy(ags_geometry_column *geometry) {
  size_t row_index = 0;

  if (geometry == NULL) {
    return;
  }

  ags_dealloc(&geometry->allocator, geometry->name);
  ags_dealloc(&geometry->allocator, geometry->crs);
  ags_dealloc(&geometry->allocator, geometry->is_null);

  if (geometry->wkt_values != NULL) {
    for (row_index = 0; row_index < geometry->row_count; ++row_index) {
      ags_dealloc(&geometry->allocator, geometry->wkt_values[row_index]);
    }
  }

  if (geometry->wkb_values != NULL) {
    for (row_index = 0; row_index < geometry->row_count; ++row_index) {
      ags_dealloc(&geometry->allocator, geometry->wkb_values[row_index]);
    }
  }

  ags_dealloc(&geometry->allocator, geometry->wkt_values);
  ags_dealloc(&geometry->allocator, geometry->wkb_values);
  ags_dealloc(&geometry->allocator, geometry->wkb_lengths);
  ags_dealloc(&geometry->allocator, geometry);
}

ags_status ags_table_derive_geometry(
  const ags_table *table,
  const ags_geometry_options *options,
  ags_geometry_column **out_geometry
) {
  ags_allocator allocator;
  ags_geometry_column *geometry = NULL;
  ags_status status = AGS_STATUS_OK;
  size_t easting_index = 0;
  size_t northing_index = 0;
  size_t row_index = 0;
  const char *geometry_name = NULL;

  if (table == NULL || out_geometry == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_geometry = NULL;

  status = pick_allocator_from_geometry_options(options, &allocator);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = find_geometry_column_indices(table, options, &easting_index, &northing_index);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  geometry = ags_alloc(&allocator, sizeof(*geometry));
  if (geometry == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }
  memset(geometry, 0, sizeof(*geometry));
  memcpy(&geometry->allocator, &allocator, sizeof(geometry->allocator));
  geometry->encoding = options == NULL ? AGS_GEOMETRY_WKT : options->encoding;
  geometry->row_count = table->row_count;
  geometry->srid = options == NULL ? 0 : options->srid;
  geometry_name = (options == NULL || options->geometry_column_name == NULL || options->geometry_column_name[0] == '\0')
    ? "GEOMETRY"
    : options->geometry_column_name;

  geometry->name = copy_string_or_empty(&geometry->allocator, geometry_name);
  geometry->crs = copy_optional_string(&geometry->allocator, options == NULL ? NULL : options->crs);
  if (geometry->name == NULL || ((options != NULL && options->crs != NULL) && geometry->crs == NULL)) {
    ags_geometry_column_destroy(geometry);
    return AGS_STATUS_NO_MEMORY;
  }

  if (geometry->row_count > 0) {
    geometry->is_null = ags_alloc(&geometry->allocator, geometry->row_count * sizeof(*geometry->is_null));
    if (geometry->is_null == NULL) {
      ags_geometry_column_destroy(geometry);
      return AGS_STATUS_NO_MEMORY;
    }
    memset(geometry->is_null, 0, geometry->row_count * sizeof(*geometry->is_null));

    if (geometry->encoding == AGS_GEOMETRY_WKT) {
      geometry->wkt_values = ags_alloc(&geometry->allocator, geometry->row_count * sizeof(*geometry->wkt_values));
      if (geometry->wkt_values == NULL) {
        ags_geometry_column_destroy(geometry);
        return AGS_STATUS_NO_MEMORY;
      }
      memset(geometry->wkt_values, 0, geometry->row_count * sizeof(*geometry->wkt_values));
    } else {
      geometry->wkb_values = ags_alloc(&geometry->allocator, geometry->row_count * sizeof(*geometry->wkb_values));
      geometry->wkb_lengths = ags_alloc(&geometry->allocator, geometry->row_count * sizeof(*geometry->wkb_lengths));
      if (geometry->wkb_values == NULL || geometry->wkb_lengths == NULL) {
        ags_geometry_column_destroy(geometry);
        return AGS_STATUS_NO_MEMORY;
      }
      memset(geometry->wkb_values, 0, geometry->row_count * sizeof(*geometry->wkb_values));
      memset(geometry->wkb_lengths, 0, geometry->row_count * sizeof(*geometry->wkb_lengths));
    }
  }

  for (row_index = 0; row_index < geometry->row_count; ++row_index) {
    const char *easting_text = table->columns[easting_index].values[row_index];
    const char *northing_text = table->columns[northing_index].values[row_index];
    double easting = 0.0;
    double northing = 0.0;
    int have_easting = easting_text != NULL && easting_text[0] != '\0';
    int have_northing = northing_text != NULL && northing_text[0] != '\0';
    int valid = 0;

    if (!have_easting && !have_northing) {
      geometry->is_null[row_index] = 1;
      continue;
    }

    valid = have_easting &&
            have_northing &&
            parse_double_exact(easting_text, &easting) &&
            parse_double_exact(northing_text, &northing);

    if (!valid) {
      if (options == NULL || options->invalid_coordinate_policy == AGS_INVALID_COORDINATES_FAIL) {
        ags_geometry_column_destroy(geometry);
        return AGS_STATUS_PARSE_ERROR;
      }

      geometry->invalid_row_count += 1;
      geometry->is_null[row_index] = 1;
      continue;
    }

    if (geometry->encoding == AGS_GEOMETRY_WKT) {
      status = build_wkt_point(&geometry->allocator, easting, northing, &geometry->wkt_values[row_index]);
    } else {
      status = build_wkb_point(
        &geometry->allocator,
        easting,
        northing,
        &geometry->wkb_values[row_index],
        &geometry->wkb_lengths[row_index]
      );
    }
    if (status != AGS_STATUS_OK) {
      ags_geometry_column_destroy(geometry);
      return status;
    }
  }

  *out_geometry = geometry;
  return AGS_STATUS_OK;
}

const char *ags_geometry_column_name(const ags_geometry_column *geometry) {
  if (geometry == NULL) {
    return NULL;
  }

  return geometry->name;
}

ags_geometry_encoding ags_geometry_column_encoding(const ags_geometry_column *geometry) {
  if (geometry == NULL) {
    return AGS_GEOMETRY_WKT;
  }

  return geometry->encoding;
}

size_t ags_geometry_column_row_count(const ags_geometry_column *geometry) {
  if (geometry == NULL) {
    return 0;
  }

  return geometry->row_count;
}

size_t ags_geometry_column_invalid_row_count(const ags_geometry_column *geometry) {
  if (geometry == NULL) {
    return 0;
  }

  return geometry->invalid_row_count;
}

int ags_geometry_column_srid(const ags_geometry_column *geometry) {
  if (geometry == NULL) {
    return 0;
  }

  return geometry->srid;
}

const char *ags_geometry_column_crs(const ags_geometry_column *geometry) {
  if (geometry == NULL) {
    return NULL;
  }

  return geometry->crs;
}

int ags_geometry_column_is_null(const ags_geometry_column *geometry, size_t row_index) {
  if (geometry == NULL || row_index >= geometry->row_count) {
    return 1;
  }

  return geometry->is_null[row_index] != 0;
}

const char *ags_geometry_column_wkt(
  const ags_geometry_column *geometry,
  size_t row_index
) {
  if (geometry == NULL ||
      geometry->encoding != AGS_GEOMETRY_WKT ||
      row_index >= geometry->row_count ||
      geometry->is_null[row_index] != 0) {
    return NULL;
  }

  return geometry->wkt_values[row_index];
}

const unsigned char *ags_geometry_column_wkb(
  const ags_geometry_column *geometry,
  size_t row_index,
  size_t *out_length
) {
  if (out_length != NULL) {
    *out_length = 0;
  }

  if (geometry == NULL ||
      geometry->encoding != AGS_GEOMETRY_WKB ||
      row_index >= geometry->row_count ||
      geometry->is_null[row_index] != 0) {
    return NULL;
  }

  if (out_length != NULL) {
    *out_length = geometry->wkb_lengths[row_index];
  }

  return geometry->wkb_values[row_index];
}
