#ifndef LIBAGS_MERGE_H
#define LIBAGS_MERGE_H

#include <stddef.h>

#include "libags/allocator.h"
#include "libags/document.h"
#include "libags/export.h"
#include "libags/status.h"
#include "libags/validate.h"

LIBAGS_EXTERN_C_BEGIN

typedef struct ags_merge_result ags_merge_result;

typedef enum ags_merge_conflict_policy {
  AGS_MERGE_CONFLICT_FAIL = 0,
  AGS_MERGE_CONFLICT_KEEP_FIRST = 1,
  AGS_MERGE_CONFLICT_KEEP_LAST = 2,
  AGS_MERGE_CONFLICT_MERGE_NON_EMPTY = 3,
  AGS_MERGE_CONFLICT_CALLBACK = 4
} ags_merge_conflict_policy;

typedef ags_status (*ags_merge_value_resolver_fn)(
  void *user_data,
  const char *group_name,
  const char *field_name,
  const char *existing_value,
  const char *incoming_value,
  const ags_allocator *allocator,
  char **out_value
);

typedef struct ags_merge_options {
  size_t struct_size;
  const ags_allocator *allocator;
  const char *dictionary_version;
  const ags_document *dictionary_document;
  ags_merge_conflict_policy keyed_row_policy;
  ags_merge_conflict_policy singleton_group_policy;
  ags_merge_value_resolver_fn value_resolver;
  void *value_resolver_user_data;
} ags_merge_options;

LIBAGS_API ags_status ags_merge_options_init(ags_merge_options *options);
LIBAGS_API ags_status ags_document_merge(
  const ags_document *const *documents,
  size_t document_count,
  const ags_merge_options *options,
  ags_merge_result **out_result
);
LIBAGS_API void ags_merge_result_destroy(ags_merge_result *result);
LIBAGS_API const ags_document *ags_merge_result_document(const ags_merge_result *result);

LIBAGS_API size_t ags_merge_result_diagnostic_count(const ags_merge_result *result);
LIBAGS_API ags_diagnostic_severity ags_merge_result_diagnostic_severity(
  const ags_merge_result *result,
  size_t diagnostic_index
);
LIBAGS_API const char *ags_merge_result_diagnostic_message(
  const ags_merge_result *result,
  size_t diagnostic_index
);
LIBAGS_API const char *ags_merge_result_diagnostic_group(
  const ags_merge_result *result,
  size_t diagnostic_index
);
LIBAGS_API const char *ags_merge_result_diagnostic_field(
  const ags_merge_result *result,
  size_t diagnostic_index
);
LIBAGS_API size_t ags_merge_result_diagnostic_source_document(
  const ags_merge_result *result,
  size_t diagnostic_index
);
LIBAGS_API size_t ags_merge_result_diagnostic_line_number(
  const ags_merge_result *result,
  size_t diagnostic_index
);

LIBAGS_API size_t ags_merge_result_row_source_count(
  const ags_merge_result *result,
  size_t group_index,
  size_t row_index
);
LIBAGS_API size_t ags_merge_result_row_source_document(
  const ags_merge_result *result,
  size_t group_index,
  size_t row_index,
  size_t source_index
);
LIBAGS_API size_t ags_merge_result_row_source_line_number(
  const ags_merge_result *result,
  size_t group_index,
  size_t row_index,
  size_t source_index
);

LIBAGS_EXTERN_C_END

#endif
