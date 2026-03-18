#include <string.h>

#include "libags/dictionary.h"
#include "dictionary_bundle_data.h"
#include "dictionary_internal.h"

static const char *normalize_version(const char *version) {
  if (version == NULL || version[0] == '\0') {
    return "4.1.1";
  }

  if (strcmp(version, "4.0") == 0 || strcmp(version, "4.0.3") == 0) {
    return "4.0.3";
  }

  if (strcmp(version, "4.0.4") == 0) {
    return "4.0.4";
  }

  if (strcmp(version, "4.1") == 0) {
    return "4.1";
  }

  if (strcmp(version, "4.1.1") == 0) {
    return "4.1.1";
  }

  return NULL;
}

static const ags_group_internal *find_group_by_name(
  const ags_document *document,
  const char *group_name
) {
  size_t index = 0;

  if (document == NULL || group_name == NULL) {
    return NULL;
  }

  for (index = 0; index < document->group_count; ++index) {
    if (strcmp(document->groups[index].name, group_name) == 0) {
      return &document->groups[index];
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

static const char *document_trans_ags_version(const ags_document *document) {
  const ags_group_internal *tran_group = NULL;
  size_t field_index = 0;

  tran_group = find_group_by_name(document, "TRAN");
  if (tran_group == NULL || tran_group->row_count == 0) {
    return NULL;
  }

  field_index = find_field_index(tran_group, "TRAN_AGS");
  if (field_index == (size_t)-1) {
    return NULL;
  }

  return tran_group->rows[0].values[field_index];
}

const char *ags_dictionary_latest_version(void) {
  return "4.1.1";
}

int ags_dictionary_version_is_supported(const char *version) {
  return normalize_version(version) != NULL;
}

ags_status ags_dictionary_load_buffer(
  const char *input,
  size_t length,
  const ags_document_options *options,
  ags_document **out_document
) {
  return ags_document_parse_buffer(input, length, options, out_document);
}

ags_status ags_dictionary_load_file(
  const char *path,
  const ags_document_options *options,
  ags_document **out_document
) {
  return ags_document_parse_file(path, options, out_document);
}

ags_status ags_dictionary_resolve_version(
  const ags_document *document,
  const char *override_version,
  const char **out_version
) {
  const char *resolved = NULL;

  if (out_version == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (override_version != NULL && override_version[0] != '\0') {
    resolved = normalize_version(override_version);
    if (resolved == NULL) {
      return AGS_STATUS_NOT_FOUND;
    }

    *out_version = resolved;
    return AGS_STATUS_OK;
  }

  resolved = normalize_version(document_trans_ags_version(document));
  if (resolved != NULL) {
    *out_version = resolved;
    return AGS_STATUS_OK;
  }

  *out_version = ags_dictionary_latest_version();
  return AGS_STATUS_OK;
}

ags_status ags_dictionary_load_bundled(
  const char *version,
  const ags_document_options *options,
  ags_document **out_document
) {
  const char *resolved = NULL;
  const char *data = NULL;
  size_t length = 0;

  if (out_document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  resolved = normalize_version(version);
  if (resolved == NULL) {
    return AGS_STATUS_NOT_FOUND;
  }

  data = ags_bundled_dictionary_data(resolved, &length);
  if (data == NULL) {
    return AGS_STATUS_NOT_FOUND;
  }

  return ags_document_parse_buffer(data, length, options, out_document);
}

static ags_status dictionary_reserve_entries(
  ags_effective_dictionary *dictionary,
  size_t required
) {
  ags_dict_entry_view *entries = NULL;
  size_t new_capacity = 0;

  if (dictionary == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (required <= dictionary->entry_capacity) {
    return AGS_STATUS_OK;
  }

  new_capacity = dictionary->entry_capacity == 0 ? 16 : dictionary->entry_capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  entries = ags_realloc_buffer(
    &dictionary->allocator,
    dictionary->entries,
    new_capacity * sizeof(*entries)
  );
  if (entries == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(
    entries + dictionary->entry_capacity,
    0,
    (new_capacity - dictionary->entry_capacity) * sizeof(*entries)
  );
  dictionary->entries = entries;
  dictionary->entry_capacity = new_capacity;
  return AGS_STATUS_OK;
}

static const ags_dict_entry_view *find_entry_by_key(
  const ags_effective_dictionary *dictionary,
  const char *dict_type,
  const char *group_name,
  const char *heading_name,
  size_t *out_index
) {
  size_t index = 0;

  if (dictionary == NULL) {
    return NULL;
  }

  for (index = 0; index < dictionary->entry_count; ++index) {
    const ags_dict_entry_view *entry = &dictionary->entries[index];
    const char *entry_heading = entry->heading == NULL ? "" : entry->heading;
    const char *search_heading = heading_name == NULL ? "" : heading_name;

    if (strcmp(entry->dict_type, dict_type) == 0 &&
        strcmp(entry->group, group_name) == 0 &&
        strcmp(entry_heading, search_heading) == 0) {
      if (out_index != NULL) {
        *out_index = index;
      }
      return entry;
    }
  }

  return NULL;
}

static ags_status append_dict_entries_from_document(
  ags_effective_dictionary *dictionary,
  const ags_document *source_document,
  int from_custom
) {
  const ags_group_internal *dict_group = NULL;
  size_t type_idx = 0;
  size_t grp_idx = 0;
  size_t hdng_idx = 0;
  size_t stat_idx = 0;
  size_t dtyp_idx = 0;
  size_t unit_idx = 0;
  size_t pgrp_idx = 0;
  size_t row_index = 0;

  if (dictionary == NULL || source_document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  dict_group = find_group_by_name(source_document, "DICT");
  if (dict_group == NULL) {
    return AGS_STATUS_OK;
  }

  type_idx = find_field_index(dict_group, "DICT_TYPE");
  grp_idx = find_field_index(dict_group, "DICT_GRP");
  hdng_idx = find_field_index(dict_group, "DICT_HDNG");
  stat_idx = find_field_index(dict_group, "DICT_STAT");
  dtyp_idx = find_field_index(dict_group, "DICT_DTYP");
  unit_idx = find_field_index(dict_group, "DICT_UNIT");
  pgrp_idx = find_field_index(dict_group, "DICT_PGRP");

  if (type_idx == (size_t)-1 || grp_idx == (size_t)-1) {
    return AGS_STATUS_OK;
  }

  for (row_index = 0; row_index < dict_group->row_count; ++row_index) {
    ags_dict_entry_view entry;
    size_t existing_index = 0;
    const char *dict_type = dict_group->rows[row_index].values[type_idx];
    const char *group_name = dict_group->rows[row_index].values[grp_idx];
    const char *heading_name = hdng_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[hdng_idx];

    if (strcmp(dict_type, "GROUP") != 0 && strcmp(dict_type, "HEADING") != 0) {
      continue;
    }

    if (find_entry_by_key(dictionary, dict_type, group_name, heading_name, &existing_index) != NULL) {
      if (from_custom) {
        ags_dict_entry_view *existing = &dictionary->entries[existing_index];
        existing->stat = stat_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[stat_idx];
        existing->dtype = dtyp_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[dtyp_idx];
        existing->unit = unit_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[unit_idx];
        existing->pgrp = pgrp_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[pgrp_idx];
        existing->from_custom = 1;
      }
      continue;
    }

    memset(&entry, 0, sizeof(entry));
    entry.dict_type = dict_type;
    entry.group = group_name;
    entry.heading = hdng_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[hdng_idx];
    entry.stat = stat_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[stat_idx];
    entry.dtype = dtyp_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[dtyp_idx];
    entry.unit = unit_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[unit_idx];
    entry.pgrp = pgrp_idx == (size_t)-1 ? "" : dict_group->rows[row_index].values[pgrp_idx];
    entry.order_index = dictionary->entry_count;
    entry.from_custom = from_custom;

    if (dictionary_reserve_entries(dictionary, dictionary->entry_count + 1) != AGS_STATUS_OK) {
      return AGS_STATUS_NO_MEMORY;
    }

    dictionary->entries[dictionary->entry_count++] = entry;
  }

  return AGS_STATUS_OK;
}

ags_status ags_effective_dictionary_append_document(
  ags_effective_dictionary *dictionary,
  const ags_document *source_document,
  int from_custom
) {
  return append_dict_entries_from_document(dictionary, source_document, from_custom);
}

ags_status ags_effective_dictionary_build(
  ags_effective_dictionary *dictionary,
  const ags_allocator *allocator,
  const ags_document *standard_dictionary,
  const ags_document *file_document
) {
  ags_status status = AGS_STATUS_OK;

  if (dictionary == NULL || allocator == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(dictionary, 0, sizeof(*dictionary));
  memcpy(&dictionary->allocator, allocator, sizeof(dictionary->allocator));

  status = append_dict_entries_from_document(dictionary, standard_dictionary, 0);
  if (status != AGS_STATUS_OK) {
    ags_effective_dictionary_destroy(dictionary);
    return status;
  }

  status = append_dict_entries_from_document(dictionary, file_document, 1);
  if (status != AGS_STATUS_OK) {
    ags_effective_dictionary_destroy(dictionary);
    return status;
  }

  return AGS_STATUS_OK;
}

void ags_effective_dictionary_destroy(ags_effective_dictionary *dictionary) {
  if (dictionary == NULL) {
    return;
  }

  ags_dealloc(&dictionary->allocator, dictionary->entries);
  memset(dictionary, 0, sizeof(*dictionary));
}

const ags_dict_entry_view *ags_effective_dictionary_find_heading(
  const ags_effective_dictionary *dictionary,
  const char *group_name,
  const char *heading_name
) {
  return find_entry_by_key(dictionary, "HEADING", group_name, heading_name, NULL);
}

const ags_dict_entry_view *ags_effective_dictionary_find_group(
  const ags_effective_dictionary *dictionary,
  const char *group_name
) {
  return find_entry_by_key(dictionary, "GROUP", group_name, "", NULL);
}

int ags_dict_stat_contains(const ags_dict_entry_view *entry, const char *token) {
  if (entry == NULL || token == NULL || entry->stat == NULL) {
    return 0;
  }

  return strstr(entry->stat, token) != NULL;
}
