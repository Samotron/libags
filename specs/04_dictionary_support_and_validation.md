# Spec 04: Dictionary Support and Dictionary-Aware Validation

## Scope
Implement bundled AGS4 dictionary support and the first dictionary-aware validation slice:

- bundled standard dictionaries for `4.0.3`, `4.0.4`, `4.1`, and `4.1.1`
- dictionary version resolution from override, `TRAN_AGS`, and fallback
- merging bundled dictionary definitions with file-local `DICT`
- dictionary-aware validation for heading presence/order, required groups/fields, keys, parent-child links, record links, and `UNIT`/`TYPE`/`ABBR` references

## Deliverables
- public dictionary API
- generated bundled dictionary data in the source tree
- internal effective-dictionary model used by validation
- fixture-backed tests for bundled dictionary loading and dictionary-aware validation

## Tasks

### Task 1: Dictionary API and bundled assets
- [x] Add public dictionary header and umbrella include.
- [x] Add runtime helpers for latest version and supported-version checks.
- [x] Add bundled dictionary loading by version.
- [x] Generate bundled dictionary source data for `4.0.3`, `4.0.4`, `4.1`, and `4.1.1`.

### Task 2: Effective dictionary resolution
- [x] Resolve dictionary version from explicit override, `TRAN_AGS`, or latest bundled version.
- [x] Parse dictionary metadata from bundled and file-local `DICT` groups.
- [x] Merge standard and custom dictionary definitions with deterministic precedence.
- [x] Expose enough internal lookup support for validation rules.

### Task 3: Dictionary-aware validation
- [x] Validate heading existence in standard or merged custom dictionary.
- [x] Validate heading order against dictionary order.
- [x] Validate required groups and singleton semantics for `PROJ` and `TRAN`, plus presence of `TYPE` and `UNIT`.
- [x] Validate required fields using `DICT_STAT`.
- [x] Validate key uniqueness.
- [x] Validate parent-child key linkage using `DICT_PGRP`.
- [x] Validate record-link (`RL`) fields.
- [x] Validate `UNIT`, `TYPE`, and `ABBR` references.

### Task 4: Tests
- [x] Add tests for bundled dictionary loading and version resolution.
- [x] Add fixtures for custom `DICT`, missing required fields, key clashes, bad parent links, bad record links, and bad `UNIT`/`TYPE`/`ABBR` references.
- [x] Keep the full test suite green.
