#include "libags/version.h"

int ags_version_major(void) {
  return AGS_VERSION_MAJOR;
}

int ags_version_minor(void) {
  return AGS_VERSION_MINOR;
}

int ags_version_patch(void) {
  return AGS_VERSION_PATCH;
}

const char *ags_version_string(void) {
  return "0.1.0";
}

uint32_t ags_abi_version(void) {
  return AGS_ABI_VERSION;
}
