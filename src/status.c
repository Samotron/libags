#include "libags/status.h"

const char *ags_status_string(ags_status status) {
  switch (status) {
    case AGS_STATUS_OK:
      return "ok";
    case AGS_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case AGS_STATUS_NO_MEMORY:
      return "out of memory";
    case AGS_STATUS_NOT_FOUND:
      return "not found";
    case AGS_STATUS_IO_ERROR:
      return "i/o error";
    case AGS_STATUS_PARSE_ERROR:
      return "parse error";
    case AGS_STATUS_UNIMPLEMENTED:
      return "unimplemented";
    case AGS_STATUS_INTERNAL_ERROR:
      return "internal error";
    default:
      return "unknown status code";
  }
}

int ags_status_is_success(ags_status status) {
  return status == AGS_STATUS_OK;
}
