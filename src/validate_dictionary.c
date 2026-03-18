#include <string.h>

#include "libags/validate.h"
#include "dictionary_internal.h"

typedef struct ags_diagnostic_internal {
  char *rule;
  char *message;
  char *group;
  char *field;
  size_t line_number;
  ags_diagnostic_severity severity;
} ags_diagnostic_internal;

struct ags_validation_report {
  ags_allocator allocator;
  ags_diagnostic_internal *diagnostics;
  size_t diagnostic_count;
  size_t diagnostic_capacity;
};

static void diagnostic_clear(ags_validation_report *report, ags_diagnostic_internal *diagnostic) {
  if (report == NULL || diagnostic == NULL) {
    return;
  }

  ags_dealloc(&report->allocator, diagnostic->rule);
  ags_dealloc(&report->allocator, diagnostic->message);
  ags_dealloc(&report->allocator, diagnostic->group);
  ags_dealloc(&report->allocator, diagnostic->field);
  memset(diagnostic, 0, sizeof(*diagnostic));
}

static ags_status report_add_diagnostic(
  ags_validation_report *report,
  const char *rule,
  ags_diagnostic_severity severity,
  size_t line_number,
  const char *group,
  const char *field,
  const char *message
) {
  ags_diagnostic_internal *diagnostics = NULL;
  ags_diagnostic_internal *diagnostic = NULL;
  size_t new_capacity = 0;

  if (report == NULL || rule == NULL || message == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (report->diagnostic_count == report->diagnostic_capacity) {
    new_capacity = report->diagnostic_capacity == 0 ? 8 : report->diagnostic_capacity * 2;
    diagnostics = ags_realloc_buffer(
      &report->allocator,
      report->diagnostics,
      new_capacity * sizeof(*diagnostics)
    );
    if (diagnostics == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }

    memset(
      diagnostics + report->diagnostic_capacity,
      0,
      (new_capacity - report->diagnostic_capacity) * sizeof(*diagnostics)
    );
    report->diagnostics = diagnostics;
    report->diagnostic_capacity = new_capacity;
  }

  diagnostic = &report->diagnostics[report->diagnostic_count];
  memset(diagnostic, 0, sizeof(*diagnostic));
  diagnostic->rule = ags_strndup_alloc(&report->allocator, rule, strlen(rule));
  diagnostic->message = ags_strndup_alloc(&report->allocator, message, strlen(message));
  if (diagnostic->rule == NULL || diagnostic->message == NULL) {
    diagnostic_clear(report, diagnostic);
    return AGS_STATUS_NO_MEMORY;
  }

  if (group != NULL) {
    diagnostic->group = ags_strndup_alloc(&report->allocator, group, strlen(group));
    if (diagnostic->group == NULL) {
      diagnostic_clear(report, diagnostic);
      return AGS_STATUS_NO_MEMORY;
    }
  }

  if (field != NULL) {
    diagnostic->field = ags_strndup_alloc(&report->allocator, field, strlen(field));
    if (diagnostic->field == NULL) {
      diagnostic_clear(report, diagnostic);
      return AGS_STATUS_NO_MEMORY;
    }
  }

  diagnostic->severity = severity;
  diagnostic->line_number = line_number;
  report->diagnostic_count += 1;
  return AGS_STATUS_OK;
}

static const ags_group_internal *find_group(const ags_document *document, const char *group_name) {
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

static const char *tran_value_or_default(
  const ags_document *document,
  const char *field_name,
  const char *default_value
) {
  const ags_group_internal *tran_group = find_group(document, "TRAN");
  size_t field_index = 0;

  if (tran_group == NULL || tran_group->row_count == 0) {
    return default_value;
  }

  field_index = find_field_index(tran_group, field_name);
  if (field_index == (size_t)-1) {
    return default_value;
  }

  if (tran_group->rows[0].values[field_index][0] == '\0') {
    return default_value;
  }

  return tran_group->rows[0].values[field_index];
}

static ags_status add_required_group_diagnostics(
  const ags_document *document,
  ags_validation_report *report
) {
  const ags_group_internal *proj = find_group(document, "PROJ");
  const ags_group_internal *tran = find_group(document, "TRAN");
  const ags_group_internal *type = find_group(document, "TYPE");
  const ags_group_internal *unit = find_group(document, "UNIT");
  ags_status status = AGS_STATUS_OK;

  if (proj == NULL) {
    status = report_add_diagnostic(report, "13", AGS_DIAGNOSTIC_ERROR, 0, "PROJ", NULL, "PROJ group is required.");
  } else if (proj->row_count != 1) {
    status = report_add_diagnostic(report, "13", AGS_DIAGNOSTIC_ERROR, proj->group_line_number, "PROJ", NULL, "PROJ group must contain exactly one row.");
  }
  if (status != AGS_STATUS_OK) {
    return status;
  }

  if (tran == NULL) {
    status = report_add_diagnostic(report, "14", AGS_DIAGNOSTIC_ERROR, 0, "TRAN", NULL, "TRAN group is required.");
  } else if (tran->row_count != 1) {
    status = report_add_diagnostic(report, "14", AGS_DIAGNOSTIC_ERROR, tran->group_line_number, "TRAN", NULL, "TRAN group must contain exactly one row.");
  }
  if (status != AGS_STATUS_OK) {
    return status;
  }

  if (type == NULL) {
    status = report_add_diagnostic(report, "17", AGS_DIAGNOSTIC_ERROR, 0, "TYPE", NULL, "TYPE group is required.");
  }
  if (status != AGS_STATUS_OK) {
    return status;
  }

  if (unit == NULL) {
    status = report_add_diagnostic(report, "15", AGS_DIAGNOSTIC_ERROR, 0, "UNIT", NULL, "UNIT group is required.");
  }
  return status;
}

static ags_status validate_heading_existence_and_order(
  const ags_document *document,
  const ags_effective_dictionary *dictionary,
  ags_validation_report *report
) {
  size_t group_index = 0;
  ags_status status = AGS_STATUS_OK;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    size_t field_index = 0;
    size_t previous_order = 0;
    int have_previous = 0;

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      const ags_dict_entry_view *entry = ags_effective_dictionary_find_heading(
        dictionary,
        group->name,
        group->fields[field_index].name
      );

      if (entry == NULL) {
        status = report_add_diagnostic(
          report,
          "9",
          AGS_DIAGNOSTIC_ERROR,
          group->heading_line_number,
          group->name,
          group->fields[field_index].name,
          "Heading is not present in the effective dictionary."
        );
        if (status != AGS_STATUS_OK) {
          return status;
        }
        continue;
      }

      if (have_previous && entry->order_index < previous_order) {
        status = report_add_diagnostic(
          report,
          "9",
          AGS_DIAGNOSTIC_ERROR,
          group->heading_line_number,
          group->name,
          group->fields[field_index].name,
          "Heading order does not match the effective dictionary."
        );
        if (status != AGS_STATUS_OK) {
          return status;
        }
      }

      previous_order = entry->order_index;
      have_previous = 1;
    }
  }

  return AGS_STATUS_OK;
}

static ags_status validate_required_fields(
  const ags_document *document,
  const ags_effective_dictionary *dictionary,
  ags_validation_report *report
) {
  size_t group_index = 0;
  ags_status status = AGS_STATUS_OK;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    size_t entry_index = 0;

    for (entry_index = 0; entry_index < dictionary->entry_count; ++entry_index) {
      const ags_dict_entry_view *entry = &dictionary->entries[entry_index];
      size_t field_index = 0;
      size_t row_index = 0;

      if (strcmp(entry->dict_type, "HEADING") != 0 ||
          strcmp(entry->group, group->name) != 0 ||
          !ags_dict_stat_contains(entry, "REQUIRED")) {
        continue;
      }

      field_index = find_field_index(group, entry->heading);
      if (field_index == (size_t)-1) {
        status = report_add_diagnostic(
          report,
          "10b",
          AGS_DIAGNOSTIC_ERROR,
          group->heading_line_number,
          group->name,
          entry->heading,
          "Required heading is missing from the group."
        );
        if (status != AGS_STATUS_OK) {
          return status;
        }
        continue;
      }

      for (row_index = 0; row_index < group->row_count; ++row_index) {
        if (group->rows[row_index].values[field_index][0] == '\0') {
          status = report_add_diagnostic(
            report,
            "10b",
            AGS_DIAGNOSTIC_ERROR,
            group->rows[row_index].line_number,
            group->name,
            entry->heading,
            "Required field is empty."
          );
          if (status != AGS_STATUS_OK) {
            return status;
          }
        }
      }
    }
  }

  return AGS_STATUS_OK;
}

static size_t collect_key_field_indices(
  const ags_group_internal *group,
  const ags_effective_dictionary *dictionary,
  const char *group_name,
  size_t **out_indices,
  const ags_allocator *allocator
) {
  size_t *indices = NULL;
  size_t count = 0;
  size_t entry_index = 0;

  if (out_indices == NULL || allocator == NULL) {
    return 0;
  }

  *out_indices = NULL;

  for (entry_index = 0; entry_index < dictionary->entry_count; ++entry_index) {
    const ags_dict_entry_view *entry = &dictionary->entries[entry_index];
    size_t field_index = 0;
    size_t *new_indices = NULL;

    if (strcmp(entry->dict_type, "HEADING") != 0 ||
        strcmp(entry->group, group_name) != 0 ||
        !ags_dict_stat_contains(entry, "KEY")) {
      continue;
    }

    field_index = find_field_index(group, entry->heading);
    if (field_index == (size_t)-1) {
      continue;
    }

    new_indices = ags_realloc_buffer(allocator, indices, (count + 1) * sizeof(*indices));
    if (new_indices == NULL) {
      ags_dealloc(allocator, indices);
      return 0;
    }

    indices = new_indices;
    indices[count++] = field_index;
  }

  *out_indices = indices;
  return count;
}

static int rows_match_on_fields(
  const ags_group_internal *left_group,
  size_t left_row_index,
  const ags_group_internal *right_group,
  size_t right_row_index,
  const size_t *field_indices,
  size_t field_count
) {
  size_t index = 0;

  for (index = 0; index < field_count; ++index) {
    size_t field_index = field_indices[index];
    if (strcmp(
          left_group->rows[left_row_index].values[field_index],
          right_group->rows[right_row_index].values[field_index]) != 0) {
      return 0;
    }
  }

  return 1;
}

static ags_status validate_key_uniqueness(
  const ags_document *document,
  const ags_effective_dictionary *dictionary,
  ags_validation_report *report
) {
  size_t group_index = 0;
  ags_status status = AGS_STATUS_OK;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    size_t *key_indices = NULL;
    size_t key_count = collect_key_field_indices(group, dictionary, group->name, &key_indices, &report->allocator);
    size_t row_index = 0;
    size_t other_row_index = 0;

    if (key_count == 0) {
      continue;
    }

    for (row_index = 0; row_index < group->row_count; ++row_index) {
      for (other_row_index = row_index + 1; other_row_index < group->row_count; ++other_row_index) {
        if (rows_match_on_fields(group, row_index, group, other_row_index, key_indices, key_count)) {
          status = report_add_diagnostic(
            report,
            "10a",
            AGS_DIAGNOSTIC_ERROR,
            group->rows[other_row_index].line_number,
            group->name,
            NULL,
            "Duplicate key values found in group."
          );
          if (status != AGS_STATUS_OK) {
            ags_dealloc(&report->allocator, key_indices);
            return status;
          }
        }
      }
    }

    ags_dealloc(&report->allocator, key_indices);
  }

  return AGS_STATUS_OK;
}

static ags_status validate_parent_child_links(
  const ags_document *document,
  const ags_effective_dictionary *dictionary,
  ags_validation_report *report
) {
  size_t group_index = 0;
  ags_status status = AGS_STATUS_OK;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    const ags_dict_entry_view *group_entry = ags_effective_dictionary_find_group(dictionary, group->name);
    const ags_group_internal *parent_group = NULL;
    size_t *parent_key_indices = NULL;
    size_t row_index = 0;
    size_t key_count = 0;

    if (group_entry == NULL ||
        group_entry->pgrp == NULL ||
        group_entry->pgrp[0] == '\0' ||
        strcmp(group_entry->pgrp, "-") == 0) {
      continue;
    }

    parent_group = find_group(document, group_entry->pgrp);
    if (parent_group == NULL) {
      status = report_add_diagnostic(
        report,
        "10c",
        AGS_DIAGNOSTIC_ERROR,
        group->group_line_number,
        group->name,
        NULL,
        "Parent group is not present in the document."
      );
      if (status != AGS_STATUS_OK) {
        return status;
      }
      continue;
    }

    key_count = collect_key_field_indices(parent_group, dictionary, parent_group->name, &parent_key_indices, &report->allocator);
    if (key_count == 0) {
      ags_dealloc(&report->allocator, parent_key_indices);
      continue;
    }

    for (row_index = 0; row_index < group->row_count; ++row_index) {
      size_t parent_row_index = 0;
      int matched = 0;

      for (parent_row_index = 0; parent_row_index < parent_group->row_count; ++parent_row_index) {
        size_t key_index = 0;
        matched = 1;

        for (key_index = 0; key_index < key_count; ++key_index) {
          size_t parent_field_index = parent_key_indices[key_index];
          size_t child_field_index = find_field_index(group, parent_group->fields[parent_field_index].name);
          if (child_field_index == (size_t)-1 ||
              strcmp(group->rows[row_index].values[child_field_index],
                     parent_group->rows[parent_row_index].values[parent_field_index]) != 0) {
            matched = 0;
            break;
          }
        }

        if (matched) {
          break;
        }
      }

      if (!matched) {
        status = report_add_diagnostic(
          report,
          "10c",
          AGS_DIAGNOSTIC_ERROR,
          group->rows[row_index].line_number,
          group->name,
          NULL,
          "Row key values do not match any row in the parent group."
        );
        if (status != AGS_STATUS_OK) {
          ags_dealloc(&report->allocator, parent_key_indices);
          return status;
        }
      }
    }

    ags_dealloc(&report->allocator, parent_key_indices);
  }

  return AGS_STATUS_OK;
}

static int string_in_group_column(
  const ags_group_internal *group,
  const char *field_name,
  const char *value
) {
  size_t field_index = find_field_index(group, field_name);
  size_t row_index = 0;

  if (group == NULL || value == NULL || field_index == (size_t)-1) {
    return 0;
  }

  for (row_index = 0; row_index < group->row_count; ++row_index) {
    if (strcmp(group->rows[row_index].values[field_index], value) == 0) {
      return 1;
    }
  }

  return 0;
}

static ags_status validate_unit_and_type_references(
  const ags_document *document,
  ags_validation_report *report
) {
  const ags_group_internal *unit_group = find_group(document, "UNIT");
  const ags_group_internal *type_group = find_group(document, "TYPE");
  size_t group_index = 0;
  ags_status status = AGS_STATUS_OK;

  if (unit_group == NULL || type_group == NULL) {
    return AGS_STATUS_OK;
  }

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    size_t field_index = 0;

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      const char *field_unit = group->fields[field_index].unit;
      const char *field_type = group->fields[field_index].type;

      if (field_type[0] != '\0' &&
          !string_in_group_column(type_group, "TYPE_TYPE", field_type)) {
        status = report_add_diagnostic(
          report,
          "17",
          AGS_DIAGNOSTIC_ERROR,
          group->type_line_number,
          group->name,
          group->fields[field_index].name,
          "Field TYPE is not defined in the TYPE group."
        );
        if (status != AGS_STATUS_OK) {
          return status;
        }
      }

      if (field_unit[0] != '\0' &&
          !string_in_group_column(unit_group, "UNIT_UNIT", field_unit)) {
        status = report_add_diagnostic(
          report,
          "15",
          AGS_DIAGNOSTIC_ERROR,
          group->unit_line_number,
          group->name,
          group->fields[field_index].name,
          "Field UNIT is not defined in the UNIT group."
        );
        if (status != AGS_STATUS_OK) {
          return status;
        }
      }

      if (strcmp(field_type, "PU") == 0) {
        size_t row_index = 0;
        for (row_index = 0; row_index < group->row_count; ++row_index) {
          const char *value = group->rows[row_index].values[field_index];
          if (value[0] != '\0' && !string_in_group_column(unit_group, "UNIT_UNIT", value)) {
            status = report_add_diagnostic(
              report,
              "15",
              AGS_DIAGNOSTIC_ERROR,
              group->rows[row_index].line_number,
              group->name,
              group->fields[field_index].name,
              "Unit-coded field value is not defined in the UNIT group."
            );
            if (status != AGS_STATUS_OK) {
              return status;
            }
          }
        }
      }
    }
  }

  return AGS_STATUS_OK;
}

static ags_status validate_abbreviations(
  const ags_document *document,
  ags_validation_report *report
) {
  const ags_group_internal *abbr_group = find_group(document, "ABBR");
  const char *concat = tran_value_or_default(document, "TRAN_RCON", "+");
  char concat_char = concat[0];
  size_t group_index = 0;
  int saw_pa_value = 0;
  ags_status status = AGS_STATUS_OK;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    size_t field_index = 0;

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      if (strcmp(group->fields[field_index].type, "PA") == 0) {
        size_t row_index = 0;
        for (row_index = 0; row_index < group->row_count; ++row_index) {
          const char *value = group->rows[row_index].values[field_index];
          if (value[0] == '\0') {
            continue;
          }
          saw_pa_value = 1;
          if (abbr_group != NULL) {
            const char *cursor = value;
            const char *segment_start = value;
            size_t segment_length = 0;

            while (1) {
              if (*cursor == concat_char || *cursor == '\0') {
                char code[128];
                segment_length = (size_t)(cursor - segment_start);
                if (segment_length >= sizeof(code)) {
                  segment_length = sizeof(code) - 1;
                }
                memcpy(code, segment_start, segment_length);
                code[segment_length] = '\0';
                if (code[0] != '\0' &&
                    !string_in_group_column(abbr_group, "ABBR_CODE", code)) {
                  status = report_add_diagnostic(
                    report,
                    "16",
                    AGS_DIAGNOSTIC_ERROR,
                    group->rows[row_index].line_number,
                    group->name,
                    group->fields[field_index].name,
                    "Abbreviation code is not defined in the ABBR group."
                  );
                  if (status != AGS_STATUS_OK) {
                    return status;
                  }
                }
                if (*cursor == '\0') {
                  break;
                }
                segment_start = cursor + 1;
              }
              cursor += 1;
            }
          }
        }
      }
    }
  }

  if (saw_pa_value && abbr_group == NULL) {
    status = report_add_diagnostic(
      report,
      "16",
      AGS_DIAGNOSTIC_WARNING,
      0,
      "ABBR",
      NULL,
      "ABBR group is required when PA values are present."
    );
  }

  return status;
}

static ags_status validate_record_links(
  const ags_document *document,
  const ags_effective_dictionary *dictionary,
  ags_validation_report *report
) {
  const char *delimiter = tran_value_or_default(document, "TRAN_DLIM", "|");
  const char *concat = tran_value_or_default(document, "TRAN_RCON", "+");
  char delimiter_char = delimiter[0];
  char concat_char = concat[0];
  size_t group_index = 0;
  ags_status status = AGS_STATUS_OK;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    size_t field_index = 0;

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      if (strcmp(group->fields[field_index].type, "RL") == 0) {
        size_t row_index = 0;
        for (row_index = 0; row_index < group->row_count; ++row_index) {
          const char *value = group->rows[row_index].values[field_index];
          const char *cursor = value;
          const char *segment_start = value;

          while (1) {
            if (*cursor == concat_char || *cursor == '\0') {
              char link[256];
              const ags_group_internal *target_group = NULL;
              size_t link_length = (size_t)(cursor - segment_start);
              size_t target_key_count = 0;
              size_t *target_key_indices = NULL;
              char *parts[16];
              size_t part_count = 0;
              char *part_cursor = NULL;
              int matched = 0;
              size_t target_row_index = 0;

              if (link_length >= sizeof(link)) {
                link_length = sizeof(link) - 1;
              }
              memcpy(link, segment_start, link_length);
              link[link_length] = '\0';

              if (link[0] != '\0') {
                parts[part_count++] = link;
                for (part_cursor = link; *part_cursor != '\0'; ++part_cursor) {
                  if (*part_cursor == delimiter_char) {
                    *part_cursor = '\0';
                    parts[part_count++] = part_cursor + 1;
                  }
                  if (part_count >= 16) {
                    break;
                  }
                }

                if (part_count < 1) {
                  status = report_add_diagnostic(
                    report, "11", AGS_DIAGNOSTIC_ERROR, group->rows[row_index].line_number,
                    group->name, group->fields[field_index].name, "Record link is malformed."
                  );
                  if (status != AGS_STATUS_OK) {
                    return status;
                  }
                } else {
                  target_group = find_group(document, parts[0]);
                  if (target_group == NULL) {
                    status = report_add_diagnostic(
                      report, "11", AGS_DIAGNOSTIC_ERROR, group->rows[row_index].line_number,
                      group->name, group->fields[field_index].name, "Record link target group is not present."
                    );
                    if (status != AGS_STATUS_OK) {
                      return status;
                    }
                  } else {
                    target_key_count = collect_key_field_indices(
                      target_group,
                      dictionary,
                      target_group->name,
                      &target_key_indices,
                      &report->allocator
                    );
                    if ((part_count - 1) != target_key_count) {
                      status = report_add_diagnostic(
                        report, "11", AGS_DIAGNOSTIC_ERROR, group->rows[row_index].line_number,
                        group->name, group->fields[field_index].name, "Record link does not match target key shape."
                      );
                      if (status != AGS_STATUS_OK) {
                        ags_dealloc(&report->allocator, target_key_indices);
                        return status;
                      }
                    } else {
                      for (target_row_index = 0; target_row_index < target_group->row_count; ++target_row_index) {
                        size_t key_index = 0;
                        matched = 1;
                        for (key_index = 0; key_index < target_key_count; ++key_index) {
                          if (strcmp(
                                target_group->rows[target_row_index].values[target_key_indices[key_index]],
                                parts[key_index + 1]) != 0) {
                            matched = 0;
                            break;
                          }
                        }
                        if (matched) {
                          break;
                        }
                      }
                      if (!matched) {
                        status = report_add_diagnostic(
                          report, "11", AGS_DIAGNOSTIC_ERROR, group->rows[row_index].line_number,
                          group->name, group->fields[field_index].name, "Record link target row was not found."
                        );
                        if (status != AGS_STATUS_OK) {
                          ags_dealloc(&report->allocator, target_key_indices);
                          return status;
                        }
                      }
                    }
                    ags_dealloc(&report->allocator, target_key_indices);
                  }
                }
              }

              if (*cursor == '\0') {
                break;
              }
              segment_start = cursor + 1;
            }
            cursor += 1;
          }
        }
      }
    }
  }

  return AGS_STATUS_OK;
}

ags_status ags_validate_document_with_dictionary(
  const ags_document *document,
  const ags_validate_options *options,
  ags_validation_report **out_report
) {
  ags_validate_options resolved_options;
  ags_document_options doc_options;
  ags_document *loaded_dictionary = NULL;
  const ags_document *standard_dictionary = NULL;
  const char *resolved_version = NULL;
  ags_effective_dictionary effective_dictionary;
  ags_validation_report *report = NULL;
  ags_status status = AGS_STATUS_OK;

  if (document == NULL || out_report == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = ags_validate_options_init(&resolved_options);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  if (options != NULL) {
    if (options->struct_size != sizeof(*options)) {
      return AGS_STATUS_INVALID_ARGUMENT;
    }
    resolved_options = *options;
  }

  status = ags_validate_text("", 0, &resolved_options, &report);
  if (status != AGS_STATUS_OK) {
    return status;
  }
  report->diagnostic_count = 0;

  if (resolved_options.dictionary_document != NULL) {
    standard_dictionary = resolved_options.dictionary_document;
  } else {
    status = ags_dictionary_resolve_version(
      document,
      resolved_options.dictionary_version,
      &resolved_version
    );
    if (status != AGS_STATUS_OK) {
      ags_validation_report_destroy(report);
      return status;
    }

    status = ags_document_options_init(&doc_options);
    if (status != AGS_STATUS_OK) {
      ags_validation_report_destroy(report);
      return status;
    }
    doc_options.allocator = report->allocator.malloc_fn != NULL ? &report->allocator : NULL;

    status = ags_dictionary_load_bundled(resolved_version, &doc_options, &loaded_dictionary);
    if (status != AGS_STATUS_OK) {
      ags_validation_report_destroy(report);
      return status;
    }
    standard_dictionary = loaded_dictionary;
  }

  status = ags_effective_dictionary_build(
    &effective_dictionary,
    &report->allocator,
    standard_dictionary,
    document
  );
  if (status != AGS_STATUS_OK) {
    ags_document_destroy(loaded_dictionary);
    ags_validation_report_destroy(report);
    return status;
  }

  status = add_required_group_diagnostics(document, report);
  if (status == AGS_STATUS_OK) {
    status = validate_heading_existence_and_order(document, &effective_dictionary, report);
  }
  if (status == AGS_STATUS_OK) {
    status = validate_required_fields(document, &effective_dictionary, report);
  }
  if (status == AGS_STATUS_OK) {
    status = validate_key_uniqueness(document, &effective_dictionary, report);
  }
  if (status == AGS_STATUS_OK) {
    status = validate_parent_child_links(document, &effective_dictionary, report);
  }
  if (status == AGS_STATUS_OK) {
    status = validate_record_links(document, &effective_dictionary, report);
  }
  if (status == AGS_STATUS_OK) {
    status = validate_unit_and_type_references(document, report);
  }
  if (status == AGS_STATUS_OK) {
    status = validate_abbreviations(document, report);
  }

  ags_effective_dictionary_destroy(&effective_dictionary);
  ags_document_destroy(loaded_dictionary);

  if (status != AGS_STATUS_OK) {
    ags_validation_report_destroy(report);
    return status;
  }

  *out_report = report;
  return AGS_STATUS_OK;
}
