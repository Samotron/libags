# Spec 01: Foundation Bootstrap

## Scope
Implement the first concrete slice of `libags`:

- repository and build layout
- public ABI foundation
- allocator contract
- opaque document lifecycle
- test harness and CI entry points

This spec intentionally does not implement AGS parsing yet. It establishes the project shape and the first stable C surface that later parser, validator, tabular, geometry, and merge work will build on.

## Deliverables
- C17 build using CMake
- static and shared library targets
- public headers under `include/libags/`
- source files under `src/`
- unit tests under `tests/`
- CI workflow definition
- project license
- basic developer documentation for coding rules and ABI constraints

## Tasks

### Task 1: Project layout
- [x] Create `include/`, `src/`, `tests/`, `docs/`, and `.github/workflows/`.
- [x] Add a root `CMakeLists.txt` that builds static and shared `libags`.
- [x] Add test integration with `CTest`.

### Task 2: Public ABI foundation
- [x] Define version macros and runtime version query functions.
- [x] Define public status codes and status-to-string helpers.
- [x] Define export macros and C/C++ compatibility guards.
- [x] Define opaque handle types for document lifecycle.

### Task 3: Allocator and lifecycle APIs
- [x] Define an allocator struct with `malloc`, `realloc`, `free`, and user context.
- [x] Expose default allocator helpers.
- [x] Implement empty document create and destroy using the allocator contract.
- [x] Ensure destroy is null-safe.

### Task 4: Documentation and licensing
- [x] Add an `MIT` license.
- [x] Document coding conventions, ABI rules, and reuse constraints.

### Task 5: Test harness and CI
- [x] Add a small test harness executable.
- [x] Add unit tests for version, status, allocator, and document lifecycle behavior.
- [x] Add a CI workflow that configures, builds, and runs tests.
