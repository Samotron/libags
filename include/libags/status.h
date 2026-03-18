#ifndef LIBAGS_STATUS_H
#define LIBAGS_STATUS_H

#include "libags/export.h"

LIBAGS_EXTERN_C_BEGIN

typedef enum ags_status {
  AGS_STATUS_OK = 0,
  AGS_STATUS_INVALID_ARGUMENT = 1,
  AGS_STATUS_NO_MEMORY = 2,
  AGS_STATUS_NOT_FOUND = 3,
  AGS_STATUS_IO_ERROR = 4,
  AGS_STATUS_PARSE_ERROR = 5,
  AGS_STATUS_UNIMPLEMENTED = 6,
  AGS_STATUS_INTERNAL_ERROR = 7
} ags_status;

LIBAGS_API const char *ags_status_string(ags_status status);
LIBAGS_API int ags_status_is_success(ags_status status);

LIBAGS_EXTERN_C_END

#endif
