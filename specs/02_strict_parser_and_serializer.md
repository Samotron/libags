# Spec 02: Strict Parser and Serializer

## Scope
Implement the first usable AGS document workflow:

- strict parsing from memory and file input
- ordered in-memory storage for groups, headings, units, types, and rows
- document inspection APIs
- canonical serialization back to AGS text
- parser and round-trip tests

This slice is intentionally strict-first. It does not yet add tolerant parsing or full validation diagnostics.

## Deliverables
- strict tokenizer for quoted AGS rows
- strict parser for `GROUP`, `HEADING`, `UNIT`, `TYPE`, and `DATA`
- line-number preservation for parsed rows
- document inspection APIs by index and group name
- serializer with CRLF and LF output modes
- file and buffer parsing entry points
- round-trip and malformed-input tests

## Tasks

### Task 1: Public API extensions
- [x] Add parse entry points for buffer and file input.
- [x] Add inspection APIs for groups, fields, rows, cells, and line numbers.
- [x] Add serialization APIs and newline-mode options.
- [x] Add buffer-free helper for serializer output.

### Task 2: Internal document model
- [x] Store ordered groups, fields, and rows in the document handle.
- [x] Store heading name, unit, and type per field.
- [x] Store line numbers for group, heading, unit, type, and data rows.
- [x] Add internal cleanup helpers for partially-built documents.

### Task 3: Strict tokenizer and parser
- [x] Implement quoted-field tokenization with escaped quote handling.
- [x] Reject malformed quoting and malformed separators.
- [x] Parse `GROUP` row shape strictly.
- [x] Parse `HEADING`, `UNIT`, `TYPE`, and `DATA` rows with field-count checks.
- [x] Reject duplicate group names.
- [x] Support both CRLF and LF input line endings.

### Task 4: Serializer
- [x] Serialize parsed documents to canonical AGS text.
- [x] Support CRLF output by default and LF output for tests.
- [x] Escape embedded quotes correctly.

### Task 5: Tests
- [x] Add tests for strict parsing from buffer.
- [x] Add tests for parsing from file.
- [x] Add tests for duplicate-group and malformed-row failures.
- [x] Add round-trip tests for parse and serialize behavior.
