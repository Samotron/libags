# Spec 07: DuckDB and Rust Integration Surface

## Scope
Implement a small FFI-focused API layer for `libags` that is stable to bind from Rust and practical to consume from DuckDB extensions without exposing internal structs.

## Deliverables
- public `ffi` API header and umbrella include
- stable borrowed view types for strings and bytes
- row cursor helpers for document iteration
- column export helpers for table, numeric, and geometry handles
- DuckDB-style and Rust-binding examples
- ABI, lifetime, and smoke tests for the new FFI-facing surface

## Tasks

### Task 1: Public FFI header and build wiring
- [x] Add public `ffi` header and umbrella include.
- [x] Add the FFI implementation to the build.
- [x] Keep the new API free of internal-struct exposure.

### Task 2: Borrowed view primitives
- [x] Add `ags_string_view` for borrowed UTF-8 text.
- [x] Add `ags_bytes_view` for borrowed byte ranges such as `WKB`.
- [x] Add ABI compatibility helpers and borrowed views for version and status strings.
- [x] Add borrowed view accessors for document, table, and geometry text data.

### Task 3: Row iteration and column export
- [x] Add a document row cursor for stable FFI iteration.
- [x] Add table column export helpers for DuckDB-style ingestion.
- [x] Add numeric column export helpers exposing value and null buffers.
- [x] Add geometry export helpers exposing encoding, null masks, and row buffers.

### Task 4: Examples and integration notes
- [x] Add a DuckDB-style ingestion example using table and geometry exports.
- [x] Add Rust binding notes showing `bindgen` usage and safe wrapper shape.
- [x] Document owner-bound lifetime rules for borrowed views and export buffers.

### Task 5: Tests
- [x] Add ABI smoke tests for the FFI helpers.
- [x] Add lifetime tests for borrowed string and byte views.
- [x] Add row cursor tests covering iteration and end-of-stream behavior.
- [x] Add table, numeric, and geometry export tests.
- [x] Keep the full suite green.
