#include <string.h>

#include "libags/document.h"
#include "document_internal.h"

typedef struct string_builder {
  const ags_allocator *allocator;
  char *data;
  size_t length;
  size_t capacity;
} string_builder;

static ags_status builder_reserve(string_builder *builder, size_t additional) {
  char *buffer = NULL;
  size_t required = 0;
  size_t new_capacity = 0;

  if (builder == NULL || builder->allocator == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  required = builder->length + additional + 1;
  if (required <= builder->capacity) {
    return AGS_STATUS_OK;
  }

  new_capacity = builder->capacity == 0 ? 128 : builder->capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  buffer = ags_realloc_buffer(builder->allocator, builder->data, new_capacity);
  if (buffer == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  builder->data = buffer;
  builder->capacity = new_capacity;
  return AGS_STATUS_OK;
}

static ags_status builder_append_bytes(
  string_builder *builder,
  const char *bytes,
  size_t length
) {
  ags_status status = builder_reserve(builder, length);

  if (status != AGS_STATUS_OK) {
    return status;
  }

  memcpy(builder->data + builder->length, bytes, length);
  builder->length += length;
  builder->data[builder->length] = '\0';
  return AGS_STATUS_OK;
}

static ags_status builder_append_char(string_builder *builder, char value) {
  return builder_append_bytes(builder, &value, 1);
}

static ags_status builder_append_field(string_builder *builder, const char *value) {
  size_t index = 0;
  ags_status status = builder_append_char(builder, '"');

  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (index = 0; value[index] != '\0'; ++index) {
    if (value[index] == '"') {
      status = builder_append_bytes(builder, "\"\"", 2);
    } else {
      status = builder_append_char(builder, value[index]);
    }

    if (status != AGS_STATUS_OK) {
      return status;
    }
  }

  return builder_append_char(builder, '"');
}

static ags_status builder_append_newline(
  string_builder *builder,
  ags_newline_mode newline_mode
) {
  if (newline_mode == AGS_NEWLINE_LF) {
    return builder_append_char(builder, '\n');
  }

  return builder_append_bytes(builder, "\r\n", 2);
}

static ags_status serialize_group(
  string_builder *builder,
  const ags_group_internal *group,
  ags_newline_mode newline_mode,
  int *needs_newline
) {
  size_t field_index = 0;
  size_t row_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (*needs_newline) {
    status = builder_append_newline(builder, newline_mode);
    if (status != AGS_STATUS_OK) {
      return status;
    }
  }

  status = builder_append_field(builder, "GROUP");
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = builder_append_char(builder, ',');
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = builder_append_field(builder, group->name);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = builder_append_newline(builder, newline_mode);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = builder_append_field(builder, "HEADING");
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    status = builder_append_char(builder, ',');
    if (status != AGS_STATUS_OK) {
      return status;
    }

    status = builder_append_field(builder, group->fields[field_index].name);
    if (status != AGS_STATUS_OK) {
      return status;
    }
  }

  status = builder_append_newline(builder, newline_mode);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = builder_append_field(builder, "UNIT");
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    status = builder_append_char(builder, ',');
    if (status != AGS_STATUS_OK) {
      return status;
    }

    status = builder_append_field(builder, group->fields[field_index].unit);
    if (status != AGS_STATUS_OK) {
      return status;
    }
  }

  status = builder_append_newline(builder, newline_mode);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = builder_append_field(builder, "TYPE");
  if (status != AGS_STATUS_OK) {
    return status;
  }

  for (field_index = 0; field_index < group->field_count; ++field_index) {
    status = builder_append_char(builder, ',');
    if (status != AGS_STATUS_OK) {
      return status;
    }

    status = builder_append_field(builder, group->fields[field_index].type);
    if (status != AGS_STATUS_OK) {
      return status;
    }
  }

  for (row_index = 0; row_index < group->row_count; ++row_index) {
    status = builder_append_newline(builder, newline_mode);
    if (status != AGS_STATUS_OK) {
      return status;
    }

    status = builder_append_field(builder, "DATA");
    if (status != AGS_STATUS_OK) {
      return status;
    }

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      status = builder_append_char(builder, ',');
      if (status != AGS_STATUS_OK) {
        return status;
      }

      status = builder_append_field(builder, group->rows[row_index].values[field_index]);
      if (status != AGS_STATUS_OK) {
        return status;
      }
    }
  }

  *needs_newline = 1;
  return AGS_STATUS_OK;
}

ags_status ags_document_serialize(
  const ags_document *document,
  const ags_serialize_options *options,
  char **out_buffer,
  size_t *out_length
) {
  string_builder builder;
  ags_serialize_options resolved_options;
  size_t group_index = 0;
  int needs_newline = 0;
  ags_status status = AGS_STATUS_OK;

  if (document == NULL || out_buffer == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = ags_serialize_options_init(&resolved_options);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  if (options != NULL) {
    if (options->struct_size != sizeof(*options)) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }

    resolved_options = *options;
  }

  memset(&builder, 0, sizeof(builder));
  builder.allocator = &document->allocator;
  *out_buffer = NULL;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    status = serialize_group(
      &builder,
      &document->groups[group_index],
      resolved_options.newline_mode,
      &needs_newline
    );
    if (status != AGS_STATUS_OK) {
      ags_dealloc(&document->allocator, builder.data);
      return status;
    }
  }

  status = builder_reserve(&builder, 0);
  if (status != AGS_STATUS_OK) {
    ags_dealloc(&document->allocator, builder.data);
    return status;
  }

  if (builder.data == NULL) {
    builder.data = ags_alloc(&document->allocator, 1);
    if (builder.data == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }

    builder.data[0] = '\0';
  }

  *out_buffer = builder.data;
  if (out_length != NULL) {
    *out_length = builder.length;
  }

  return AGS_STATUS_OK;
}
