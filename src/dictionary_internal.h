#ifndef LIBAGS_DICTIONARY_INTERNAL_H
#define LIBAGS_DICTIONARY_INTERNAL_H

#include <stddef.h>

#include "libags/dictionary.h"
#include "document_internal.h"

typedef struct ags_dict_entry_view {
  const char *dict_type;
  const char *group;
  const char *heading;
  const char *stat;
  const char *dtype;
  const char *unit;
  const char *pgrp;
  size_t order_index;
  int from_custom;
} ags_dict_entry_view;

typedef struct ags_effective_dictionary {
  ags_allocator allocator;
  ags_dict_entry_view *entries;
  size_t entry_count;
  size_t entry_capacity;
} ags_effective_dictionary;

ags_status ags_effective_dictionary_build(
  ags_effective_dictionary *dictionary,
  const ags_allocator *allocator,
  const ags_document *standard_dictionary,
  const ags_document *file_document
);
ags_status ags_effective_dictionary_append_document(
  ags_effective_dictionary *dictionary,
  const ags_document *source_document,
  int from_custom
);
void ags_effective_dictionary_destroy(ags_effective_dictionary *dictionary);
const ags_dict_entry_view *ags_effective_dictionary_find_heading(
  const ags_effective_dictionary *dictionary,
  const char *group_name,
  const char *heading_name
);
const ags_dict_entry_view *ags_effective_dictionary_find_group(
  const ags_effective_dictionary *dictionary,
  const char *group_name
);
int ags_dict_stat_contains(const ags_dict_entry_view *entry, const char *token);

#endif
