#ifndef LIBAGS_DOCUMENT_INTERNAL_H
#define LIBAGS_DOCUMENT_INTERNAL_H

#include <stddef.h>

#include "libags/document.h"

typedef struct ags_field_internal {
  char *name;
  char *unit;
  char *type;
} ags_field_internal;

typedef struct ags_row_internal {
  size_t line_number;
  char **values;
} ags_row_internal;

typedef struct ags_group_internal {
  char *name;
  size_t group_line_number;
  size_t heading_line_number;
  size_t unit_line_number;
  size_t type_line_number;
  size_t field_count;
  ags_field_internal *fields;
  size_t row_count;
  size_t row_capacity;
  ags_row_internal *rows;
} ags_group_internal;

struct ags_document {
  ags_allocator allocator;
  size_t group_count;
  size_t group_capacity;
  ags_group_internal *groups;
};

ags_status ags_document_pick_allocator(
  const ags_document_options *options,
  ags_allocator *out_allocator
);
void *ags_alloc(const ags_allocator *allocator, size_t size);
void *ags_realloc_buffer(const ags_allocator *allocator, void *ptr, size_t size);
void ags_dealloc(const ags_allocator *allocator, void *ptr);
char *ags_strndup_alloc(const ags_allocator *allocator, const char *src, size_t length);
void ags_document_reset(ags_document *document);
ags_status ags_document_reserve_groups(ags_document *document, size_t required);
ags_status ags_group_reserve_rows(
  ags_document *document,
  ags_group_internal *group,
  size_t required
);
ags_group_internal *ags_document_get_group_mutable(ags_document *document, size_t group_index);
const ags_group_internal *ags_document_get_group(const ags_document *document, size_t group_index);
size_t ags_document_find_group_index(const ags_document *document, const char *group_name);

#endif
