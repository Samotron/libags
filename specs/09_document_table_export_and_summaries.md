# Spec 09: Document Table Export and Summaries

## Scope
Add a document-level tabular export layer to `libags` so consumers do not have to reimplement:

- whole-document export from one parsed AGS file into a collection of tables
- per-table summaries for discovery and UI
- per-column summaries for schema inspection and typed-ingest planning
- lightweight column classification for numeric and geometry-capable fields

This spec is motivated by the DuckDB extension, but the API should remain neutral so Rust and other tabular consumers can reuse it.

## Problem
Today `libags` exposes:

- document accessors
- single-group export via `ags_table_from_group`
- per-column numeric conversion via `ags_table_column_to_numeric`
- geometry derivation on one table at a time

That is enough to build a one-group-at-a-time adapter, but not enough to build an ergonomic whole-file adapter without duplicating traversal, summary, and schema-analysis logic in the consumer.

Current pain points for DuckDB-style consumers:

- exporting an entire AGS document requires looping over every group manually
- there is no owned collection type for “all exported tables”
- there is no summary API for groups or columns
- inferring output types requires attempting full numeric conversion on each column
- there is no canonical “read whole file” tabular form for engines that need one relation per scan

## Deliverables

- public API for exporting all groups in a document as a reusable collection
- public API for per-table summaries
- public API for per-column summaries and lightweight classification
- FFI exports for the new summary surfaces
- tests covering whole-document export, summaries, duplicate headings, and numeric classification
- one DuckDB-style example updated to use the new surfaces

## Proposed API

### Option A: owned table collection

Add an owned collection type:

- `typedef struct ags_table_collection ags_table_collection;`

Lifecycle:

- `ags_status ags_document_export_tables(const ags_document *document, const ags_table_options *options, ags_table_collection **out_collection);`
- `void ags_table_collection_destroy(ags_table_collection *collection);`

Accessors:

- `size_t ags_table_collection_count(const ags_table_collection *collection);`
- `const ags_table *ags_table_collection_get(const ags_table_collection *collection, size_t table_index);`

Convenience loaders:

- `ags_status ags_table_collection_from_file(const char *path, const ags_document_options *document_options, const ags_table_options *table_options, ags_table_collection **out_collection);`
- `ags_status ags_table_collection_from_buffer(const char *input, size_t length, const ags_document_options *document_options, const ags_table_options *table_options, ags_table_collection **out_collection);`

Rationale:

- gives DuckDB a one-parse, many-table export path
- keeps ownership and destruction explicit
- avoids exposing raw `ags_table **` arrays in the ABI

### Table summaries

Add a plain summary struct:

- `typedef struct ags_table_summary { ... } ags_table_summary;`

Suggested fields:

- `size_t group_index`
- `const char *group_name`
- `size_t column_count`
- `size_t row_count`
- `size_t group_line_number`
- `size_t heading_line_number`
- `size_t unit_line_number`
- `size_t type_line_number`
- `size_t numeric_column_count`
- `size_t geometry_candidate_count`

APIs:

- `ags_status ags_table_collection_get_summary(const ags_table_collection *collection, size_t table_index, ags_table_summary *out_summary);`
- `ags_status ags_table_get_summary(const ags_table *table, ags_table_summary *out_summary);`

Rationale:

- lets DuckDB expose `ags_groups(...)` without rebuilding summary rows by hand
- makes summaries reusable for Rust bindings and CLI tools

### Column summaries and lightweight schema analysis

Add a small classification enum:

- `typedef enum ags_column_class { AGS_COLUMN_CLASS_TEXT = 0, AGS_COLUMN_CLASS_NUMERIC = 1, AGS_COLUMN_CLASS_GEOMETRY_EASTING_CANDIDATE = 2, AGS_COLUMN_CLASS_GEOMETRY_NORTHING_CANDIDATE = 3 } ags_column_class;`

Add a per-column summary struct:

- `typedef struct ags_column_summary { ... } ags_column_summary;`

Suggested fields:

- `size_t column_index`
- `const char *column_name`
- `const char *unit`
- `const char *type`
- `ags_column_class column_class`
- `size_t null_count`
- `size_t non_null_count`
- `int is_numeric`
- `int can_derive_geometry`

APIs:

- `ags_status ags_table_get_column_summary(const ags_table *table, size_t column_index, ags_column_summary *out_summary);`
- `ags_status ags_table_get_column_summaries(const ags_table *table, ags_column_summary *out_summaries, size_t summary_count);`

Design note:

- this API should not allocate numeric buffers unless the caller explicitly asks for conversion
- it should reuse the same type parsing rules already used by `ags_table_column_to_numeric`

Rationale:

- lets DuckDB implement `ags_columns(...)` directly from `libags`
- avoids allocating one `ags_numeric_column` per candidate column just to decide DuckDB output types

### Canonical whole-file tabular form

For engines that need one relation from one scan, add a long-form document export:

- `ags_status ags_document_export_long_table(const ags_document *document, const ags_table_options *options, ags_table **out_table);`

Suggested schema:

- `AGS_GROUP_INDEX`
- `AGS_GROUP_NAME`
- `AGS_ROW_INDEX`
- `AGS_LINE_NUMBER`
- `AGS_COLUMN_INDEX`
- `AGS_COLUMN_NAME`
- `AGS_UNIT`
- `AGS_TYPE`
- `AGS_VALUE`

Rationale:

- enables a future DuckDB `read_ags_all(path)` without synthesizing one DuckDB relation per AGS group
- gives non-DuckDB consumers a canonical lossless tabular representation of the entire file

## FFI additions

Extend `ffi.h` with borrowed export structs for summaries:

- `ags_table_summary_export`
- `ags_column_summary_export`

And borrowed accessors:

- `ags_status ags_table_collection_get_summary_export(...)`
- `ags_status ags_table_get_column_summary_export(...)`

This keeps the stable FFI layer aligned with the core API instead of forcing bindings to reconstruct summaries from low-level getters.

## Tasks

### Task 1: Public types and ownership
- [x] Add `ags_table_collection` public type and lifecycle API.
- [x] Implement collection creation from document, file, and buffer.
- [x] Ensure collection ownership and destruction are allocator-safe.

### Task 2: Table summaries
- [x] Add `ags_table_summary` public struct.
- [x] Implement summary generation from `ags_table`.
- [x] Implement collection-level summary accessors.

### Task 3: Column summaries and classification
- [x] Add `ags_column_summary` and `ags_column_class`.
- [x] Implement lightweight numeric classification without allocating numeric buffers.
- [x] Expose geometry-candidate hints based on the existing heading heuristics.
- [x] Count null and non-null rows per column.

### Task 4: Canonical whole-file long-form export
- [x] Define the long-form schema and naming.
- [x] Implement `ags_document_export_long_table`.
- [x] Preserve enough metadata to round-trip group, row, and column identity.

### Task 5: FFI surface
- [x] Add summary export structs to `ffi.h`.
- [x] Add borrowed summary accessors.
- [x] Document lifetime rules for borrowed summary strings.

### Task 6: Tests
- [x] Add tests for whole-document export with multiple groups.
- [x] Add tests for duplicate-heading rename behavior across full-document export.
- [x] Add tests for per-table summaries.
- [x] Add tests for per-column summaries and numeric classification.
- [x] Add tests for long-form whole-file export.

### Task 7: Examples
- [x] Update the DuckDB-style example to export all groups through the collection API.
- [x] Add one example showing table and column summaries.

## Recommended implementation order

1. `ags_table_collection`
2. `ags_table_summary`
3. `ags_column_summary`
4. FFI exports
5. long-form whole-file export
6. examples and tests

## Expected DuckDB follow-up

Once this spec lands, the DuckDB extension can cleanly add:

- `read_ags_all(path)` backed by the long-form export
- cheaper `ags_columns(path, group)` backed by column summaries
- cheaper `ags_groups(path)` backed by table summaries
- a macro or procedure that materializes one DuckDB view/table per AGS group from one parsed file
