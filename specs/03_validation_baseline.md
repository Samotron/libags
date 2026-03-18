# Spec 03: Validation Baseline

## Scope
Implement the first validation slice for `libags`:

- public diagnostics and validation report APIs
- raw-text validation over AGS input buffers
- parsed-structure validation over `ags_document`
- fixture-backed tests for the implemented rules

This slice does not yet add dictionary-aware validation.

## Deliverables
- validation report handle with enumerated diagnostics
- raw validation entry point
- parsed validation entry point
- rule ids, severities, line numbers, group names, and field names in diagnostics where available
- fixture files for representative valid and invalid inputs

## Tasks

### Task 1: Public validation API
- [x] Add public validation header and umbrella include.
- [x] Add validation report lifecycle and accessors.
- [x] Add validation options with allocator support.
- [x] Add raw-text and parsed-document validation entry points.

### Task 2: Raw-text validation
- [x] Implement ASCII validation.
- [x] Implement CRLF validation.
- [x] Implement descriptor validation.
- [x] Implement `GROUP` row shape validation.
- [x] Implement `HEADING`/`UNIT`/`TYPE`/`DATA` field-count checks.
- [x] Implement malformed-quote and malformed-separator validation.

### Task 3: Parsed-structure validation
- [x] Implement group-name validation.
- [x] Implement heading-name validation.
- [x] Implement duplicate-heading detection.
- [x] Implement heading-prefix and cross-group heading reuse checks.
- [x] Implement baseline `TYPE` conformance checks for empty, text, `YN`, `T`, `DT`, `DP`, `SCI`, and `SF` values.

### Task 4: Tests
- [x] Add fixture files for valid and invalid raw inputs.
- [x] Add tests for raw-text validation diagnostics.
- [x] Add tests for parsed-structure validation diagnostics.
- [x] Run the full test suite and keep it green.
