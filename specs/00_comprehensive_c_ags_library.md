# Spec 00: Comprehensive C AGS Library

## Status
- [x] Draft the initial specification from the AGS reference libraries and AGS4 dictionaries.

## Summary
Build `libags`, a comprehensive C library for reading, writing, validating, transforming, and exposing AGS4 data to DuckDB and other tabular systems. The target is feature parity with the combined core behavior of:

- `python-AGS4`: read/write AGS4, validate files, convert to tabular form, convert numeric columns to and from AGS text formatting, sort groups, and support standard AGS4 dictionaries.
- `@groundup-dev/ags`: parse AGS text, serialize parsed AGS, and run raw-text, parsed-structure, and dictionary-aware validation.

The library must be suitable for:

- embedding inside a DuckDB extension
- wrapping from Rust through a stable C ABI
- future use from other FFI consumers without redesigning the core

## References
- AGS validator reference: <https://github.com/groundup-dev/ags-validator/tree/main/ags>
- python-AGS4 reference: <https://pypi.org/project/python-AGS4/>
- AGS4 format reference: <http://www.agsdataformat.com/datatransferv4/intro.php>

## Goals
- Provide a dependency-light core C library for AGS4 parsing, validation, and serialization.
- Support AGS4 standard dictionary versions `4.0.3`, `4.0.4`, `4.1`, and `4.1.1`.
- Support custom `DICT` groups in addition to the bundled standard dictionaries.
- Preserve enough source metadata for precise diagnostics and stable round-tripping.
- Expose both document-oriented and table-oriented APIs.
- Support automatic geometry derivation from easting/northing columns into WKT and WKB.
- Support smart multi-file AGS merges driven by dictionary keys and caller-selected conflict policies.
- Keep the ABI stable and explicit for Rust and DuckDB consumers.
- Make large-file processing practical through streaming and bounded-memory modes.

## Non-Goals
- AGS3 support in the first release.
- A GUI.
- Pandas-specific APIs in the core library.
- Mandatory format-conversion dependencies in the core library.
- Copying implementation code from the reference libraries.

## Product Shape
The project should be split into small, composable layers:

- `libags_core`
  - tokenization
  - parsing
  - document model
  - serialization
  - diagnostics
- `libags_validate`
  - raw-text rules
  - parsed-structure rules
  - dictionary-aware rules
- `libags_dict`
  - bundled standard dictionaries
  - custom dictionary loading
  - merged dictionary resolution
- `libags_tabular`
  - group-to-table export
  - table-to-document import
  - sort and normalization helpers
- `libags_geo`
  - automatic easting/northing detection
  - WKT and WKB derivation
  - CRS and SRID mapping hooks
- `libags_merge`
  - multi-file merge planning
  - keyed deduplication
  - conflict detection and resolution
- `libags_ffi`
  - stable C ABI surface for Rust and DuckDB
- `agsctl` optional tool
  - validate files
  - print diagnostics
  - sort files
  - convert AGS to tabular interchange formats

Tabular format adapters should stay outside the core parser and validator whenever they introduce heavier dependencies. The core library should expose neutral table and geometry primitives that DuckDB and other consumers can use directly.

## Functional Requirements

### 1. Input and Parsing
- Parse AGS4 from file paths, memory buffers, and caller-supplied stream callbacks.
- Support UTF-8 input with strict ASCII validation rules where required by AGS.
- Detect and report invalid quoting, row descriptors, row arity mismatches, malformed `GROUP` blocks, and duplicate group names.
- Preserve:
  - group order
  - heading order
  - unit/type rows
  - line numbers for `GROUP`, `HEADING`, `UNIT`, `TYPE`, and `DATA`
- Offer two parse modes:
  - strict mode: stop on fatal structural errors
  - tolerant mode: continue parsing and accumulate diagnostics when safe

### 2. In-Memory Model
- Represent an AGS document as ordered groups.
- Represent each group with:
  - group name
  - heading definitions
  - unit row
  - type row
  - ordered data rows
  - source line metadata
- Represent diagnostics with:
  - rule id
  - severity
  - message
  - line number
  - group
  - field
- Use explicit ownership and allocator rules so the model is safe over FFI.

### 3. Serialization
- Serialize a parsed document back to canonical AGS text.
- Preserve logical data fidelity even when byte-for-byte original formatting is not retained.
- Support configurable line endings:
  - canonical CRLF output
  - LF output for development and tests
- Correctly escape embedded quotes and emit valid AGS rows for all groups.

### 4. Validation
- Implement validation in three phases:
  - raw-text validation
  - parsed-structure validation
  - dictionary-aware validation
- Raw-text validation must cover, at minimum:
  - ASCII-only checks
  - CRLF checks
  - valid row descriptors
  - `GROUP` row shape
  - `HEADING`/`UNIT`/`TYPE`/`DATA` field-count consistency
  - quoting and escaped quote rules
- Parsed-structure validation must cover, at minimum:
  - valid group naming
  - valid heading naming
  - duplicate headings
  - heading prefix rules
  - field type conformance for AGS `TYPE` values
- Dictionary-aware validation must cover, at minimum:
  - heading existence in standard or custom dictionary
  - heading order against dictionary order
  - required fields
  - required groups
  - key uniqueness
  - parent-child key integrity
  - record-link integrity
  - `UNIT`, `TYPE`, and `ABBR` cross-checks
  - user-defined groups and headings via `DICT`

### 5. Dictionary Support
- Bundle standard AGS4 dictionaries for:
  - `4.0.3`
  - `4.0.4`
  - `4.1`
  - `4.1.1`
- Resolve the effective dictionary by:
  - explicit caller override
  - `TRAN_AGS` version in the file
  - fallback to the latest bundled standard dictionary
- Support merging the bundled dictionary with a file-local `DICT` group.
- Expose lookup helpers for:
  - group definitions
  - heading definitions
  - required fields
  - key fields
  - parent groups
  - field ordering

### 6. Tabular, Spatial, and Transformation APIs
- Export each AGS group as a table-oriented structure suitable for DuckDB ingestion and other tabular outputs.
- Rebuild an AGS document from table-oriented input while preserving heading order.
- Expose neutral row-oriented and column-oriented table representations so adapters can target CSV, TSV, JSONL, Arrow-compatible buffers, and similar tabular formats without changing the parser.
- Provide helpers equivalent to the useful parts of `python-AGS4`:
  - convert AGS groups to typed numeric columns when possible
  - convert numeric columns back to AGS-formatted text using dictionary `TYPE` and `UNIT`
  - sort groups using:
    - input order
    - alphabetical order
    - dictionary order
    - hierarchical order
- Allow duplicate-heading handling strategies:
  - reject
  - rename with deterministic suffixes
  - caller-supplied policy callback
- Automatically derive geometry columns from easting/northing pairs.
- Support geometry output as:
  - WKT text
  - WKB bytes
- Resolve geometry source columns using:
  - dictionary metadata when available
  - common heading heuristics such as `*_EAST`, `*_NORT`, `*_NATE`, and `*_NATN`
  - explicit caller mapping overrides
- Allow caller control over:
  - target geometry column name
  - output encoding
  - CRS or SRID metadata
  - whether invalid coordinates fail, warn, or produce null geometry

### 7. Smart Merge APIs
- Merge multiple AGS files or parsed documents into one logical document.
- Use effective dictionary key definitions to identify duplicate or matching rows where possible.
- Support singleton metadata merge handling for `PROJ`, `TRAN`, `TYPE`, `UNIT`, `ABBR`, and `DICT`, with true single-row enforcement for `PROJ` and `TRAN`.
- Support configurable conflict policies:
  - fail on conflict
  - keep first
  - keep last
  - merge non-empty values
  - caller-supplied callback
- Merge dictionary content intelligently so custom `DICT` entries, units, types, and abbreviations are deduplicated rather than blindly appended.
- Preserve provenance metadata so merged rows can be traced back to source file and original line number.
- Emit diagnostics for:
  - incompatible singleton groups
  - conflicting keyed rows
  - unresolved parent-child relationships after merge
  - incompatible custom dictionary definitions

### 8. FFI and Embedding
- Expose a stable, versioned C ABI based on opaque handles.
- Avoid global mutable state.
- Be reentrant and thread-safe for independent documents and validator contexts.
- Support caller-provided allocators.
- Make error retrieval explicit and side-effect free.
- Expose iteration and direct lookup APIs that are practical for:
  - Rust wrappers
  - DuckDB table functions
  - DuckDB scalar or pragma-based validation entry points

### 9. CLI and Tabular Conversion Tools
- Ship a small CLI for:
  - `check`
  - `parse`
  - `format`
  - `sort`
  - `convert`
- Prefer neutral intermediate table abstractions in the core so adapters for DuckDB and other tabular formats stay separate from the parser.

### 10. Testing Requirements
- The library must ship with an automated test suite from the first implementation milestone onward.
- Every public API surface in the core library must have unit tests.
- Parser, serializer, validator, dictionary, tabular, geometry, and merge layers must each have dedicated test coverage.
- New validation rules must include:
  - passing fixtures
  - failing fixtures
  - expected diagnostic assertions
- The project must include:
  - unit tests
  - integration tests
  - fixture-based regression tests
  - round-trip tests
  - fuzz or property-style parser robustness tests
  - performance benchmarks for representative large files
- FFI-facing APIs must have ABI and memory-lifetime tests.
- CLI and tabular format adapters must have their own integration tests when enabled.
- CI must run the default test suite on every change.

## Proposed Public API Shape
The exact names may change, but the API should support the following families:

- document lifecycle
  - parse file
  - parse buffer
  - destroy document
  - serialize document
- document inspection
  - group count
  - group lookup by index and name
  - heading lookup
  - row and cell access
- validation
  - validate raw input
  - validate parsed document
  - validate with dictionary
  - enumerate diagnostics
- dictionary
  - load bundled standard dictionary
  - load custom dictionary from file or buffer
  - merge dictionaries
- transformation
  - export group to table
  - import table to group
  - derive WKT and WKB geometry columns
  - merge documents
  - sort groups
  - convert numeric/text columns
- FFI utilities
  - allocator registration
  - ABI version query
  - string-view helpers
  - status-code to message helpers

## Design Constraints
- The core must compile cleanly as C17.
- The library should build as static and shared artifacts.
- No hidden dependence on C++.
- No mandatory dependency on GLib, ICU, Python, or Arrow in the core.
- Optional modules may depend on extra libraries, but those dependencies must stay isolated.
- Licensing should remain friendly for DuckDB and Rust consumers.
  - Recommended default: `MIT` or `Apache-2.0 OR MIT`
  - Do not port LGPL implementation code from `python-AGS4`
  - Use AGS specifications, bundled dictionaries, and black-box behavior tests as references

## Acceptance Criteria
- A caller can parse an AGS4 document from a buffer and inspect groups, headings, and rows.
- A caller can serialize the document back to valid AGS text.
- A caller can validate raw text, parsed structure, and dictionary-linked semantics with line-level diagnostics.
- A caller can select a bundled dictionary version or load a custom dictionary.
- A caller can export group data to neutral table structures that work for DuckDB and other tabular adapters.
- A caller can derive WKT and WKB geometry from easting/northing pairs with configurable column mapping and CRS handling.
- A caller can merge multiple AGS files with deterministic conflict handling and provenance tracking.
- A Rust crate can wrap the C ABI without relying on unstable internal structs.
- The project ships fixtures and tests that cover both valid and invalid AGS samples.
- CI runs the automated test suite and treats failing core tests as release blockers.

## Milestones

### Task 1: Project foundation and ABI rules
- [x] Create the repository layout for core, validation, dictionary, tabular, FFI, tests, and tools.
- [x] Define coding standards, error-code conventions, and allocator strategy.
- [x] Define opaque handle types and ABI versioning rules.
- [x] Choose the project license and document reuse constraints from reference implementations.
- [x] Add build system support for static and shared library targets.
- [x] Add the initial test harness and CI entry points.

### Task 2: Tokenizer and parser
- [x] Implement a tokenizer for quoted AGS rows and descriptors.
- [x] Implement strict parsing for `GROUP`, `HEADING`, `UNIT`, `TYPE`, and `DATA` rows.
- [x] Preserve source line numbers and group order.
- [x] Detect duplicate group names and malformed row shapes.
- [ ] Add tolerant parsing mode with best-effort recovery and diagnostics.
- [ ] Add parser unit tests and malformed-input regression fixtures.

### Task 3: Core document model
- [ ] Define document, group, heading, row, and cell structures behind opaque handles.
- [ ] Implement stable iteration APIs for groups, headings, rows, and cells.
- [ ] Implement lookup APIs by group name and heading name.
- [ ] Implement deterministic memory ownership and destruction semantics.
- [ ] Add unit tests for ownership, iteration, and lookup behavior.

### Task 4: Serialization
- [x] Implement canonical AGS serialization from the document model.
- [x] Implement configurable newline output with canonical CRLF by default.
- [x] Implement correct escaping for embedded quotes.
- [ ] Add round-trip tests for valid fixture files.
- [ ] Add golden serialization tests for newline and escaping behavior.

### Task 5: Bundled dictionary support
- [x] Import and bundle standard dictionaries `4.0.3`, `4.0.4`, `4.1`, and `4.1.1`.
- [x] Implement dictionary loading from bundled assets, file paths, and memory buffers.
- [x] Implement dictionary merging with file-local `DICT` groups.
- [x] Implement lookup helpers for required fields, key fields, parent groups, and heading order.
- [x] Add tests for dictionary resolution, overrides, and merge precedence.

### Task 6: Raw-text validation
- [x] Implement ASCII validation.
- [x] Implement CRLF validation.
- [x] Implement descriptor validation for `GROUP`, `HEADING`, `UNIT`, `TYPE`, and `DATA`.
- [x] Implement row-shape and field-count validation.
- [x] Implement quote and escaped-quote validation.
- [x] Add fixture-based tests for each raw-text rule and expected diagnostics.

### Task 7: Parsed-structure validation
- [x] Implement group-name validation.
- [x] Implement heading-name validation.
- [x] Implement duplicate-heading detection.
- [x] Implement heading-prefix and cross-group heading reuse rules.
- [x] Implement AGS `TYPE` conformance checks for numeric, date/time, scientific, significant-figure, yes/no, text, and unit-coded fields.
- [x] Add positive and negative tests for every parsed-structure rule.

### Task 8: Dictionary-aware validation
- [x] Implement standard and user-defined heading existence checks.
- [x] Implement required-group checks for `PROJ`, `TRAN`, `TYPE`, `UNIT`, and `ABBR` semantics.
- [x] Implement heading-order checks against the effective dictionary.
- [x] Implement required-field checks.
- [x] Implement key uniqueness checks.
- [x] Implement parent-child key linkage checks.
- [x] Implement record-link validation for `RL` fields.
- [x] Implement `UNIT`, `TYPE`, and `ABBR` reference validation.
- [x] Add fixture suites covering each bundled dictionary version and custom `DICT` extensions.

### Task 9: Tabular APIs and transformations
- [x] Implement export of AGS groups to table-oriented structures.
- [x] Implement import from table-oriented structures back to AGS documents.
- [x] Implement neutral adapters or export helpers for DuckDB-facing and other tabular formats.
- [x] Implement numeric conversion helpers for compatible columns.
- [x] Implement numeric-to-text reformatting using dictionary metadata.
- [x] Implement group sorting strategies: input, alphabetical, dictionary, and hierarchical.
- [x] Implement duplicate-heading handling policies.
- [x] Implement automatic WKT and WKB derivation from easting/northing pairs.
- [x] Implement configurable coordinate-column detection, geometry-column naming, and CRS or SRID metadata handling.
- [x] Add integration tests for group export/import, sorting, numeric/text conversion, and geometry derivation.

### Task 10: Smart AGS merge
- [x] Implement multi-document merge planning using effective dictionary and key-field metadata.
- [x] Implement singleton metadata merge policies for `PROJ`, `TRAN`, `TYPE`, `UNIT`, `ABBR`, and `DICT`.
- [x] Implement keyed row deduplication and conflict detection for regular data groups.
- [x] Implement provenance tracking for merged rows and diagnostics.
- [x] Expose merge conflict-resolution policies and caller-supplied hooks.
- [x] Add merge fixtures and regression tests for conflict and deduplication behavior.

### Task 11: DuckDB and Rust integration surface
- [x] Design a minimal C API specifically for FFI-safe consumption.
- [x] Add zero-copy string-view APIs where lifetimes are well-defined.
- [x] Add row-iterator and column-export helpers for DuckDB ingestion.
- [x] Add headers and examples for Rust binding generation.
- [x] Add examples showing group-to-table and geometry export into DuckDB-style structures.
- [x] Add ABI, lifetime, and smoke tests for Rust and DuckDB-facing entry points.

### Task 12: CLI and tabular adapters
- [ ] Implement `agsctl check`.
- [ ] Implement `agsctl sort`.
- [ ] Implement `agsctl format` and `agsctl parse` for debugging and tests.
- [ ] Implement `agsctl convert` for supported tabular formats built on the neutral table abstraction.
- [ ] Define the optional tabular adapter boundary so the core stays dependency-light.
- [ ] Add CLI integration tests and optional-adapter tests gated by build configuration.

### Task 13: Testing, fixtures, and benchmarks
- [ ] Add valid and invalid AGS fixtures covering all supported dictionary versions.
- [ ] Add golden tests for diagnostics and serialization.
- [ ] Add round-trip tests for parse and serialize behavior.
- [ ] Add validation parity tests against known behavior from the reference libraries where legally safe.
- [ ] Add fuzz tests for tokenizer and parser robustness.
- [ ] Add geometry and merge-specific regression fixtures.
- [ ] Add performance benchmarks for large AGS files and streaming parse mode.

## Implementation Order
Recommended delivery order:

1. Tasks 1 through 4
2. Task 5
3. Tasks 6 through 8
4. Tasks 9 through 11
5. Tasks 12 and 13

## Open Decisions
- [ ] Decide whether the first release exposes mutable document editing APIs or starts read-mostly.
- [ ] Decide whether tolerant parsing should create placeholder cells for malformed rows or drop them.
- [ ] Decide the exact table abstraction for DuckDB-facing export.
- [ ] Decide which tabular adapters ship in the first release beyond the neutral core table abstraction.
- [ ] Decide the default heuristics and override model for easting/northing to geometry detection.
- [ ] Decide the default merge conflict policy for singleton groups and keyed rows.
- [ ] Decide whether the Rust crate lives in-repo or in a separate repository.
