# Spec 05: Tabular API Foundation

## Scope
Implement the first complete tabular and transformation slice for `libags`:

- neutral table handles for AGS group export and import
- duplicate-heading handling for tabular input
- numeric column parsing and formatting helpers
- document group sorting for input, alphabetical, dictionary, and hierarchical order
- point geometry derivation from easting/northing columns as `WKT` or `WKB`

## Deliverables
- public `tabular` API header and umbrella include
- table export/import implementation built on the existing document model
- numeric conversion helpers using AGS field metadata
- group-sorting helpers using bundled or custom effective dictionaries
- geometry derivation with heuristics and explicit column overrides
- integration-style tests covering tabular round-trip, duplicate handling, numeric conversion, sorting, and geometry

## Tasks

### Task 1: Public API and build wiring
- [x] Add public `tabular` header and umbrella include.
- [x] Add table, numeric-column, geometry-column, and sort option types.
- [x] Add table option initialization and object lifecycle APIs.
- [x] Wire the new implementation into the build.

### Task 2: Neutral table import and export
- [x] Export a document group to a neutral table handle.
- [x] Preserve group name, heading order, `UNIT`, and `TYPE` metadata in the table.
- [x] Import one or more tables back into a document while preserving table order.
- [x] Expose both cell-level and column-level accessors.
- [x] Implement duplicate-heading policies: reject, deterministic rename, and callback.

### Task 3: Numeric helpers
- [x] Parse compatible AGS numeric columns into typed numeric buffers with null tracking.
- [x] Format numeric buffers back into AGS text using the column `TYPE`.
- [x] Keep blank AGS values round-trippable as null numeric entries.

### Task 4: Sorting and geometry
- [x] Sort documents by input order, alphabetical order, dictionary order, and hierarchical order.
- [x] Resolve dictionary ordering from explicit dictionary override or document `TRAN_AGS`.
- [x] Detect easting/northing columns using explicit mapping or heading heuristics.
- [x] Derive geometry output as `WKT` text or `WKB` bytes.
- [x] Expose geometry metadata including column name, invalid-row count, and optional `SRID`/`CRS`.

### Task 5: Tests
- [x] Add export/import round-trip tests.
- [x] Add duplicate-heading policy tests.
- [x] Add numeric conversion tests.
- [x] Add sorting tests for all supported strategies.
- [x] Add geometry derivation tests for heuristics, explicit overrides, `WKT`, and `WKB`.
- [x] Keep the full suite green.
