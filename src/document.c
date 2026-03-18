#include <stddef.h>
#include <string.h>

#include "libags/document.h"
#include "document_internal.h"

ags_status ags_document_options_init(ags_document_options *options) {
  if (options == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  options->struct_size = sizeof(*options);
  options->allocator = NULL;
  return AGS_STATUS_OK;
}

ags_status ags_serialize_options_init(ags_serialize_options *options) {
  if (options == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  options->struct_size = sizeof(*options);
  options->newline_mode = AGS_NEWLINE_CRLF;
  return AGS_STATUS_OK;
}

ags_status ags_document_pick_allocator(
  const ags_document_options *options,
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

void *ags_alloc(const ags_allocator *allocator, size_t size) {
  size_t actual_size = size;

  if (actual_size == 0) {
    actual_size = 1;
  }

  return allocator->malloc_fn(allocator->user_data, actual_size);
}

void *ags_realloc_buffer(const ags_allocator *allocator, void *ptr, size_t size) {
  size_t actual_size = size;

  if (actual_size == 0) {
    actual_size = 1;
  }

  return allocator->realloc_fn(allocator->user_data, ptr, actual_size);
}

void ags_dealloc(const ags_allocator *allocator, void *ptr) {
  if (ptr == NULL) {
    return;
  }

  allocator->free_fn(allocator->user_data, ptr);
}

char *ags_strndup_alloc(const ags_allocator *allocator, const char *src, size_t length) {
  char *copy = NULL;

  copy = ags_alloc(allocator, length + 1);
  if (copy == NULL) {
    return NULL;
  }

  if (length > 0) {
    memcpy(copy, src, length);
  }

  copy[length] = '\0';
  return copy;
}

static void ags_group_clear(ags_document *document, ags_group_internal *group) {
  size_t field_index = 0;
  size_t row_index = 0;

  if (document == NULL || group == NULL) {
    return;
  }

  ags_dealloc(&document->allocator, group->name);

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    ags_dealloc(&document->allocator, group->fields[field_index].name);
    ags_dealloc(&document->allocator, group->fields[field_index].unit);
    ags_dealloc(&document->allocator, group->fields[field_index].type);
  }

  for (row_index = 0; row_index < group->row_count; ++row_index) {
    size_t value_index = 0;
    for (value_index = 0; value_index < group->field_count; ++value_index) {
      ags_dealloc(&document->allocator, group->rows[row_index].values[value_index]);
    }

    ags_dealloc(&document->allocator, group->rows[row_index].values);
  }

  ags_dealloc(&document->allocator, group->fields);
  ags_dealloc(&document->allocator, group->rows);
  memset(group, 0, sizeof(*group));
}

void ags_document_reset(ags_document *document) {
  size_t group_index = 0;

  if (document == NULL) {
    return;
  }

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    ags_group_clear(document, &document->groups[group_index]);
  }

  ags_dealloc(&document->allocator, document->groups);
  document->groups = NULL;
  document->group_count = 0;
  document->group_capacity = 0;
}

ags_status ags_document_reserve_groups(ags_document *document, size_t required) {
  ags_group_internal *groups = NULL;
  size_t new_capacity = 0;

  if (document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (required <= document->group_capacity) {
    return AGS_STATUS_OK;
  }

  new_capacity = document->group_capacity == 0 ? 4 : document->group_capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  groups = ags_realloc_buffer(
    &document->allocator,
    document->groups,
    new_capacity * sizeof(*groups)
  );
  if (groups == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(groups + document->group_capacity, 0, (new_capacity - document->group_capacity) * sizeof(*groups));
  document->groups = groups;
  document->group_capacity = new_capacity;

  return AGS_STATUS_OK;
}

ags_status ags_group_reserve_rows(
  ags_document *document,
  ags_group_internal *group,
  size_t required
) {
  ags_row_internal *rows = NULL;
  size_t new_capacity = 0;

  if (document == NULL || group == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (required <= group->row_capacity) {
    return AGS_STATUS_OK;
  }

  new_capacity = group->row_capacity == 0 ? 4 : group->row_capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  rows = ags_realloc_buffer(
    &document->allocator,
    group->rows,
    new_capacity * sizeof(*rows)
  );
  if (rows == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(rows + group->row_capacity, 0, (new_capacity - group->row_capacity) * sizeof(*rows));
  group->rows = rows;
  group->row_capacity = new_capacity;

  return AGS_STATUS_OK;
}

ags_group_internal *ags_document_get_group_mutable(ags_document *document, size_t group_index) {
  if (document == NULL || group_index >= document->group_count) {
    return NULL;
  }

  return &document->groups[group_index];
}

const ags_group_internal *ags_document_get_group(const ags_document *document, size_t group_index) {
  if (document == NULL || group_index >= document->group_count) {
    return NULL;
  }

  return &document->groups[group_index];
}

size_t ags_document_find_group_index(const ags_document *document, const char *group_name) {
  size_t group_index = 0;

  if (document == NULL || group_name == NULL) {
    return (size_t)-1;
  }

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    if (strcmp(document->groups[group_index].name, group_name) == 0) {
      return group_index;
    }
  }

  return (size_t)-1;
}

ags_status ags_document_create(
  const ags_document_options *options,
  ags_document **out_document
) {
  ags_allocator allocator;
  ags_document *document = NULL;
  ags_status status;

  if (out_document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_document = NULL;

  status = ags_document_pick_allocator(options, &allocator);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  document = allocator.malloc_fn(allocator.user_data, sizeof(*document));
  if (document == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(document, 0, sizeof(*document));
  document->allocator = allocator;
  *out_document = document;

  return AGS_STATUS_OK;
}

void ags_document_destroy(ags_document *document) {
  if (document == NULL) {
    return;
  }

  ags_document_reset(document);
  document->allocator.free_fn(document->allocator.user_data, document);
}

void ags_document_free_buffer(const ags_document *document, void *buffer) {
  if (document == NULL || buffer == NULL) {
    return;
  }

  document->allocator.free_fn(document->allocator.user_data, buffer);
}

size_t ags_document_group_count(const ags_document *document) {
  if (document == NULL) {
    return 0;
  }

  return document->group_count;
}

const ags_allocator *ags_document_allocator(const ags_document *document) {
  if (document == NULL) {
    return NULL;
  }

  return &document->allocator;
}

ags_status ags_document_find_group(
  const ags_document *document,
  const char *group_name,
  size_t *out_index
) {
  size_t index = 0;

  if (document == NULL || group_name == NULL || out_index == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  index = ags_document_find_group_index(document, group_name);
  if (index == (size_t)-1) {
    return AGS_STATUS_NOT_FOUND;
  }

  *out_index = index;
  return AGS_STATUS_OK;
}

const char *ags_document_group_name(const ags_document *document, size_t group_index) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);
  return group == NULL ? NULL : group->name;
}

size_t ags_document_group_line_number(const ags_document *document, size_t group_index) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);
  return group == NULL ? 0 : group->group_line_number;
}

size_t ags_document_group_heading_line_number(const ags_document *document, size_t group_index) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);
  return group == NULL ? 0 : group->heading_line_number;
}

size_t ags_document_group_unit_line_number(const ags_document *document, size_t group_index) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);
  return group == NULL ? 0 : group->unit_line_number;
}

size_t ags_document_group_type_line_number(const ags_document *document, size_t group_index) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);
  return group == NULL ? 0 : group->type_line_number;
}

size_t ags_document_group_field_count(const ags_document *document, size_t group_index) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);
  return group == NULL ? 0 : group->field_count;
}

size_t ags_document_group_row_count(const ags_document *document, size_t group_index) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);
  return group == NULL ? 0 : group->row_count;
}

const char *ags_document_field_name(
  const ags_document *document,
  size_t group_index,
  size_t field_index
) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);

  if (group == NULL || field_index >= group->field_count) {
    return NULL;
  }

  return group->fields[field_index].name;
}

const char *ags_document_field_unit(
  const ags_document *document,
  size_t group_index,
  size_t field_index
) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);

  if (group == NULL || field_index >= group->field_count) {
    return NULL;
  }

  return group->fields[field_index].unit;
}

const char *ags_document_field_type(
  const ags_document *document,
  size_t group_index,
  size_t field_index
) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);

  if (group == NULL || field_index >= group->field_count) {
    return NULL;
  }

  return group->fields[field_index].type;
}

size_t ags_document_row_line_number(
  const ags_document *document,
  size_t group_index,
  size_t row_index
) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);

  if (group == NULL || row_index >= group->row_count) {
    return 0;
  }

  return group->rows[row_index].line_number;
}

const char *ags_document_cell_value(
  const ags_document *document,
  size_t group_index,
  size_t row_index,
  size_t field_index
) {
  const ags_group_internal *group = ags_document_get_group(document, group_index);

  if (group == NULL || row_index >= group->row_count || field_index >= group->field_count) {
    return NULL;
  }

  return group->rows[row_index].values[field_index];
}
