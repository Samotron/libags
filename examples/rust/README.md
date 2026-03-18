# Rust Binding Notes

`libags` exposes a dedicated `ffi.h` layer for binding generation. The borrowed view APIs in that header are intended to be wrapped as Rust references or slices whose lifetime is tied to the owning `ags_document`, `ags_table`, `ags_numeric_column`, or `ags_geometry_column`.

## Generate bindings

```bash
bindgen include/libags/ffi.h \
  --allowlist-function 'ags_.*' \
  --allowlist-type 'ags_.*' \
  --allowlist-var 'AGS_.*' \
  -- -Iinclude > bindings.rs
```

## Wrapper shape

- Treat `ags_string_view` as `&str` after validating UTF-8.
- Treat `ags_bytes_view` as `&[u8]`.
- Keep owner objects alive while any borrowed view or export struct is in use.
- Use `ags_row_cursor` for row-wise iteration and `ags_table_get_column_export` or `ags_geometry_column_get_export` for column-wise ingestion.

See [example.rs](/home/samotron/dev/libags/examples/rust/example.rs) for a minimal wrapper pattern.
