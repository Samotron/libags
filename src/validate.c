#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libags/validate.h"
#include "document_internal.h"

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

typedef struct validation_tokens {
  char **values;
  size_t count;
  size_t capacity;
} validation_tokens;

static ags_status report_pick_allocator(
  const ags_validate_options *options,
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

static ags_status report_create(
  const ags_validate_options *options,
  ags_validation_report **out_report
) {
  ags_allocator allocator;
  ags_validation_report *report = NULL;
  ags_status status = AGS_STATUS_OK;

  if (out_report == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_report = NULL;

  status = report_pick_allocator(options, &allocator);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  report = ags_alloc(&allocator, sizeof(*report));
  if (report == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memset(report, 0, sizeof(*report));
  report->allocator = allocator;
  *out_report = report;
  return AGS_STATUS_OK;
}

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

void ags_validation_report_destroy(ags_validation_report *report) {
  size_t diagnostic_index = 0;

  if (report == NULL) {
    return;
  }

  for (diagnostic_index = 0; diagnostic_index < report->diagnostic_count; ++diagnostic_index) {
    diagnostic_clear(report, &report->diagnostics[diagnostic_index]);
  }

  ags_dealloc(&report->allocator, report->diagnostics);
  ags_dealloc(&report->allocator, report);
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

static void validation_tokens_reset(const ags_allocator *allocator, validation_tokens *tokens) {
  size_t index = 0;

  if (allocator == NULL || tokens == NULL) {
    return;
  }

  for (index = 0; index < tokens->count; ++index) {
    ags_dealloc(allocator, tokens->values[index]);
  }

  ags_dealloc(allocator, tokens->values);
  memset(tokens, 0, sizeof(*tokens));
}

static ags_status validation_tokens_push(
  const ags_allocator *allocator,
  validation_tokens *tokens,
  char *value
) {
  char **values = NULL;
  size_t new_capacity = 0;

  if (allocator == NULL || tokens == NULL || value == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  if (tokens->count == tokens->capacity) {
    new_capacity = tokens->capacity == 0 ? 4 : tokens->capacity * 2;
    values = ags_realloc_buffer(allocator, tokens->values, new_capacity * sizeof(*values));
    if (values == NULL) {
      return AGS_STATUS_NO_MEMORY;
    }

    tokens->values = values;
    tokens->capacity = new_capacity;
  }

  tokens->values[tokens->count++] = value;
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

static int is_ascii_line(const char *line, size_t length) {
  size_t index = 0;

  for (index = 0; index < length; ++index) {
    if ((unsigned char)line[index] > 127U) {
      return 0;
    }
  }

  return 1;
}

static int is_valid_descriptor(const char *value) {
  return strcmp(value, "GROUP") == 0 ||
         strcmp(value, "HEADING") == 0 ||
         strcmp(value, "UNIT") == 0 ||
         strcmp(value, "TYPE") == 0 ||
         strcmp(value, "DATA") == 0;
}

static ags_status tokenize_validation_line(
  const ags_allocator *allocator,
  const char *line,
  size_t length,
  validation_tokens *out_tokens
) {
  size_t index = 0;
  ags_status status = AGS_STATUS_OK;

  if (allocator == NULL || line == NULL || out_tokens == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  memset(out_tokens, 0, sizeof(*out_tokens));

  while (index < length) {
    char *value = NULL;
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

    value[write_index] = '\0';
    status = validation_tokens_push(allocator, out_tokens, value);
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
  validation_tokens_reset(allocator, out_tokens);
  return status;
}

static ags_status validate_raw_text_into_report(
  const char *input,
  size_t length,
  ags_validation_report *report
) {
  size_t position = 0;
  size_t line_number = 1;
  char current_group[5];
  size_t heading_count = 0;
  int have_group = 0;
  int have_heading = 0;
  int have_unit = 0;
  int have_type = 0;
  ags_status status = AGS_STATUS_OK;

  memset(current_group, 0, sizeof(current_group));

  while (position <= length) {
    size_t line_start = position;
    size_t line_end = position;
    size_t next_position = position;
    int line_has_crlf = 0;
    validation_tokens tokens;
    const char *descriptor = NULL;

    while (line_end < length && input[line_end] != '\n' && input[line_end] != '\r') {
      line_end += 1;
    }

    next_position = line_end;
    if (next_position < length) {
      if (input[next_position] == '\r' &&
          (next_position + 1) < length &&
          input[next_position + 1] == '\n') {
        next_position += 2;
        line_has_crlf = 1;
      } else {
        next_position += 1;
        line_has_crlf = 0;
      }
    } else {
      line_has_crlf = 0;
      next_position += 1;
    }

    position = next_position;

    if (line_end == line_start || is_blank_line(input + line_start, line_end - line_start)) {
      line_number += 1;
      continue;
    }

    if (!is_ascii_line(input + line_start, line_end - line_start)) {
      status = report_add_diagnostic(
        report,
        "1",
        AGS_DIAGNOSTIC_ERROR,
        line_number,
        NULL,
        NULL,
        "Line contains non-ASCII characters."
      );
      if (status != AGS_STATUS_OK) {
        return status;
      }
    }

    if (!line_has_crlf) {
      status = report_add_diagnostic(
        report,
        "2a",
        AGS_DIAGNOSTIC_WARNING,
        line_number,
        NULL,
        NULL,
        "Line is not terminated by CRLF."
      );
      if (status != AGS_STATUS_OK) {
        return status;
      }
    }

    status = tokenize_validation_line(
      &report->allocator,
      input + line_start,
      line_end - line_start,
      &tokens
    );
    if (status == AGS_STATUS_PARSE_ERROR) {
      status = report_add_diagnostic(
        report,
        "5",
        AGS_DIAGNOSTIC_ERROR,
        line_number,
        have_group ? current_group : NULL,
        NULL,
        "Fields are not properly quoted or escaped."
      );
      if (status != AGS_STATUS_OK) {
        return status;
      }

      line_number += 1;
      continue;
    }
    if (status != AGS_STATUS_OK) {
      return status;
    }

    if (tokens.count == 0) {
      validation_tokens_reset(&report->allocator, &tokens);
      line_number += 1;
      continue;
    }

    descriptor = tokens.values[0];
    if (!is_valid_descriptor(descriptor)) {
      status = report_add_diagnostic(
        report,
        "3",
        AGS_DIAGNOSTIC_ERROR,
        line_number,
        have_group ? current_group : NULL,
        NULL,
        "Row does not start with a valid descriptor."
      );
      if (status != AGS_STATUS_OK) {
        validation_tokens_reset(&report->allocator, &tokens);
        return status;
      }
    }

    if (strcmp(descriptor, "GROUP") == 0) {
      if (have_group && (!have_heading || !have_unit || !have_type)) {
        status = report_add_diagnostic(
          report,
          "4.2",
          AGS_DIAGNOSTIC_ERROR,
          line_number,
          current_group,
          NULL,
          "Group is missing HEADING, UNIT, TYPE, or DATA context."
        );
        if (status != AGS_STATUS_OK) {
          validation_tokens_reset(&report->allocator, &tokens);
          return status;
        }
      }

      if (tokens.count != 2) {
        status = report_add_diagnostic(
          report,
          "4.1",
          AGS_DIAGNOSTIC_ERROR,
          line_number,
          NULL,
          NULL,
          "GROUP row must contain exactly one group name."
        );
        if (status != AGS_STATUS_OK) {
          validation_tokens_reset(&report->allocator, &tokens);
          return status;
        }

        have_group = 0;
        current_group[0] = '\0';
      } else {
        snprintf(current_group, sizeof(current_group), "%s", tokens.values[1]);
        have_group = 1;
      }

      have_heading = 0;
      have_unit = 0;
      have_type = 0;
      heading_count = 0;
    } else if (strcmp(descriptor, "HEADING") == 0) {
      if (!have_group) {
        status = report_add_diagnostic(
          report,
          "4.2",
          AGS_DIAGNOSTIC_ERROR,
          line_number,
          NULL,
          NULL,
          "HEADING row encountered before GROUP row."
        );
        if (status != AGS_STATUS_OK) {
          validation_tokens_reset(&report->allocator, &tokens);
          return status;
        }
      } else {
        have_heading = 1;
        heading_count = tokens.count > 0 ? tokens.count - 1 : 0;
      }
    } else if (strcmp(descriptor, "UNIT") == 0 ||
               strcmp(descriptor, "TYPE") == 0 ||
               strcmp(descriptor, "DATA") == 0) {
      if (!have_heading) {
        status = report_add_diagnostic(
          report,
          "4.2",
          AGS_DIAGNOSTIC_ERROR,
          line_number,
          have_group ? current_group : NULL,
          NULL,
          "HEADING row missing before UNIT, TYPE, or DATA row."
        );
        if (status != AGS_STATUS_OK) {
          validation_tokens_reset(&report->allocator, &tokens);
          return status;
        }
      } else if ((tokens.count - 1) != heading_count) {
        status = report_add_diagnostic(
          report,
          "4.2",
          AGS_DIAGNOSTIC_ERROR,
          line_number,
          have_group ? current_group : NULL,
          NULL,
          "Row does not have the same number of fields as the HEADING row."
        );
        if (status != AGS_STATUS_OK) {
          validation_tokens_reset(&report->allocator, &tokens);
          return status;
        }
      }

      if (strcmp(descriptor, "UNIT") == 0) {
        have_unit = 1;
      } else if (strcmp(descriptor, "TYPE") == 0) {
        have_type = 1;
      }
    }

    validation_tokens_reset(&report->allocator, &tokens);
    line_number += 1;
  }

  if (have_group && (!have_heading || !have_unit || !have_type)) {
    status = report_add_diagnostic(
      report,
      "4.2",
      AGS_DIAGNOSTIC_ERROR,
      line_number,
      current_group,
      NULL,
      "Group is missing HEADING, UNIT, TYPE, or DATA context."
    );
    if (status != AGS_STATUS_OK) {
      return status;
    }
  }

  return AGS_STATUS_OK;
}

static int is_valid_group_name(const char *group_name) {
  size_t index = 0;

  if (group_name == NULL || strlen(group_name) == 0 || strlen(group_name) > 4) {
    return 0;
  }

  for (index = 0; group_name[index] != '\0'; ++index) {
    if (!isupper((unsigned char)group_name[index]) &&
        !isdigit((unsigned char)group_name[index])) {
      return 0;
    }
  }

  return 1;
}

static int is_valid_heading_name(const char *heading_name) {
  size_t index = 0;

  if (heading_name == NULL || strlen(heading_name) == 0 || strlen(heading_name) > 9) {
    return 0;
  }

  for (index = 0; heading_name[index] != '\0'; ++index) {
    if (!isupper((unsigned char)heading_name[index]) &&
        !isdigit((unsigned char)heading_name[index]) &&
        heading_name[index] != '_') {
      return 0;
    }
  }

  return 1;
}

static int heading_is_known_in_other_group(
  const ags_document *document,
  size_t current_group_index,
  const char *heading_name
) {
  size_t group_index = 0;
  size_t field_index = 0;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];

    if (group_index == current_group_index) {
      continue;
    }

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      if (strcmp(group->fields[field_index].name, heading_name) == 0) {
        return 1;
      }
    }
  }

  return 0;
}

static int is_fixed_decimal_places(const char *value, unsigned int places) {
  const char *cursor = value;
  unsigned int actual_places = 0;

  if (*cursor == '+' || *cursor == '-') {
    cursor += 1;
  }

  if (!isdigit((unsigned char)*cursor)) {
    return 0;
  }

  while (isdigit((unsigned char)*cursor)) {
    cursor += 1;
  }

  if (places == 0U) {
    return *cursor == '\0';
  }

  if (*cursor != '.') {
    return 0;
  }

  cursor += 1;
  while (isdigit((unsigned char)*cursor)) {
    actual_places += 1U;
    cursor += 1;
  }

  return *cursor == '\0' && actual_places == places;
}

static int matches_time_template(const char *value, const char *pattern) {
  size_t index = 0;

  if (pattern == NULL || pattern[0] == '\0') {
    return 0;
  }

  for (index = 0; pattern[index] != '\0'; ++index) {
    char expected = pattern[index];
    char actual = value[index];

    if (actual == '\0') {
      return 0;
    }

    if (expected == 'h' || expected == 'm' || expected == 's' ||
        expected == 'y' || expected == 'd') {
      if (!isdigit((unsigned char)actual)) {
        return 0;
      }
    } else if (expected == '+') {
      if (actual != '+' && actual != '-') {
        return 0;
      }
    } else if (expected != actual) {
      return 0;
    }
  }

  return value[index] == '\0';
}

static int is_scientific_notation(const char *value, unsigned int places) {
  const char *cursor = value;
  unsigned int actual_places = 0;

  if (*cursor == '+' || *cursor == '-') {
    cursor += 1;
  }

  if (!isdigit((unsigned char)*cursor)) {
    return 0;
  }

  while (isdigit((unsigned char)*cursor)) {
    cursor += 1;
  }

  if (*cursor != '.') {
    return 0;
  }

  cursor += 1;
  while (isdigit((unsigned char)*cursor)) {
    actual_places += 1U;
    cursor += 1;
  }

  if (actual_places != places) {
    return 0;
  }

  if (*cursor != 'e' && *cursor != 'E') {
    return 0;
  }

  cursor += 1;
  if (*cursor == '+' || *cursor == '-') {
    cursor += 1;
  }

  if (!isdigit((unsigned char)*cursor)) {
    return 0;
  }

  while (isdigit((unsigned char)*cursor)) {
    cursor += 1;
  }

  return *cursor == '\0';
}

static unsigned int count_significant_figures(const char *value) {
  const char *cursor = value;
  unsigned int count = 0;
  int significant_started = 0;

  if (*cursor == '+' || *cursor == '-') {
    cursor += 1;
  }

  while (*cursor != '\0') {
    if (*cursor == '.') {
      cursor += 1;
      continue;
    }

    if (!isdigit((unsigned char)*cursor)) {
      return 0;
    }

    if (!significant_started) {
      if (*cursor != '0') {
        significant_started = 1;
        count += 1U;
      }
    } else {
      count += 1U;
    }

    cursor += 1;
  }

  if (!significant_started) {
    return 1U;
  }

  return count;
}

static int value_matches_type(const char *type, const char *unit, const char *value) {
  char *end = NULL;
  unsigned long number = 0;

  if (type == NULL || value == NULL) {
    return 0;
  }

  if (value[0] == '\0') {
    return 1;
  }

  if (strcmp(type, "ID") == 0 ||
      strcmp(type, "PA") == 0 ||
      strcmp(type, "PT") == 0 ||
      strcmp(type, "PU") == 0 ||
      strcmp(type, "X") == 0 ||
      strcmp(type, "XN") == 0 ||
      strcmp(type, "U") == 0 ||
      strcmp(type, "RL") == 0) {
    return 1;
  }

  if (strcmp(type, "YN") == 0) {
    return (strcmp(value, "Y") == 0 ||
            strcmp(value, "N") == 0 ||
            strcmp(value, "y") == 0 ||
            strcmp(value, "n") == 0);
  }

  if (strcmp(type, "T") == 0) {
    return matches_time_template(value, unit != NULL && unit[0] != '\0' ? unit : "hh:mm:ss");
  }

  if (strcmp(type, "DT") == 0) {
    const char *pattern = unit != NULL && unit[0] != '\0' ? unit : "yyyy-mm-dd";
    return matches_time_template(value, pattern);
  }

  if (strlen(type) > 2 && strcmp(type + strlen(type) - 2, "DP") == 0) {
    number = strtoul(type, &end, 10);
    if (end == type || strcmp(end, "DP") != 0) {
      return 1;
    }

    return is_fixed_decimal_places(value, (unsigned int)number);
  }

  if (strlen(type) > 3 && strcmp(type + strlen(type) - 3, "SCI") == 0) {
    number = strtoul(type, &end, 10);
    if (end == type || strcmp(end, "SCI") != 0) {
      return 1;
    }

    return is_scientific_notation(value, (unsigned int)number);
  }

  if (strlen(type) > 2 && strcmp(type + strlen(type) - 2, "SF") == 0) {
    number = strtoul(type, &end, 10);
    if (end == type || strcmp(end, "SF") != 0) {
      return 1;
    }

    return count_significant_figures(value) == (unsigned int)number;
  }

  return 1;
}

static ags_status validate_document_into_report(
  const ags_document *document,
  ags_validation_report *report
) {
  size_t group_index = 0;
  ags_status status = AGS_STATUS_OK;

  for (group_index = 0; group_index < document->group_count; ++group_index) {
    const ags_group_internal *group = &document->groups[group_index];
    size_t field_index = 0;
    size_t row_index = 0;
    char prefix[16];

    if (!is_valid_group_name(group->name)) {
      status = report_add_diagnostic(
        report,
        "19",
        AGS_DIAGNOSTIC_ERROR,
        group->group_line_number,
        group->name,
        NULL,
        "Invalid GROUP name format."
      );
      if (status != AGS_STATUS_OK) {
        return status;
      }
    }

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      size_t other_index = 0;
      const char *heading_name = group->fields[field_index].name;

      if (!is_valid_heading_name(heading_name)) {
        status = report_add_diagnostic(
          report,
          "19a",
          AGS_DIAGNOSTIC_WARNING,
          group->heading_line_number,
          group->name,
          heading_name,
          "Invalid HEADING name format."
        );
        if (status != AGS_STATUS_OK) {
          return status;
        }
      }

      for (other_index = field_index + 1; other_index < group->field_count; ++other_index) {
        if (strcmp(heading_name, group->fields[other_index].name) == 0) {
          status = report_add_diagnostic(
            report,
            "7",
            AGS_DIAGNOSTIC_ERROR,
            group->heading_line_number,
            group->name,
            heading_name,
            "Duplicate headings found in group."
          );
          if (status != AGS_STATUS_OK) {
            return status;
          }
          break;
        }
      }
    }

    snprintf(prefix, sizeof(prefix), "%s_", group->name);

    for (field_index = 0; field_index < group->field_count; ++field_index) {
      const char *heading_name = group->fields[field_index].name;

      if (strncmp(heading_name, prefix, strlen(prefix)) != 0 &&
          strncmp(heading_name, "SPEC_", 5) != 0 &&
          strncmp(heading_name, "TEST_", 5) != 0 &&
          !heading_is_known_in_other_group(document, group_index, heading_name)) {
        status = report_add_diagnostic(
          report,
          "19b",
          AGS_DIAGNOSTIC_ERROR,
          group->heading_line_number,
          group->name,
          heading_name,
          "HEADING does not start with the group prefix and is not reused from another group."
        );
        if (status != AGS_STATUS_OK) {
          return status;
        }
      }
    }

    for (row_index = 0; row_index < group->row_count; ++row_index) {
      for (field_index = 0; field_index < group->field_count; ++field_index) {
        if (!value_matches_type(
              group->fields[field_index].type,
              group->fields[field_index].unit,
              group->rows[row_index].values[field_index])) {
          status = report_add_diagnostic(
            report,
            "8",
            AGS_DIAGNOSTIC_ERROR,
            group->rows[row_index].line_number,
            group->name,
            group->fields[field_index].name,
            "Data value does not match the declared TYPE and UNIT."
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

ags_status ags_validate_options_init(ags_validate_options *options) {
  if (options == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  options->struct_size = sizeof(*options);
  options->allocator = NULL;
  options->dictionary_version = NULL;
  options->dictionary_document = NULL;
  return AGS_STATUS_OK;
}

ags_status ags_validate_text(
  const char *input,
  size_t length,
  const ags_validate_options *options,
  ags_validation_report **out_report
) {
  ags_validation_report *report = NULL;
  ags_status status = AGS_STATUS_OK;

  if (input == NULL || out_report == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = report_create(options, &report);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = validate_raw_text_into_report(input, length, report);
  if (status != AGS_STATUS_OK) {
    ags_validation_report_destroy(report);
    return status;
  }

  *out_report = report;
  return AGS_STATUS_OK;
}

ags_status ags_validate_document(
  const ags_document *document,
  const ags_validate_options *options,
  ags_validation_report **out_report
) {
  ags_validation_report *report = NULL;
  ags_status status = AGS_STATUS_OK;

  if (document == NULL || out_report == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  status = report_create(options, &report);
  if (status != AGS_STATUS_OK) {
    return status;
  }

  status = validate_document_into_report(document, report);
  if (status != AGS_STATUS_OK) {
    ags_validation_report_destroy(report);
    return status;
  }

  *out_report = report;
  return AGS_STATUS_OK;
}

size_t ags_validation_report_diagnostic_count(const ags_validation_report *report) {
  return report == NULL ? 0 : report->diagnostic_count;
}

ags_diagnostic_severity ags_validation_report_diagnostic_severity(
  const ags_validation_report *report,
  size_t diagnostic_index
) {
  if (report == NULL || diagnostic_index >= report->diagnostic_count) {
    return AGS_DIAGNOSTIC_INFO;
  }

  return report->diagnostics[diagnostic_index].severity;
}

const char *ags_validation_report_diagnostic_rule(
  const ags_validation_report *report,
  size_t diagnostic_index
) {
  if (report == NULL || diagnostic_index >= report->diagnostic_count) {
    return NULL;
  }

  return report->diagnostics[diagnostic_index].rule;
}

const char *ags_validation_report_diagnostic_message(
  const ags_validation_report *report,
  size_t diagnostic_index
) {
  if (report == NULL || diagnostic_index >= report->diagnostic_count) {
    return NULL;
  }

  return report->diagnostics[diagnostic_index].message;
}

size_t ags_validation_report_diagnostic_line_number(
  const ags_validation_report *report,
  size_t diagnostic_index
) {
  if (report == NULL || diagnostic_index >= report->diagnostic_count) {
    return 0;
  }

  return report->diagnostics[diagnostic_index].line_number;
}

const char *ags_validation_report_diagnostic_group(
  const ags_validation_report *report,
  size_t diagnostic_index
) {
  if (report == NULL || diagnostic_index >= report->diagnostic_count) {
    return NULL;
  }

  return report->diagnostics[diagnostic_index].group;
}

const char *ags_validation_report_diagnostic_field(
  const ags_validation_report *report,
  size_t diagnostic_index
) {
  if (report == NULL || diagnostic_index >= report->diagnostic_count) {
    return NULL;
  }

  return report->diagnostics[diagnostic_index].field;
}
