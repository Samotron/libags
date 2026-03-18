# Spec 08: GitHub Actions Builds and Releases

## Scope
Add practical GitHub Actions automation for `libags` so pushes and pull requests get cross-platform build and test coverage, examples get smoke-checked, and tagged builds can produce release artifacts.

## Deliverables
- expanded `ci` workflow with cross-platform build and test coverage
- example smoke-check job for the DuckDB-style C example
- release-artifact workflow for manual and tag-driven packaging
- CMake install and packaging support needed by the workflows
- spec and verification notes kept in sync with the implementation

## Tasks

### Task 1: Build matrix
- [x] Expand CI to build and test on Linux, macOS, and Windows.
- [x] Cover both `Debug` and `Release` configurations where practical.
- [x] Keep pull request and push execution paths simple and deterministic.

### Task 2: Packaging support
- [x] Add install rules for headers and library artifacts.
- [x] Resolve Windows output-name collisions between static and shared builds.
- [x] Make install output usable as a release artifact payload.

### Task 3: Example and smoke coverage
- [x] Add a workflow job that smoke-checks the DuckDB-style example.
- [x] Keep the example check dependency-light and fast.

### Task 4: Release automation
- [x] Add a workflow for manual and tag-triggered release builds.
- [x] Produce per-platform release artifacts from the install tree.
- [x] Publish artifacts to the workflow run and attach them to GitHub releases on tags.

### Task 5: Verification
- [x] Verify the local CMake build still passes after workflow-related build-system changes.
- [x] Validate workflow YAML locally where feasible.
- [x] Update the spec checkboxes to match the implemented state.
