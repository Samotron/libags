# Development Notes

## Coding Conventions
- Language level: C17.
- Public headers live under `include/libags/`.
- Implementation files live under `src/`.
- Tests live under `tests/`.
- Build warnings are treated as errors in CI.
- Keep the public ABI small, explicit, and documented before adding features.

## ABI Rules
- Public APIs must use exported functions and opaque handle types.
- Public structs intended for future expansion should include a `struct_size` field.
- Do not expose internal struct layouts in public headers.
- Avoid global mutable state in the core library.
- Prefer additive ABI evolution over breaking changes.
- The current ABI version is `1`.

## Error and Status Conventions
- Functions return `ags_status` unless a null-safe accessor is more practical.
- `AGS_STATUS_OK` indicates success.
- Invalid caller input returns `AGS_STATUS_INVALID_ARGUMENT`.
- Allocation failures return `AGS_STATUS_NO_MEMORY`.
- Missing lookups return `AGS_STATUS_NOT_FOUND`.
- File read failures return `AGS_STATUS_IO_ERROR`.
- Strict parser failures return `AGS_STATUS_PARSE_ERROR`.
- Future unimplemented surfaces should return `AGS_STATUS_UNIMPLEMENTED` rather than silently succeeding.
- Internal invariant failures should return `AGS_STATUS_INTERNAL_ERROR`.

## Allocator Strategy
- The library accepts caller-provided allocators through `ags_allocator`.
- Handles copy allocator function pointers and user data on creation so caller-owned allocator structs do not need to outlive the handle.
- The default allocator wraps `malloc`, `realloc`, and `free`.
- Destroy operations must be null-safe.

## Reuse Constraints
- Do not copy implementation code from `python-AGS4` or other LGPL projects into `libags`.
- Treat AGS specifications, shipped AGS dictionaries, and black-box behavioral tests as reference material.
- Reference libraries may inform API coverage and validation behavior, but not direct code reuse.
