#include <stdio.h>
#include <string.h>

#include "libags/document.h"
#include "document_internal.h"

typedef struct parsed_fields {
  char **values;
  size_t count;
  size_t capacity;
} parsed_fields;

static void parsed_fields_reset(const ags_allocator *allocator, parsed_fields *fields) {
  size_t index = 0;

  if (allocator == NULL || fields == NULL) {
    return;
  }

  for (index = 0; index < fields->count; ++index) {
    ags_dealloc(allocator, fields->values[index]);
  }

  ags_dealloc(allocator, fields->values);
  memset(fields, 0, sizeof(*fields));
}

static ags_status parsed_fields_push(
  const ags_allocator *allocator,
  parsed_fields *fields,
  char *value
) {
  char **values = NULL;
  size_t new_capacity = 0;

  if (allocator == NULL || fields == NULL || value == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (fields->count == fields->capacity) {
    new_capacity = fields->capacity == 0 ? 4 : fields->capacity * 2;
    values = ags_realloc_buffer(allocator, fields->values, new_capacity * sizeof(*values));
    if (values == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }

    fields->values = values;
    fields->capacity = new_capacity;
  }

  fields->values[fields->count++] = value;
  return AGS_STATUS_OK;
}

static int is_blank_line(const char *line, size_t length) {
  size_t index = 0;

  for (index = 0; index < length; ++index) {
    if (line[index] != ' ' && line[index] != '\t') {
      return 0;
    }
  }

  return 1;
}

static ags_status tokenize_line(
  const ags_allocator *allocator,
  const char *line,
  size_t length,
  parsed_fields *out_fields
) {
  size_t index = 0;
  ags_status status = AGS_STATUS_OK;

  if (allocator == NULL || line == NULL || out_fields == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(out_fields, 0, sizeof(*out_fields));

  while (index < length) {
    char *value = NULL;
    size_t value_length = 0;
    size_t write_index = 0;
    int closed = 0;

    if (line[index] != '"') {
      status = AGS_STATUS_PARSE_ERROR;
      goto cleanup;
    }

    value = ags_alloc(allocator, length + 1);
    if (value == NULL) {
      status = AGS_STATUS_NO_MEMORY;
      goto cleanup;
    }

    index += 1;
    while (index < length) {
      if (line[index] == '"') {
        if ((index + 1) < length && line[index + 1] == '"') {
          value[write_index++] = '"';
          index += 2;
          continue;
        }

        closed = 1;
        index += 1;
        break;
      }

      value[write_index++] = line[index++];
    }

    if (!closed) {
      ags_dealloc(allocator, value);
      status = AGS_STATUS_PARSE_ERROR;
      goto cleanup;
    }

    value_length = write_index;
    value[value_length] = '\0';

    status = parsed_fields_push(allocator, out_fields, value);
    if (status != AGS_STATUS_OK) {
      ags_dealloc(allocator, value);
      goto cleanup;
    }

    if (index == length) {
      break;
    }

    if (line[index] != ',') {
      status = AGS_STATUS_PARSE_ERROR;
      goto cleanup;
    }

    index += 1;
    if (index == length) {
      status = AGS_STATUS_PARSE_ERROR;
      goto cleanup;
    }
  }

  return AGS_STATUS_OK;

cleanup:
  parsed_fields_reset(allocator, out_fields);
  return status;
}

static ags_status ags_group_set_heading_names(
  ags_document *document,
  ags_group_internal *group,
  const parsed_fields *fields,
  size_t line_number
) {
  size_t index = 0;

  group->field_count = fields->count - 1;
  group->heading_line_number = line_number;
  group->fields = ags_alloc(&document->allocator, group->field_count * sizeof(*group->fields));
  if (group->fields == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(group->fields, 0, group->field_count * sizeof(*group->fields));

  for (index = 0; index < group->field_count; ++index) {
    group->fields[index].name = ags_strndup_alloc(
      &document->allocator,
      fields->values[index + 1],
      strlen(fields->values[index + 1])
    );
    if (group->fields[index].name == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  return AGS_STATUS_OK;
}

static ags_status ags_group_set_units_or_types(
  ags_document *document,
  ags_group_internal *group,
  const parsed_fields *fields,
  size_t line_number,
  int store_units
) {
  size_t index = 0;

  if ((fields->count - 1) != group->field_count) {
    return AGS_STATUS_PARSE_ERROR;
  }

  if (store_units) {
    group->unit_line_number = line_number;
  } else {
    group->type_line_number = line_number;
  }

  for (index = 0; index < group->field_count; ++index) {
    char **slot = store_units ? &group->fields[index].unit : &group->fields[index].type;

    *slot = ags_strndup_alloc(
      &document->allocator,
      fields->values[index + 1],
      strlen(fields->values[index + 1])
    );
    if (*slot == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  return AGS_STATUS_OK;
}

static ags_status ags_group_add_row(
  ags_document *document,
  ags_group_internal *group,
  const parsed_fields *fields,
  size_t line_number
) {
  size_t index = 0;
  ags_row_internal *row = NULL;
  ags_status status = AGS_STATUS_OK;

  if ((fields->count - 1) != group->field_count) {
    return AGS_STATUS_PARSE_ERROR;
  }

  status = ags_group_reserve_rows(document, group, group->row_count + 1);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  row = &group->rows[group->row_count];
  memset(row, 0, sizeof(*row));
  group->row_count += 1;
  row->values = ags_alloc(&document->allocator, group->field_count * sizeof(*row->values));
  if (row->values == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(row->values, 0, group->field_count * sizeof(*row->values));
  row->line_number = line_number;

  for (index = 0; index < group->field_count; ++index) {
    row->values[index] = ags_strndup_alloc(
      &document->allocator,
      fields->values[index + 1],
      strlen(fields->values[index + 1])
    );
    if (row->values[index] == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }
  }

  return AGS_STATUS_OK;
}

static ags_status parse_into_document(
  ags_document *document,
  const char *input,
  size_t length
) {
  enum parser_phase {
    PARSER_EXPECT_GROUP = 0,
    PARSER_EXPECT_HEADING = 1,
    PARSER_EXPECT_UNIT = 2,
    PARSER_EXPECT_TYPE = 3,
    PARSER_EXPECT_DATA_OR_GROUP = 4
  };

  enum parser_phase phase = PARSER_EXPECT_GROUP;
  size_t position = 0;
  size_t line_number = 1;
  size_t current_group_index = (size_t)-1;
  ags_status status = AGS_STATUS_OK;

  if (document == NULL || input == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (length >= 3 &&
      (unsigned char)input[0] == 0xEF &&
      (unsigned char)input[1] == 0xBB &&
      (unsigned char)input[2] == 0xBF) {
    position = 3;
  }

  while (position <= length) {
    size_t line_start = position;
    size_t line_end = position;
    parsed_fields fields;
    const char *descriptor = NULL;
    ags_group_internal *group = NULL;

    while (line_end < length && input[line_end] != '\n' && input[line_end] != '\r') {
      line_end += 1;
    }

    position = line_end;
    if (position < length) {
      if (input[position] == '\r' && (position + 1) < length && input[position + 1] == '\n') {
        position += 2;
      } else {
        position += 1;
      }
    } else {
      position += 1;
    }

    if (line_end == line_start || is_blank_line(input + line_start, line_end - line_start)) {
      line_number += 1;
      continue;
    }

    status = tokenize_line(
      &document->allocator,
      input + line_start,
      line_end - line_start,
      &fields
    );
    if (status != AGS_STATUS_OK) {
      return status;
    }

    if (fields.count == 0) {
      parsed_fields_reset(&document->allocator, &fields);
      return AGS_STATUS_PARSE_ERROR;
    }

    descriptor = fields.values[0];

    if (strcmp(descriptor, "GROUP") == 0) {
      ags_group_internal *new_group = NULL;
      char *group_name = NULL;

      if (phase != PARSER_EXPECT_GROUP && phase != PARSER_EXPECT_DATA_OR_GROUP) {
        parsed_fields_reset(&document->allocator, &fields);
        return AGS_STATUS_PARSE_ERROR;
      }

      if (fields.count != 2) {
        parsed_fields_reset(&document->allocator, &fields);
        return AGS_STATUS_PARSE_ERROR;
      }

      if (ags_document_find_group_index(document, fields.values[1]) != (size_t)-1) {
        parsed_fields_reset(&document->allocator, &fields);
        return AGS_STATUS_PARSE_ERROR;
      }

      status = ags_document_reserve_groups(document, document->group_count + 1);
      if (status != AGS_STATUS_OK) {
        parsed_fields_reset(&document->allocator, &fields);
        return status;
      }

      new_group = &document->groups[document->group_count];
      memset(new_group, 0, sizeof(*new_group));

      group_name = ags_strndup_alloc(
        &document->allocator,
        fields.values[1],
        strlen(fields.values[1])
      );
      if (group_name == NULL) {
        parsed_fields_reset(&document->allocator, &fields);
        return AGS_STATUS_NO_MEMORY;
      }

      new_group->name = group_name;
      new_group->group_line_number = line_number;

      current_group_index = document->group_count;
      document->group_count += 1;
      phase = PARSER_EXPECT_HEADING;
    } else {
      group = ags_document_get_group_mutable(document, current_group_index);
      if (group == NULL) {
        parsed_fields_reset(&document->allocator, &fields);
        return AGS_STATUS_PARSE_ERROR;
      }

      if (strcmp(descriptor, "HEADING") == 0) {
        if (phase != PARSER_EXPECT_HEADING || fields.count < 2) {
          parsed_fields_reset(&document->allocator, &fields);
          return AGS_STATUS_PARSE_ERROR;
        }

        status = ags_group_set_heading_names(document, group, &fields, line_number);
        if (status != AGS_STATUS_OK) {
          parsed_fields_reset(&document->allocator, &fields);
          return status;
        }

        phase = PARSER_EXPECT_UNIT;
      } else if (strcmp(descriptor, "UNIT") == 0) {
        if (phase != PARSER_EXPECT_UNIT) {
          parsed_fields_reset(&document->allocator, &fields);
          return AGS_STATUS_PARSE_ERROR;
        }

        status = ags_group_set_units_or_types(document, group, &fields, line_number, 1);
        if (status != AGS_STATUS_OK) {
          parsed_fields_reset(&document->allocator, &fields);
          return status;
        }

        phase = PARSER_EXPECT_TYPE;
      } else if (strcmp(descriptor, "TYPE") == 0) {
        if (phase != PARSER_EXPECT_TYPE) {
          parsed_fields_reset(&document->allocator, &fields);
          return AGS_STATUS_PARSE_ERROR;
        }

        status = ags_group_set_units_or_types(document, group, &fields, line_number, 0);
        if (status != AGS_STATUS_OK) {
          parsed_fields_reset(&document->allocator, &fields);
          return status;
        }

        phase = PARSER_EXPECT_DATA_OR_GROUP;
      } else if (strcmp(descriptor, "DATA") == 0) {
        if (phase != PARSER_EXPECT_DATA_OR_GROUP) {
          parsed_fields_reset(&document->allocator, &fields);
          return AGS_STATUS_PARSE_ERROR;
        }

        status = ags_group_add_row(document, group, &fields, line_number);
        if (status != AGS_STATUS_OK) {
          parsed_fields_reset(&document->allocator, &fields);
          return status;
        }
      } else {
        parsed_fields_reset(&document->allocator, &fields);
        return AGS_STATUS_PARSE_ERROR;
      }
    }

    parsed_fields_reset(&document->allocator, &fields);
    line_number += 1;
  }

  if (phase == PARSER_EXPECT_HEADING || phase == PARSER_EXPECT_UNIT || phase == PARSER_EXPECT_TYPE) {
    return AGS_STATUS_PARSE_ERROR;
  }

  return AGS_STATUS_OK;
}

ags_status ags_document_parse_buffer(
  const char *input,
  size_t length,
  const ags_document_options *options,
  ags_document **out_document
) {
  ags_document *document = NULL;
  ags_status status = AGS_STATUS_OK;

  if (input == NULL || out_document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_document = NULL;

  status = ags_document_create(options, &document);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = parse_into_document(document, input, length);
  if (status != AGS_STATUS_OK) {
    ags_document_destroy(document);
    return status;
  }

  *out_document = document;
  return AGS_STATUS_OK;
}

ags_status ags_document_parse_file(
  const char *path,
  const ags_document_options *options,
  ags_document **out_document
) {
  ags_document *document = NULL;
  FILE *file = NULL;
  long file_size = 0;
  char *buffer = NULL;
  size_t bytes_read = 0;
  ags_status status = AGS_STATUS_OK;

  if (path == NULL || out_document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_document = NULL;

  status = ags_document_create(options, &document);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    ags_document_destroy(document);
    return AGS_STATUS_IO_ERROR;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    ags_document_destroy(document);
    return AGS_STATUS_IO_ERROR;
  }

  file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    ags_document_destroy(document);
    return AGS_STATUS_IO_ERROR;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    ags_document_destroy(document);
    return AGS_STATUS_IO_ERROR;
  }

  buffer = ags_alloc(&document->allocator, (size_t)file_size);
  if (buffer == NULL && file_size > 0) {
    fclose(file);
    ags_document_destroy(document);
    return AGS_STATUS_NO_MEMORY;
  }

  bytes_read = fread(buffer, 1, (size_t)file_size, file);
  fclose(file);

  if (bytes_read != (size_t)file_size) {
    ags_dealloc(&document->allocator, buffer);
    ags_document_destroy(document);
    return AGS_STATUS_IO_ERROR;
  }

  status = parse_into_document(document, buffer, (size_t)file_size);
  ags_dealloc(&document->allocator, buffer);
  if (status != AGS_STATUS_OK) {
    ags_document_destroy(document);
    return status;
  }

  *out_document = document;
  return AGS_STATUS_OK;
}
