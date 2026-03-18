#ifndef LIBAGS_VALIDATE_H
#define LIBAGS_VALIDATE_H

#include <stddef.h>

#include "libags/allocator.h"
#include "libags/document.h"
#include "libags/export.h"
#include "libags/status.h"

LIBAGS_EXTERN_C_BEGIN

typedef enum ags_diagnostic_severity {
  AGS_DIAGNOSTIC_INFO = 0,
  AGS_DIAGNOSTIC_WARNING = 1,
  AGS_DIAGNOSTIC_ERROR = 2
} ags_diagnostic_severity;

typedef struct ags_validation_report ags_validation_report;

typedef struct ags_validate_options {
  size_t struct_size;
  const ags_allocator *allocator;
  const char *dictionary_version;
  const ags_document *dictionary_document;
} ags_validate_options;

LIBAGS_API ags_status ags_validate_options_init(ags_validate_options *options);
LIBAGS_API ags_status ags_validate_text(
  const char *input,
  size_t length,
  const ags_validate_options *options,
  ags_validation_report **out_report
);
LIBAGS_API ags_status ags_validate_document(
  const ags_document *document,
  const ags_validate_options *options,
  ags_validation_report **out_report
);
LIBAGS_API ags_status ags_validate_document_with_dictionary(
  const ags_document *document,
  const ags_validate_options *options,
  ags_validation_report **out_report
);
LIBAGS_API void ags_validation_report_destroy(ags_validation_report *report);
LIBAGS_API size_t ags_validation_report_diagnostic_count(const ags_validation_report *report);
LIBAGS_API ags_diagnostic_severity ags_validation_report_diagnostic_severity(
  const ags_validation_report *report,
  size_t diagnostic_index
);
LIBAGS_API const char *ags_validation_report_diagnostic_rule(
  const ags_validation_report *report,
  size_t diagnostic_index
);
LIBAGS_API const char *ags_validation_report_diagnostic_message(
  const ags_validation_report *report,
  size_t diagnostic_index
);
LIBAGS_API size_t ags_validation_report_diagnostic_line_number(
  const ags_validation_report *report,
  size_t diagnostic_index
);
LIBAGS_API const char *ags_validation_report_diagnostic_group(
  const ags_validation_report *report,
  size_t diagnostic_index
);
LIBAGS_API const char *ags_validation_report_diagnostic_field(
  const ags_validation_report *report,
  size_t diagnostic_index
);

LIBAGS_EXTERN_C_END

#endif
