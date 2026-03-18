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
    "\"GROUP\",\"PROJ\"\r\n"
    "\"HEADING\",\"PROJ_ID\",\"PROJ_NAME\"\r\n"
    "\"UNIT\",\"\",\"\"\r\n"
    "\"TYPE\",\"ID\",\"X\"\r\n"
    "\"DATA\",\"P1\",\"Site Alpha\"\r\n"
    "\"GROUP\",\"LOCA\"\r\n"
    "\"HEADING\",\"LOCA_ID\",\"LOCA_NATE\",\"LOCA_NATN\"\r\n"
    "\"UNIT\",\"\",\"m\",\"m\"\r\n"
    "\"TYPE\",\"ID\",\"2DP\",\"2DP\"\r\n"
    "\"DATA\",\"L1\",\"123.45\",\"456.78\"\r\n"
    "\"DATA\",\"L2\",\"223.45\",\"556.78\"";
  ags_document *document = NULL;
  ags_table_collection *collection = NULL;
  fake_duckdb_sink sink = {0};
  size_t table_index = 0;

  if (ags_document_parse_buffer(sample, strlen(sample), NULL, &document) != AGS_STATUS_OK) {
    fprintf(stderr, "failed to parse sample\n");
    return 1;
  }

  if (ags_document_export_tables(document, NULL, &collection) != AGS_STATUS_OK) {
    fprintf(stderr, "failed to export document tables\n");
    ags_document_destroy(document);
    return 1;
  }

  for (table_index = 0; table_index < ags_table_collection_count(collection); ++table_index) {
    const ags_table *table = ags_table_collection_get(collection, table_index);
    ags_table_summary summary;
    ags_geometry_column *geometry = NULL;
    ags_geometry_export geometry_export;
    size_t column_index = 0;

    if (table == NULL || ags_table_collection_get_summary(collection, table_index, &summary) != AGS_STATUS_OK) {
      fprintf(stderr, "failed to summarize exported table\n");
      ags_table_collection_destroy(collection);
      ags_document_destroy(document);
      return 1;
    }

    printf("table %zu: %s columns=%zu rows=%zu numeric=%zu geometry_candidates=%zu\n",
           table_index,
           summary.group_name,
           summary.column_count,
           summary.row_count,
           summary.numeric_column_count,
           summary.geometry_candidate_count);

    for (column_index = 0; column_index < ags_table_column_count(table); ++column_index) {
      ags_column_summary column_summary;
      ags_table_column_export column_export;

      if (ags_table_get_column_summary(table, column_index, &column_summary) != AGS_STATUS_OK ||
          ags_table_get_column_export(table, column_index, &column_export) != AGS_STATUS_OK) {
        fprintf(stderr, "failed to export table column\n");
        ags_table_collection_destroy(collection);
        ags_document_destroy(document);
        return 1;
      }

      printf("  column %zu: %s class=%d non_null=%zu\n",
             column_index,
             column_summary.column_name,
             (int)column_summary.column_class,
             column_summary.non_null_count);
      fake_duckdb_append_varchar_column(&sink, column_export);
    }

    if (ags_table_derive_geometry(table, NULL, &geometry) == AGS_STATUS_OK && geometry != NULL &&
        ags_geometry_column_get_export(geometry, &geometry_export) == AGS_STATUS_OK) {
      fake_duckdb_append_wkt_column(&sink, &geometry_export);
    }

    ags_geometry_column_destroy(geometry);
  }

  printf("rows appended: %zu\n", sink.rows_appended);

  ags_table_collection_destroy(collection);
  ags_document_destroy(document);
  return 0;
}
