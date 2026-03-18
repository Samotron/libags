#include <string.h>

#include "libags/merge.h"
#include "dictionary_internal.h"
#include "document_internal.h"

typedef struct ags_merge_source_ref {
  size_t source_document_index;
  size_t line_number;
} ags_merge_source_ref;

typedef struct ags_merge_row_provenance {
  ags_merge_source_ref *sources;
  size_t source_count;
  size_t source_capacity;
} ags_merge_row_provenance;

typedef struct ags_merge_group_provenance {
  ags_merge_row_provenance *rows;
  size_t row_count;
  size_t row_capacity;
} ags_merge_group_provenance;

typedef struct ags_merge_diagnostic_internal {
  char *message;
  char *group;
  char *field;
  size_t source_document_index;
  size_t line_number;
  ags_diagnostic_severity severity;
} ags_merge_diagnostic_internal;

struct ags_merge_result {
  ags_allocator allocator;
  ags_document *document;
  ags_merge_group_provenance *groups;
  size_t group_count;
  size_t group_capacity;
  ags_merge_diagnostic_internal *diagnostics;
  size_t diagnostic_count;
  size_t diagnostic_capacity;
};

static ags_status pick_allocator_from_merge_options(
  const ags_merge_options *options,
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

ags_status ags_merge_options_init(ags_merge_options *options) {
  if (options == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  options->struct_size = sizeof(*options);
  options->allocator = NULL;
  options->dictionary_version = NULL;
  options->dictionary_document = NULL;
  options->keyed_row_policy = AGS_MERGE_CONFLICT_KEEP_FIRST;
  options->singleton_group_policy = AGS_MERGE_CONFLICT_KEEP_FIRST;
  options->value_resolver = NULL;
  options->value_resolver_user_data = NULL;
  return AGS_STATUS_OK;
}

static char *copy_string_or_empty(const ags_allocator *allocator, const char *value) {
  const char *actual = value == NULL ? "" : value;
  return ags_strndup_alloc(allocator, actual, strlen(actual));
}

static void diagnostic_clear(
  ags_merge_result *result,
  ags_merge_diagnostic_internal *diagnostic
) {
  if (result == NULL || diagnostic == NULL) {
    return;
  }

  ags_dealloc(&result->allocator, diagnostic->message);
  ags_dealloc(&result->allocator, diagnostic->group);
  ags_dealloc(&result->allocator, diagnostic->field);
  memset(diagnostic, 0, sizeof(*diagnostic));
}

static ags_status merge_result_add_diagnostic(
  ags_merge_result *result,
  ags_diagnostic_severity severity,
  const char *message,
  const char *group_name,
  const char *field_name,
  size_t source_document_index,
  size_t line_number
) {
  ags_merge_diagnostic_internal *diagnostics = NULL;
  ags_merge_diagnostic_internal *diagnostic = NULL;
  size_t new_capacity = 0;

  if (result == NULL || message == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (result->diagnostic_count == result->diagnostic_capacity) {
    new_capacity = result->diagnostic_capacity == 0 ? 8 : result->diagnostic_capacity * 2;
    diagnostics = ags_realloc_buffer(
      &result->allocator,
      result->diagnostics,
      new_capacity * sizeof(*diagnostics)
    );
    if (diagnostics == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }

    memset(
      diagnostics + result->diagnostic_capacity,
      0,
      (new_capacity - result->diagnostic_capacity) * sizeof(*diagnostics)
    );
    result->diagnostics = diagnostics;
    result->diagnostic_capacity = new_capacity;
  }

  diagnostic = &result->diagnostics[result->diagnostic_count];
  memset(diagnostic, 0, sizeof(*diagnostic));
  diagnostic->message = copy_string_or_empty(&result->allocator, message);
  if (diagnostic->message == NULL) {
    diagnostic_clear(result, diagnostic);
    return AGS_STATUS_NO_MEMORY;
  }

  if (group_name != NULL) {
    diagnostic->group = copy_string_or_empty(&result->allocator, group_name);
    if (diagnostic->group == NULL) {
      diagnostic_clear(result, diagnostic);
      return AGS_STATUS_NO_MEMORY;
    }
  }

  if (field_name != NULL) {
    diagnostic->field = copy_string_or_empty(&result->allocator, field_name);
    if (diagnostic->field == NULL) {
      diagnostic_clear(result, diagnostic);
      return AGS_STATUS_NO_MEMORY;
    }
  }

  diagnostic->severity = severity;
  diagnostic->source_document_index = source_document_index;
  diagnostic->line_number = line_number;
  result->diagnostic_count += 1;
  return AGS_STATUS_OK;
}

static void provenance_clear(
  const ags_allocator *allocator,
  ags_merge_group_provenance *group_provenance
) {
  size_t row_index = 0;

  if (allocator == NULL || group_provenance == NULL) {
    return;
  }

  for (row_index = 0; row_index < group_provenance->row_count; ++row_index) {
    ags_dealloc(allocator, group_provenance->rows[row_index].sources);
  }

  ags_dealloc(allocator, group_provenance->rows);
  memset(group_provenance, 0, sizeof(*group_provenance));
}

void ags_merge_result_destroy(ags_merge_result *result) {
  size_t group_index = 0;
  size_t diagnostic_index = 0;

  if (result == NULL) {
    return;
  }

  for (group_index = 0; group_index < result->group_count; ++group_index) {
    provenance_clear(&result->allocator, &result->groups[group_index]);
  }
  ags_dealloc(&result->allocator, result->groups);

  for (diagnostic_index = 0; diagnostic_index < result->diagnostic_count; ++diagnostic_index) {
    diagnostic_clear(result, &result->diagnostics[diagnostic_index]);
  }
  ags_dealloc(&result->allocator, result->diagnostics);

  ags_document_destroy(result->document);
  ags_dealloc(&result->allocator, result);
}

static ags_status merge_result_create(
  const ags_merge_options *options,
  ags_merge_result **out_result
) {
  ags_allocator allocator;
  ags_document_options document_options;
  ags_merge_result *result = NULL;
  ags_status status = AGS_STATUS_OK;

  if (out_result == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_result = NULL;

  status = pick_allocator_from_merge_options(options, &allocator);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  result = ags_alloc(&allocator, sizeof(*result));
  if (result == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }
  memset(result, 0, sizeof(*result));
  memcpy(&result->allocator, &allocator, sizeof(result->allocator));

  status = ags_document_options_init(&document_options);
  if (status != AGS_STATUS_OK) {
    ags_merge_result_destroy(result);
    return status;
  }
  document_options.allocator = &result->allocator;

  status = ags_document_create(&document_options, &result->document);
  if (status != AGS_STATUS_OK) {
    ags_merge_result_destroy(result);
    return status;
  }

  *out_result = result;
  return AGS_STATUS_OK;
}

static ags_group_internal *find_group_mutable(ags_document *document, const char *group_name, size_t *out_index) {
  size_t group_index = 0;

  if (document == NULL || group_name == NULL) {
    return NULL;
  }

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    if (strcmp(document->groups[group_index].name, group_name) == 0) {
      if (out_index != NULL) {
        *out_index = group_index;
      }
      return &document->groups[group_index];
    }
  }

  return NULL;
}

static size_t find_field_index(const ags_group_internal *group, const char *field_name) {
  size_t field_index = 0;

  if (group == NULL || field_name == NULL) {
    return (size_t)-1;
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    if (strcmp(group->fields[field_index].name, field_name) == 0) {
      return field_index;
    }
  }

  return (size_t)-1;
}

static int is_row_singleton_group(const char *group_name) {
  return strcmp(group_name, "PROJ") == 0 || strcmp(group_name, "TRAN") == 0;
}

static int uses_metadata_group_policy(const char *group_name) {
  return strcmp(group_name, "PROJ") == 0 ||
    strcmp(group_name, "TRAN") == 0 ||
    strcmp(group_name, "TYPE") == 0 ||
    strcmp(group_name, "UNIT") == 0 ||
    strcmp(group_name, "ABBR") == 0 ||
    strcmp(group_name, "DICT") == 0;
}

static ags_status provenance_reserve_groups(ags_merge_result *result, size_t required) {
  ags_merge_group_provenance *groups = NULL;
  size_t new_capacity = 0;

  if (result == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (required <= result->group_capacity) {
    return AGS_STATUS_OK;
  }

  new_capacity = result->group_capacity == 0 ? 4 : result->group_capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  groups = ags_realloc_buffer(&result->allocator, result->groups, new_capacity * sizeof(*groups));
  if (groups == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(groups + result->group_capacity, 0, (new_capacity - result->group_capacity) * sizeof(*groups));
  result->groups = groups;
  result->group_capacity = new_capacity;
  return AGS_STATUS_OK;
}

static ags_status provenance_reserve_rows(
  ags_merge_result *result,
  ags_merge_group_provenance *group_provenance,
  size_t required
) {
  ags_merge_row_provenance *rows = NULL;
  size_t new_capacity = 0;

  if (result == NULL || group_provenance == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (required <= group_provenance->row_capacity) {
    return AGS_STATUS_OK;
  }

  new_capacity = group_provenance->row_capacity == 0 ? 4 : group_provenance->row_capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  rows = ags_realloc_buffer(&result->allocator, group_provenance->rows, new_capacity * sizeof(*rows));
  if (rows == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(
    rows + group_provenance->row_capacity,
    0,
    (new_capacity - group_provenance->row_capacity) * sizeof(*rows)
  );
  group_provenance->rows = rows;
  group_provenance->row_capacity = new_capacity;
  return AGS_STATUS_OK;
}

static ags_status provenance_add_source(
  ags_merge_result *result,
  size_t group_index,
  size_t row_index,
  size_t source_document_index,
  size_t line_number
) {
  ags_merge_group_provenance *group_provenance = NULL;
  ags_merge_row_provenance *row_provenance = NULL;
  ags_merge_source_ref *sources = NULL;
  size_t new_capacity = 0;

  if (result == NULL || group_index >= result->group_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  group_provenance = &result->groups[group_index];
  if (row_index >= group_provenance->row_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  row_provenance = &group_provenance->rows[row_index];
  if (row_provenance->source_count == row_provenance->source_capacity) {
    new_capacity = row_provenance->source_capacity == 0 ? 2 : row_provenance->source_capacity * 2;
    sources = ags_realloc_buffer(
      &result->allocator,
      row_provenance->sources,
      new_capacity * sizeof(*sources)
    );
    if (sources == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    row_provenance->sources = sources;
    row_provenance->source_capacity = new_capacity;
  }

  row_provenance->sources[row_provenance->source_count].source_document_index = source_document_index;
  row_provenance->sources[row_provenance->source_count].line_number = line_number;
  row_provenance->source_count += 1;
  return AGS_STATUS_OK;
}

static ags_status result_add_group(
  ags_merge_result *result,
  const ags_group_internal *source_group,
  size_t *out_group_index
) {
  ags_group_internal *group = NULL;
  ags_status status = AGS_STATUS_OK;
  size_t field_index = 0;

  if (result == NULL || source_group == NULL || out_group_index == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = ags_document_reserve_groups(result->document, result->document->group_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = provenance_reserve_groups(result, result->group_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  group = &result->document->groups[result->document->group_count];
  memset(group, 0, sizeof(*group));
  group->name = copy_string_or_empty(&result->allocator, source_group->name);
  if (group->name == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  group->field_count = source_group->field_count;
  group->group_line_number = source_group->group_line_number;
  group->heading_line_number = source_group->heading_line_number;
  group->unit_line_number = source_group->unit_line_number;
  group->type_line_number = source_group->type_line_number;

  if (group->field_count > 0) {
    group->fields = ags_alloc(&result->allocator, group->field_count * sizeof(*group->fields));
    if (group->fields == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    memset(group->fields, 0, group->field_count * sizeof(*group->fields));
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    group->fields[field_index].name = copy_string_or_empty(&result->allocator, source_group->fields[field_index].name);
    group->fields[field_index].unit = copy_string_or_empty(&result->allocator, source_group->fields[field_index].unit);
    group->fields[field_index].type = copy_string_or_empty(&result->allocator, source_group->fields[field_index].type);
    if (group->fields[field_index].name == NULL ||
        group->fields[field_index].unit == NULL ||
        group->fields[field_index].type == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  memset(&result->groups[result->group_count], 0, sizeof(result->groups[result->group_count]));
  *out_group_index = result->document->group_count;
  result->document->group_count += 1;
  result->group_count += 1;
  return AGS_STATUS_OK;
}

static ags_status ensure_group_field(
  ags_merge_result *result,
  size_t group_index,
  const char *field_name,
  const char *unit,
  const char *type,
  size_t source_document_index,
  size_t line_number,
  size_t *out_field_index
) {
  ags_group_internal *group = NULL;
  size_t field_index = 0;
  ags_field_internal *fields = NULL;
  size_t row_index = 0;

  if (result == NULL || field_name == NULL || out_field_index == NULL || group_index >= result->document->group_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  group = &result->document->groups[group_index];
  field_index = find_field_index(group, field_name);
  if (field_index != (size_t)-1) {
    if (group->fields[field_index].unit[0] == '\0' && unit != NULL && unit[0] != '\0') {
      ags_dealloc(&result->allocator, group->fields[field_index].unit);
      group->fields[field_index].unit = copy_string_or_empty(&result->allocator, unit);
      if (group->fields[field_index].unit == NULL) {
        return AGS_STATUS_NO_MEMORY;
      }
    } else if (unit != NULL && unit[0] != '\0' && strcmp(group->fields[field_index].unit, unit) != 0) {
      if (merge_result_add_diagnostic(
            result,
            AGS_DIAGNOSTIC_WARNING,
            "Conflicting field UNIT metadata encountered during merge; keeping first value.",
            group->name,
            field_name,
            source_document_index,
            line_number
          ) != AGS_STATUS_OK) {
        return AGS_STATUS_NO_MEMORY;
      }
    }

    if (group->fields[field_index].type[0] == '\0' && type != NULL && type[0] != '\0') {
      ags_dealloc(&result->allocator, group->fields[field_index].type);
      group->fields[field_index].type = copy_string_or_empty(&result->allocator, type);
      if (group->fields[field_index].type == NULL) {
        return AGS_STATUS_NO_MEMORY;
      }
    } else if (type != NULL && type[0] != '\0' && strcmp(group->fields[field_index].type, type) != 0) {
      if (merge_result_add_diagnostic(
            result,
            AGS_DIAGNOSTIC_WARNING,
            "Conflicting field TYPE metadata encountered during merge; keeping first value.",
            group->name,
            field_name,
            source_document_index,
            line_number
          ) != AGS_STATUS_OK) {
        return AGS_STATUS_NO_MEMORY;
      }
    }

    *out_field_index = field_index;
    return AGS_STATUS_OK;
  }

  fields = ags_realloc_buffer(
    &result->allocator,
    group->fields,
    (group->field_count + 1) * sizeof(*fields)
  );
  if (fields == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }
  group->fields = fields;
  memset(&group->fields[group->field_count], 0, sizeof(group->fields[group->field_count]));

  group->fields[group->field_count].name = copy_string_or_empty(&result->allocator, field_name);
  group->fields[group->field_count].unit = copy_string_or_empty(&result->allocator, unit);
  group->fields[group->field_count].type = copy_string_or_empty(&result->allocator, type);
  if (group->fields[group->field_count].name == NULL ||
      group->fields[group->field_count].unit == NULL ||
      group->fields[group->field_count].type == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  for (row_index = 0; row_index < group->row_count; ++row_index) {
    char **values = ags_realloc_buffer(
      &result->allocator,
      group->rows[row_index].values,
      (group->field_count + 1) * sizeof(*values)
    );
    if (values == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    group->rows[row_index].values = values;
    group->rows[row_index].values[group->field_count] = copy_string_or_empty(&result->allocator, "");
    if (group->rows[row_index].values[group->field_count] == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  *out_field_index = group->field_count;
  group->field_count += 1;
  return AGS_STATUS_OK;
}

static ags_status append_row_to_group(
  ags_merge_result *result,
  size_t group_index,
  const ags_group_internal *source_group,
  size_t row_index,
  size_t source_document_index
) {
  ags_group_internal *group = NULL;
  ags_merge_group_provenance *group_provenance = NULL;
  ags_row_internal *row = NULL;
  ags_status status = AGS_STATUS_OK;
  size_t merged_field_index = 0;

  if (result == NULL ||
      source_group == NULL ||
      group_index >= result->document->group_count ||
      row_index >= source_group->row_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  group = &result->document->groups[group_index];
  group_provenance = &result->groups[group_index];

  status = ags_group_reserve_rows(result->document, group, group->row_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = provenance_reserve_rows(result, group_provenance, group->row_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  row = &group->rows[group->row_count];
  memset(row, 0, sizeof(*row));
  row->line_number = source_group->rows[row_index].line_number;
  row->values = ags_alloc(&result->allocator, group->field_count * sizeof(*row->values));
  if (row->values == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }
  memset(row->values, 0, group->field_count * sizeof(*row->values));

  for (merged_field_index = 0; merged_field_index < group->field_count; ++merged_field_index) {
    size_t source_field_index = find_field_index(source_group, group->fields[merged_field_index].name);
    const char *value = source_field_index == (size_t)-1
      ? ""
      : source_group->rows[row_index].values[source_field_index];
    row->values[merged_field_index] = copy_string_or_empty(&result->allocator, value);
    if (row->values[merged_field_index] == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  group_provenance->row_count = group->row_count + 1;
  group->row_count += 1;

  status = provenance_add_source(
    result,
    group_index,
    group->row_count - 1,
    source_document_index,
    source_group->rows[row_index].line_number
  );
  return status;
}

static ags_status collect_key_field_names(
  const ags_effective_dictionary *dictionary,
  const ags_allocator *allocator,
  const char *group_name,
  const char ***out_names,
  size_t *out_count
) {
  const char **names = NULL;
  size_t count = 0;
  size_t entry_index = 0;

  if (dictionary == NULL || allocator == NULL || group_name == NULL || out_names == NULL || out_count == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_names = NULL;
  *out_count = 0;

  for (entry_index = 0; entry_index < dictionary->entry_count; ++entry_index) {
    const ags_dict_entry_view *entry = &dictionary->entries[entry_index];
    const char **new_names = NULL;

    if (strcmp(entry->dict_type, "HEADING") != 0 ||
        strcmp(entry->group, group_name) != 0 ||
        !ags_dict_stat_contains(entry, "KEY")) {
      continue;
    }

    new_names = ags_realloc_buffer(allocator, names, (count + 1) * sizeof(*new_names));
    if (new_names == NULL) {
      ags_dealloc(allocator, names);
      return AGS_STATUS_NO_MEMORY;
    }
    names = new_names;
    names[count++] = entry->heading;
  }

  *out_names = names;
  *out_count = count;
  return AGS_STATUS_OK;
}

static int rows_match_on_keys(
  const ags_group_internal *merged_group,
  size_t merged_row_index,
  const ags_group_internal *incoming_group,
  size_t incoming_row_index,
  const char *const *key_names,
  size_t key_count
) {
  size_t key_index = 0;

  if (key_count == 0) {
    return 0;
  }

  for (key_index = 0; key_index < key_count; ++key_index) {
    size_t merged_field_index = find_field_index(merged_group, key_names[key_index]);
    size_t incoming_field_index = find_field_index(incoming_group, key_names[key_index]);
    const char *merged_value = NULL;
    const char *incoming_value = NULL;

    if (merged_field_index == (size_t)-1 || incoming_field_index == (size_t)-1) {
      return 0;
    }

    merged_value = merged_group->rows[merged_row_index].values[merged_field_index];
    incoming_value = incoming_group->rows[incoming_row_index].values[incoming_field_index];
    if (merged_value[0] == '\0' || incoming_value[0] == '\0') {
      return 0;
    }
    if (strcmp(merged_value, incoming_value) != 0) {
      return 0;
    }
  }

  return 1;
}

static ags_status resolve_conflict_value(
  ags_merge_result *result,
  const ags_merge_options *options,
  const char *group_name,
  const char *field_name,
  const char *existing_value,
  const char *incoming_value,
  size_t source_document_index,
  size_t line_number,
  ags_merge_conflict_policy policy,
  char **out_replacement,
  int *out_replace
) {
  ags_status status = AGS_STATUS_OK;
  const char *message = NULL;
  ags_diagnostic_severity severity = AGS_DIAGNOSTIC_WARNING;

  if (result == NULL || group_name == NULL || field_name == NULL || existing_value == NULL ||
      incoming_value == NULL || out_replacement == NULL || out_replace == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_replacement = NULL;
  *out_replace = 0;

  if (strcmp(existing_value, incoming_value) == 0 || incoming_value[0] == '\0') {
    return AGS_STATUS_OK;
  }

  if (existing_value[0] == '\0') {
    *out_replacement = copy_string_or_empty(&result->allocator, incoming_value);
    if (*out_replacement == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    *out_replace = 1;
    return AGS_STATUS_OK;
  }

  if (strcmp(group_name, "DICT") == 0) {
    message = "Incompatible custom dictionary definition encountered during merge.";
    severity = AGS_DIAGNOSTIC_ERROR;
  } else {
    message = "Conflicting keyed row values encountered during merge.";
    severity = policy == AGS_MERGE_CONFLICT_FAIL ? AGS_DIAGNOSTIC_ERROR : AGS_DIAGNOSTIC_WARNING;
  }

  if (policy == AGS_MERGE_CONFLICT_KEEP_LAST) {
    *out_replacement = copy_string_or_empty(&result->allocator, incoming_value);
    if (*out_replacement == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
    *out_replace = 1;
  } else if (policy == AGS_MERGE_CONFLICT_CALLBACK) {
    if (options == NULL || options->value_resolver == NULL) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }
    status = options->value_resolver(
      options->value_resolver_user_data,
      group_name,
      field_name,
      existing_value,
      incoming_value,
      &result->allocator,
      out_replacement
    );
    if (status != AGS_STATUS_OK) {
      return status;
    }
    if (*out_replacement == NULL) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }
    *out_replace = 1;
  }

  return merge_result_add_diagnostic(
    result,
    severity,
    message,
    group_name,
    field_name,
    source_document_index,
    line_number
  );
}

static ags_status merge_row_into_existing(
  ags_merge_result *result,
  size_t group_index,
  size_t merged_row_index,
  const ags_group_internal *incoming_group,
  size_t incoming_row_index,
  size_t source_document_index,
  const ags_merge_options *options,
  ags_merge_conflict_policy policy
) {
  ags_group_internal *merged_group = NULL;
  size_t field_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (result == NULL || incoming_group == NULL || group_index >= result->document->group_count) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  merged_group = &result->document->groups[group_index];
  for (field_index = 0; field_index < merged_group->field_count; ++field_index) {
    const char *field_name = merged_group->fields[field_index].name;
    size_t incoming_field_index = find_field_index(incoming_group, field_name);
    const char *incoming_value = incoming_field_index == (size_t)-1
      ? ""
      : incoming_group->rows[incoming_row_index].values[incoming_field_index];
    const char *existing_value = merged_group->rows[merged_row_index].values[field_index];
    char *replacement = NULL;
    int replace = 0;

    status = resolve_conflict_value(
      result,
      options,
      merged_group->name,
      field_name,
      existing_value,
      incoming_value,
      source_document_index,
      incoming_group->rows[incoming_row_index].line_number,
      policy,
      &replacement,
      &replace
    );
    if (status != AGS_STATUS_OK) {
      return status;
    }

    if (replace) {
      ags_dealloc(&result->allocator, merged_group->rows[merged_row_index].values[field_index]);
      merged_group->rows[merged_row_index].values[field_index] = replacement;
    }
  }

  return provenance_add_source(
    result,
    group_index,
    merged_row_index,
    source_document_index,
    incoming_group->rows[incoming_row_index].line_number
  );
}

static ags_status ensure_group_schema(
  ags_merge_result *result,
  size_t group_index,
  const ags_group_internal *incoming_group,
  size_t source_document_index
) {
  size_t field_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (result == NULL || incoming_group == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  for (field_index = 0; field_index < incoming_group->field_count; ++field_index) {
    size_t merged_field_index = 0;
    status = ensure_group_field(
      result,
      group_index,
      incoming_group->fields[field_index].name,
      incoming_group->fields[field_index].unit,
      incoming_group->fields[field_index].type,
      source_document_index,
      incoming_group->heading_line_number,
      &merged_field_index
    );
    if (status != AGS_STATUS_OK) {
      return status;
    }
  }

  return AGS_STATUS_OK;
}

static ags_status merge_group_rows(
  ags_merge_result *result,
  size_t group_index,
  const ags_group_internal *incoming_group,
  size_t source_document_index,
  const ags_effective_dictionary *dictionary,
  const ags_merge_options *options
) {
  ags_group_internal *merged_group = NULL;
  const char **key_names = NULL;
  size_t key_count = 0;
  size_t incoming_row_index = 0;
  ags_status status = AGS_STATUS_OK;
  ags_merge_conflict_policy match_policy = AGS_MERGE_CONFLICT_KEEP_FIRST;

  if (result == NULL || incoming_group == NULL || dictionary == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  merged_group = &result->document->groups[group_index];
  if (options != NULL && options->struct_size != sizeof(*options)) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (uses_metadata_group_policy(merged_group->name)) {
    match_policy = options == NULL ? AGS_MERGE_CONFLICT_KEEP_FIRST : options->singleton_group_policy;
  } else {
    match_policy = options == NULL ? AGS_MERGE_CONFLICT_KEEP_FIRST : options->keyed_row_policy;
  }

  status = collect_key_field_names(dictionary, &result->allocator, merged_group->name, &key_names, &key_count);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  if (is_row_singleton_group(merged_group->name) && merged_group->row_count > 0 && incoming_group->row_count > 0) {
    status = merge_row_into_existing(
      result,
      group_index,
      0,
      incoming_group,
      0,
      source_document_index,
      options,
      match_policy
    );
    if (status != AGS_STATUS_OK) {
      ags_dealloc(&result->allocator, key_names);
      return status;
    }

    for (incoming_row_index = 1; incoming_row_index < incoming_group->row_count; ++incoming_row_index) {
      status = merge_result_add_diagnostic(
        result,
        AGS_DIAGNOSTIC_ERROR,
        "Singleton group contains multiple rows during merge.",
        merged_group->name,
        NULL,
        source_document_index,
        incoming_group->rows[incoming_row_index].line_number
      );
      if (status != AGS_STATUS_OK) {
        ags_dealloc(&result->allocator, key_names);
        return status;
      }
    }

    ags_dealloc(&result->allocator, key_names);
    return AGS_STATUS_OK;
  }

  for (incoming_row_index = 0; incoming_row_index < incoming_group->row_count; ++incoming_row_index) {
    size_t merged_row_index = 0;
    int matched = 0;

    if (key_count > 0) {
      for (merged_row_index = 0; merged_row_index < merged_group->row_count; ++merged_row_index) {
        if (rows_match_on_keys(
              merged_group,
              merged_row_index,
              incoming_group,
              incoming_row_index,
              key_names,
              key_count
            )) {
          matched = 1;
          break;
        }
      }
    }

    if (matched) {
      status = merge_row_into_existing(
        result,
        group_index,
        merged_row_index,
        incoming_group,
        incoming_row_index,
        source_document_index,
        options,
        match_policy
      );
    } else {
      status = append_row_to_group(result, group_index, incoming_group, incoming_row_index, source_document_index);
    }

    if (status != AGS_STATUS_OK) {
      ags_dealloc(&result->allocator, key_names);
      return status;
    }
  }

  ags_dealloc(&result->allocator, key_names);
  return AGS_STATUS_OK;
}

static ags_status build_effective_dictionary_for_merge(
  const ags_document *const *documents,
  size_t document_count,
  const ags_merge_options *options,
  const ags_allocator *allocator,
  ags_document **out_loaded_dictionary,
  ags_effective_dictionary *out_dictionary
) {
  ags_document *loaded_dictionary = NULL;
  const ags_document *standard_dictionary = NULL;
  const char *resolved_version = NULL;
  ags_document_options document_options;
  ags_status status = AGS_STATUS_OK;
  size_t document_index = 0;

  if (documents == NULL || allocator == NULL || out_loaded_dictionary == NULL || out_dictionary == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_loaded_dictionary = NULL;
  memset(out_dictionary, 0, sizeof(*out_dictionary));

  if (options != NULL && options->dictionary_document != NULL) {
    standard_dictionary = options->dictionary_document;
  } else {
    status = ags_dictionary_resolve_version(
      document_count == 0 ? NULL : documents[0],
      options == NULL ? NULL : options->dictionary_version,
      &resolved_version
    );
    if (status != AGS_STATUS_OK) {
      return status;
    }

    status = ags_document_options_init(&document_options);
    if (status != AGS_STATUS_OK) {
      return status;
    }
    document_options.allocator = allocator;

    status = ags_dictionary_load_bundled(resolved_version, &document_options, &loaded_dictionary);
    if (status != AGS_STATUS_OK) {
      return status;
    }
    standard_dictionary = loaded_dictionary;
  }

  status = ags_effective_dictionary_build(
    out_dictionary,
    allocator,
    standard_dictionary,
    document_count == 0 ? NULL : documents[0]
  );
  if (status != AGS_STATUS_OK) {
    ags_document_destroy(loaded_dictionary);
    return status;
  }

  for (document_index = 1; document_index < document_count; ++document_index) {
    status = ags_effective_dictionary_append_document(out_dictionary, documents[document_index], 1);
    if (status != AGS_STATUS_OK) {
      ags_effective_dictionary_destroy(out_dictionary);
      ags_document_destroy(loaded_dictionary);
      return status;
    }
  }

  *out_loaded_dictionary = loaded_dictionary;
  return AGS_STATUS_OK;
}

static ags_status add_parent_child_merge_diagnostics(
  ags_merge_result *result,
  const ags_merge_options *options
) {
  ags_validate_options validate_options;
  ags_validation_report *report = NULL;
  size_t diagnostic_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (result == NULL || result->document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = ags_validate_options_init(&validate_options);
  if (status != AGS_STATUS_OK) {
    return status;
  }
  validate_options.allocator = &result->allocator;
  if (options != NULL) {
    validate_options.dictionary_version = options->dictionary_version;
    validate_options.dictionary_document = options->dictionary_document;
  }

  status = ags_validate_document_with_dictionary(result->document, &validate_options, &report);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (diagnostic_index = 0; diagnostic_index < ags_validation_report_diagnostic_count(report); ++diagnostic_index) {
    const char *rule = ags_validation_report_diagnostic_rule(report, diagnostic_index);
    if (rule != NULL && strcmp(rule, "10c") == 0) {
      status = merge_result_add_diagnostic(
        result,
        ags_validation_report_diagnostic_severity(report, diagnostic_index),
        ags_validation_report_diagnostic_message(report, diagnostic_index),
        ags_validation_report_diagnostic_group(report, diagnostic_index),
        ags_validation_report_diagnostic_field(report, diagnostic_index),
        (size_t)-1,
        ags_validation_report_diagnostic_line_number(report, diagnostic_index)
      );
      if (status != AGS_STATUS_OK) {
        ags_validation_report_destroy(report);
        return status;
      }
    }
  }

  ags_validation_report_destroy(report);
  return AGS_STATUS_OK;
}

ags_status ags_document_merge(
  const ags_document *const *documents,
  size_t document_count,
  const ags_merge_options *options,
  ags_merge_result **out_result
) {
  ags_merge_result *result = NULL;
  ags_document *loaded_dictionary = NULL;
  ags_effective_dictionary dictionary;
  ags_status status = AGS_STATUS_OK;
  size_t document_index = 0;

  if (out_result == NULL || (document_count > 0 && documents == NULL)) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_result = NULL;

  status = merge_result_create(options, &result);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = build_effective_dictionary_for_merge(
    documents,
    document_count,
    options,
    &result->allocator,
    &loaded_dictionary,
    &dictionary
  );
  if (status != AGS_STATUS_OK) {
    ags_merge_result_destroy(result);
    return status;
  }

  for (document_index = 0; document_index < document_count; ++document_index) {
    size_t group_index = 0;

    for (group_index = 0; group_index < documents[document_index]->group_count; ++group_index) {
      const ags_group_internal *incoming_group = &documents[document_index]->groups[group_index];
      size_t merged_group_index = 0;
      ags_group_internal *merged_group = find_group_mutable(result->document, incoming_group->name, &merged_group_index);

      if (merged_group == NULL) {
        status = result_add_group(result, incoming_group, &merged_group_index);
        if (status != AGS_STATUS_OK) {
          ags_effective_dictionary_destroy(&dictionary);
          ags_document_destroy(loaded_dictionary);
          ags_merge_result_destroy(result);
          return status;
        }
      }

      status = ensure_group_schema(result, merged_group_index, incoming_group, document_index);
      if (status != AGS_STATUS_OK) {
        ags_effective_dictionary_destroy(&dictionary);
        ags_document_destroy(loaded_dictionary);
        ags_merge_result_destroy(result);
        return status;
      }

      status = merge_group_rows(
        result,
        merged_group_index,
        incoming_group,
        document_index,
        &dictionary,
        options
      );
      if (status != AGS_STATUS_OK) {
        ags_effective_dictionary_destroy(&dictionary);
        ags_document_destroy(loaded_dictionary);
        ags_merge_result_destroy(result);
        return status;
      }
    }
  }

  status = add_parent_child_merge_diagnostics(result, options);
  ags_effective_dictionary_destroy(&dictionary);
  ags_document_destroy(loaded_dictionary);
  if (status != AGS_STATUS_OK) {
    ags_merge_result_destroy(result);
    return status;
  }

  *out_result = result;
  return AGS_STATUS_OK;
}

const ags_document *ags_merge_result_document(const ags_merge_result *result) {
  if (result == NULL) {
    return NULL;
  }

  return result->document;
}

size_t ags_merge_result_diagnostic_count(const ags_merge_result *result) {
  if (result == NULL) {
    return 0;
  }

  return result->diagnostic_count;
}

ags_diagnostic_severity ags_merge_result_diagnostic_severity(
  const ags_merge_result *result,
  size_t diagnostic_index
) {
  if (result == NULL || diagnostic_index >= result->diagnostic_count) {
    return AGS_DIAGNOSTIC_ERROR;
  }

  return result->diagnostics[diagnostic_index].severity;
}

const char *ags_merge_result_diagnostic_message(
  const ags_merge_result *result,
  size_t diagnostic_index
) {
  if (result == NULL || diagnostic_index >= result->diagnostic_count) {
    return NULL;
  }

  return result->diagnostics[diagnostic_index].message;
}

const char *ags_merge_result_diagnostic_group(
  const ags_merge_result *result,
  size_t diagnostic_index
) {
  if (result == NULL || diagnostic_index >= result->diagnostic_count) {
    return NULL;
  }

  return result->diagnostics[diagnostic_index].group;
}

const char *ags_merge_result_diagnostic_field(
  const ags_merge_result *result,
  size_t diagnostic_index
) {
  if (result == NULL || diagnostic_index >= result->diagnostic_count) {
    return NULL;
  }

  return result->diagnostics[diagnostic_index].field;
}

size_t ags_merge_result_diagnostic_source_document(
  const ags_merge_result *result,
  size_t diagnostic_index
) {
  if (result == NULL || diagnostic_index >= result->diagnostic_count) {
    return 0;
  }

  return result->diagnostics[diagnostic_index].source_document_index;
}

size_t ags_merge_result_diagnostic_line_number(
  const ags_merge_result *result,
  size_t diagnostic_index
) {
  if (result == NULL || diagnostic_index >= result->diagnostic_count) {
    return 0;
  }

  return result->diagnostics[diagnostic_index].line_number;
}

size_t ags_merge_result_row_source_count(
  const ags_merge_result *result,
  size_t group_index,
  size_t row_index
) {
  if (result == NULL ||
      group_index >= result->group_count ||
      row_index >= result->groups[group_index].row_count) {
    return 0;
  }

  return result->groups[group_index].rows[row_index].source_count;
}

size_t ags_merge_result_row_source_document(
  const ags_merge_result *result,
  size_t group_index,
  size_t row_index,
  size_t source_index
) {
  if (result == NULL ||
      group_index >= result->group_count ||
      row_index >= result->groups[group_index].row_count ||
      source_index >= result->groups[group_index].rows[row_index].source_count) {
    return 0;
  }

  return result->groups[group_index].rows[row_index].sources[source_index].source_document_index;
}

size_t ags_merge_result_row_source_line_number(
  const ags_merge_result *result,
  size_t group_index,
  size_t row_index,
  size_t source_index
) {
  if (result == NULL ||
      group_index >= result->group_count ||
      row_index >= result->groups[group_index].row_count ||
      source_index >= result->groups[group_index].rows[row_index].source_count) {
    return 0;
  }

  return result->groups[group_index].rows[row_index].sources[source_index].line_number;
}
