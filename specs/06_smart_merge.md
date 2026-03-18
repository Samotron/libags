# Spec 06: Smart AGS Merge

## Scope
Implement the first end-to-end merge layer for `libags`:

- merge multiple parsed AGS documents into one logical output document
- use effective-dictionary key metadata when available
- support configurable singleton and keyed-row conflict policies
- preserve row provenance across merged output
- emit diagnostics for merge conflicts, incompatible custom dictionary definitions, and unresolved parent-child relationships after merge

## Deliverables
- public `merge` API header and umbrella include
- merge result handle containing merged document, diagnostics, and provenance
- internal keyed merge and singleton-group merge implementation
- validation-backed post-merge parent-child diagnostics
- fixture-backed tests for keyed merge, singleton policies, callback handling, provenance, and merge diagnostics

## Tasks

### Task 1: Public API and build wiring
- [x] Add public `merge` header and umbrella include.
- [x] Add merge options, conflict policies, callback types, result handle, and accessors.
- [x] Wire the merge implementation into the build.

### Task 2: Dictionary-aware merge planning
- [x] Resolve the standard dictionary from override, `TRAN_AGS`, or bundled fallback.
- [x] Build an effective dictionary that includes custom `DICT` rows from all inputs.
- [x] Detect key fields and parent-group metadata for merge decisions.

### Task 3: Merge execution
- [x] Merge groups by name into one output document.
- [x] Merge keyed rows using dictionary key fields when available.
- [x] Apply the singleton metadata policy across `PROJ`, `TRAN`, `TYPE`, `UNIT`, `ABBR`, and `DICT`, with true single-row enforcement for `PROJ` and `TRAN`.
- [x] Support `fail`, `keep first`, `keep last`, `merge non-empty`, and callback conflict handling.
- [x] Preserve schema unions when later inputs add headings.

### Task 4: Provenance and diagnostics
- [x] Track source document index and source line number for each merged output row.
- [x] Emit diagnostics for keyed-row conflicts.
- [x] Emit diagnostics for incompatible custom `DICT` definitions.
- [x] Emit diagnostics for unresolved parent-child links after merge.

### Task 5: Tests
- [x] Add merge fixtures for compatible merges, conflicting singletons, missing parents, and conflicting custom dictionaries.
- [x] Add tests for keyed merge behavior and provenance accessors.
- [x] Add tests for singleton policies and callback-based conflict resolution.
- [x] Add tests for merge diagnostics.
- [x] Keep the full suite green.
