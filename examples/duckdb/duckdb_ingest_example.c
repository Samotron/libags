#include <stdio.h>
#include <string.h>

#include "libags/libags.h"

typedef struct fake_duckdb_sink {
  size_t rows_appended;
} fake_duckdb_sink;

static void fake_duckdb_append_varchar_column(
  fake_duckdb_sink *sink,
  ags_table_column_export column_export
) {
  size_t row_index = 0;

  printf(
    "column %.*s (%.*s / %.*s)\n",
    (int)column_export.column_name.length,
    column_export.column_name.data,
    (int)column_export.unit.length,
    column_export.unit.data,
    (int)column_export.type.length,
    column_export.type.data
  );

  for (row_index = 0; row_index < column_export.row_count; ++row_index) {
    printf("  row %zu = %s\n", row_index, column_export.values[row_index]);
  }

  sink->rows_appended = column_export.row_count;
}

static void fake_duckdb_append_wkt_column(
  fake_duckdb_sink *sink,
  const ags_geometry_export *geometry_export
) {
  size_t row_index = 0;

  if (geometry_export->encoding != AGS_GEOMETRY_WKT) {
    return;
  }

  printf(
    "geometry %.*s srid=%d\n",
    (int)geometry_export->column_name.length,
    geometry_export->column_name.data,
    geometry_export->srid
  );

  for (row_index = 0; row_index < geometry_export->row_count; ++row_index) {
    if (geometry_export->is_null[row_index] != 0) {
      printf("  row %zu = NULL\n", row_index);
    } else {
      printf("  row %zu = %s\n", row_index, geometry_export->wkt_values[row_index]);
    }
  }

  sink->rows_appended = geometry_export->row_count;
}

int main(void) {
  static const char *sample =
    "\"GROUP\",\"LOCA\"\r\n"
    "\"HEADING\",\"LOCA_ID\",\"LOCA_NATE\",\"LOCA_NATN\"\r\n"
    "\"UNIT\",\"\",\"m\",\"m\"\r\n"
    "\"TYPE\",\"ID\",\"2DP\",\"2DP\"\r\n"
    "\"DATA\",\"L1\",\"123.45\",\"456.78\"\r\n"
    "\"DATA\",\"L2\",\"223.45\",\"556.78\"";
  ags_document *document = NULL;
  ags_table *table = NULL;
  ags_geometry_column *geometry = NULL;
  ags_table_column_export easting_export;
  ags_table_column_export northing_export;
  ags_geometry_export geometry_export;
  fake_duckdb_sink sink = {0};

  if (ags_document_parse_buffer(sample, strlen(sample), NULL, &document) != AGS_STATUS_OK) {
    fprintf(stderr, "failed to parse sample\n");
    return 1;
  }

  if (ags_table_from_group(document, 0, NULL, &table) != AGS_STATUS_OK) {
    fprintf(stderr, "failed to export LOCA table\n");
    ags_document_destroy(document);
    return 1;
  }

  if (ags_table_get_column_export(table, 1, &easting_export) != AGS_STATUS_OK ||
      ags_table_get_column_export(table, 2, &northing_export) != AGS_STATUS_OK) {
    fprintf(stderr, "failed to export coordinate columns\n");
    ags_table_destroy(table);
    ags_document_destroy(document);
    return 1;
  }

  fake_duckdb_append_varchar_column(&sink, easting_export);
  fake_duckdb_append_varchar_column(&sink, northing_export);

  if (ags_table_derive_geometry(table, NULL, &geometry) != AGS_STATUS_OK) {
    fprintf(stderr, "failed to derive geometry\n");
    ags_table_destroy(table);
    ags_document_destroy(document);
    return 1;
  }

  if (ags_geometry_column_get_export(geometry, &geometry_export) != AGS_STATUS_OK) {
    fprintf(stderr, "failed to export geometry\n");
    ags_geometry_column_destroy(geometry);
    ags_table_destroy(table);
    ags_document_destroy(document);
    return 1;
  }

  fake_duckdb_append_wkt_column(&sink, &geometry_export);
  printf("rows appended: %zu\n", sink.rows_appended);

  ags_geometry_column_destroy(geometry);
  ags_table_destroy(table);
  ags_document_destroy(document);
  return 0;
}
