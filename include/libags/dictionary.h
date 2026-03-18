#ifndef LIBAGS_DICTIONARY_H
#define LIBAGS_DICTIONARY_H

#include "libags/document.h"
#include "libags/export.h"
#include "libags/status.h"

LIBAGS_EXTERN_C_BEGIN

LIBAGS_API const char *ags_dictionary_latest_version(void);
LIBAGS_API int ags_dictionary_version_is_supported(const char *version);
LIBAGS_API ags_status ags_dictionary_load_buffer(
  const char *input,
  size_t length,
  const ags_document_options *options,
  ags_document **out_document
);
LIBAGS_API ags_status ags_dictionary_load_file(
  const char *path,
  const ags_document_options *options,
  ags_document **out_document
);
LIBAGS_API ags_status ags_dictionary_resolve_version(
  const ags_document *document,
  const char *override_version,
  const char **out_version
);
LIBAGS_API ags_status ags_dictionary_load_bundled(
  const char *version,
  const ags_document_options *options,
  ags_document **out_document
);

LIBAGS_EXTERN_C_END

#endif
