#ifndef LIBAGS_DOCUMENT_H
#define LIBAGS_DOCUMENT_H

#include <stddef.h>

#include "libags/allocator.h"
#include "libags/export.h"
#include "libags/status.h"

LIBAGS_EXTERN_C_BEGIN

typedef struct ags_document ags_document;

typedef enum ags_newline_mode {
  AGS_NEWLINE_CRLF = 0,
  AGS_NEWLINE_LF = 1
} ags_newline_mode;

typedef struct ags_document_options {
  size_t struct_size;
  const ags_allocator *allocator;
} ags_document_options;

typedef struct ags_serialize_options {
  size_t struct_size;
  ags_newline_mode newline_mode;
} ags_serialize_options;

LIBAGS_API ags_status ags_document_options_init(ags_document_options *options);
LIBAGS_API ags_status ags_serialize_options_init(ags_serialize_options *options);
LIBAGS_API ags_status ags_document_create(
  const ags_document_options *options,
  ags_document **out_document
);
LIBAGS_API ags_status ags_document_parse_buffer(
  const char *input,
  size_t length,
  const ags_document_options *options,
  ags_document **out_document
);
LIBAGS_API ags_status ags_document_parse_file(
  const char *path,
  const ags_document_options *options,
  ags_document **out_document
);
LIBAGS_API void ags_document_destroy(ags_document *document);
LIBAGS_API void ags_document_free_buffer(const ags_document *document, void *buffer);
LIBAGS_API size_t ags_document_group_count(const ags_document *document);
LIBAGS_API const ags_allocator *ags_document_allocator(const ags_document *document);
LIBAGS_API ags_status ags_document_find_group(
  const ags_document *document,
  const char *group_name,
  size_t *out_index
);
LIBAGS_API const char *ags_document_group_name(
  const ags_document *document,
  size_t group_index
);
LIBAGS_API size_t ags_document_group_line_number(
  const ags_document *document,
  size_t group_index
);
LIBAGS_API size_t ags_document_group_heading_line_number(
  const ags_document *document,
  size_t group_index
);
LIBAGS_API size_t ags_document_group_unit_line_number(
  const ags_document *document,
  size_t group_index
);
LIBAGS_API size_t ags_document_group_type_line_number(
  const ags_document *document,
  size_t group_index
);
LIBAGS_API size_t ags_document_group_field_count(
  const ags_document *document,
  size_t group_index
);
LIBAGS_API size_t ags_document_group_row_count(
  const ags_document *document,
  size_t group_index
);
LIBAGS_API const char *ags_document_field_name(
  const ags_document *document,
  size_t group_index,
  size_t field_index
);
LIBAGS_API const char *ags_document_field_unit(
  const ags_document *document,
  size_t group_index,
  size_t field_index
);
LIBAGS_API const char *ags_document_field_type(
  const ags_document *document,
  size_t group_index,
  size_t field_index
);
LIBAGS_API size_t ags_document_row_line_number(
  const ags_document *document,
  size_t group_index,
  size_t row_index
);
LIBAGS_API const char *ags_document_cell_value(
  const ags_document *document,
  size_t group_index,
  size_t row_index,
  size_t field_index
);
LIBAGS_API ags_status ags_document_serialize(
  const ags_document *document,
  const ags_serialize_options *options,
  char **out_buffer,
  size_t *out_length
);

LIBAGS_EXTERN_C_END

#endif
