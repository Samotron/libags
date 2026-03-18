#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libags/libags.h"

static int tests_run = 0;
static int tests_failed = 0;

#define EXPECT_TRUE(expr) do { \
  if (!(expr)) { \
    fprintf(stderr, "EXPECT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; \
  } \
} while (0)

#define EXPECT_EQ_INT(expected, actual) do { \
  int expected_value = (expected); \
  int actual_value = (actual); \
  if (expected_value != actual_value) { \
    fprintf(stderr, "EXPECT_EQ_INT failed at %s:%d: expected=%d actual=%d\n", __FILE__, __LINE__, expected_value, actual_value); \
    return 1; \
  } \
} while (0)

#define EXPECT_EQ_SIZE(expected, actual) do { \
  size_t expected_value = (expected); \
  size_t actual_value = (actual); \
  if (expected_value != actual_value) { \
    fprintf(stderr, "EXPECT_EQ_SIZE failed at %s:%d: expected=%zu actual=%zu\n", __FILE__, __LINE__, expected_value, actual_value); \
    return 1; \
  } \
} while (0)

#define EXPECT_STREQ(expected, actual) do { \
  const char *expected_value = (expected); \
  const char *actual_value = (actual); \
  if (strcmp(expected_value, actual_value) != 0) { \
    fprintf(stderr, "EXPECT_STREQ failed at %s:%d: expected=%s actual=%s\n", __FILE__, __LINE__, expected_value, actual_value); \
    return 1; \
  } \
} while (0)

#define RUN_TEST(fn) do { \
  int rc = 0; \
  ++tests_run; \
  rc = (fn)(); \
  if (rc != 0) { \
    ++tests_failed; \
  } \
} while (0)

static char *load_fixture(const char *relative_path, size_t *out_length) {
  char *buffer = NULL;
  FILE *file = NULL;
  long file_size = 0;
  size_t bytes_read = 0;

  file = fopen(relative_path, "rb");
  if (file == NULL) {
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }

  file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    return NULL;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }

  buffer = (char *)malloc((size_t)file_size + 1);
  if (buffer == NULL) {
    fclose(file);
    return NULL;
  }

  bytes_read = fread(buffer, 1, (size_t)file_size, file);
  if (fclose(file) != 0 || bytes_read != (size_t)file_size) {
    free(buffer);
    return NULL;
  }

  buffer[file_size] = '\0';

  if (out_length != NULL) {
    *out_length = (size_t)file_size;
  }

  return buffer;
}

static int report_has_rule(
  const ags_validation_report *report,
  const char *rule,
  ags_diagnostic_severity severity
) {
  size_t index = 0;

  for (index = 0; index < ags_validation_report_diagnostic_count(report); ++index) {
    const char *actual_rule = ags_validation_report_diagnostic_rule(report, index);
    if (actual_rule != NULL &&
        strcmp(actual_rule, rule) == 0 &&
        ags_validation_report_diagnostic_severity(report, index) == severity) {
      return 1;
    }
  }

  return 0;
}

static ags_status parse_document_fixture(const char *relative_path, ags_document **out_document) {
  char *input = NULL;
  size_t length = 0;
  ags_status status = AGS_STATUS_OK;

  if (out_document == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  *out_document = NULL;
  input = load_fixture(relative_path, &length);
  if (input == NULL) {
    return AGS_STATUS_IO_ERROR;
  }

  status = ags_document_parse_buffer(input, length, NULL, out_document);
  free(input);
  return status;
}

static int merge_result_has_diagnostic(
  const ags_merge_result *result,
  const char *group_name,
  ags_diagnostic_severity severity,
  const char *message_substring
) {
  size_t index = 0;

  for (index = 0; index < ags_merge_result_diagnostic_count(result); ++index) {
    const char *group = ags_merge_result_diagnostic_group(result, index);
    const char *message = ags_merge_result_diagnostic_message(result, index);
    if (ags_merge_result_diagnostic_severity(result, index) == severity &&
        message != NULL &&
        strstr(message, message_substring) != NULL &&
        ((group_name == NULL && group == NULL) ||
         (group_name != NULL && group != NULL && strcmp(group_name, group) == 0))) {
      return 1;
    }
  }

  return 0;
}

static int string_view_matches(ags_string_view view, const char *expected) {
  size_t length = 0;

  if (expected == NULL) {
    return view.data == NULL && view.length == 0;
  }

  if (view.data == NULL) {
    return 0;
  }

  length = strlen(expected);
  return view.length == length && memcmp(view.data, expected, length) == 0;
}

static int bytes_view_matches(ags_bytes_view view, const unsigned char *expected, size_t expected_length) {
  if (view.data == NULL || expected == NULL) {
    return 0;
  }

  return view.length == expected_length && memcmp(view.data, expected, expected_length) == 0;
}

struct test_allocator_state {
  int malloc_calls;
  int realloc_calls;
  int free_calls;
};

static void *test_malloc(void *user_data, size_t size) {
  struct test_allocator_state *state = (struct test_allocator_state *)user_data;
  state->malloc_calls += 1;
  return malloc(size);
}

static void *test_realloc(void *user_data, void *ptr, size_t size) {
  struct test_allocator_state *state = (struct test_allocator_state *)user_data;
  state->realloc_calls += 1;
  return realloc(ptr, size);
}

static void test_free(void *user_data, void *ptr) {
  struct test_allocator_state *state = (struct test_allocator_state *)user_data;
  state->free_calls += 1;
  free(ptr);
}

static int test_version_queries(void) {
  EXPECT_EQ_INT(AGS_VERSION_MAJOR, ags_version_major());
  EXPECT_EQ_INT(AGS_VERSION_MINOR, ags_version_minor());
  EXPECT_EQ_INT(AGS_VERSION_PATCH, ags_version_patch());
  EXPECT_STREQ("0.1.0", ags_version_string());
  EXPECT_TRUE(ags_abi_version() == AGS_ABI_VERSION);
  return 0;
}

static int test_status_helpers(void) {
  EXPECT_TRUE(ags_status_is_success(AGS_STATUS_OK));
  EXPECT_TRUE(!ags_status_is_success(AGS_STATUS_INVALID_ARGUMENT));
  EXPECT_STREQ("ok", ags_status_string(AGS_STATUS_OK));
  EXPECT_STREQ("invalid argument", ags_status_string(AGS_STATUS_INVALID_ARGUMENT));
  EXPECT_STREQ("parse error", ags_status_string(AGS_STATUS_PARSE_ERROR));
  EXPECT_STREQ("unknown status code", ags_status_string((ags_status)999));
  return 0;
}

static int test_default_allocator(void) {
  const ags_allocator *allocator = ags_default_allocator();
  EXPECT_TRUE(allocator != NULL);
  EXPECT_TRUE(ags_allocator_is_valid(allocator));
  return 0;
}

static int test_document_options_init(void) {
  ags_document_options options;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_options_init(&options));
  EXPECT_EQ_SIZE(sizeof(options), options.struct_size);
  EXPECT_TRUE(options.allocator == NULL);
  EXPECT_EQ_INT(AGS_STATUS_INVALID_ARGUMENT, ags_document_options_init(NULL));
  return 0;
}

static int test_document_create_and_destroy_default_allocator(void) {
  ags_document *document = NULL;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_create(NULL, &document));
  EXPECT_TRUE(document != NULL);
  EXPECT_EQ_SIZE(0u, ags_document_group_count(document));
  EXPECT_TRUE(ags_document_allocator(document) != NULL);
  ags_document_destroy(document);
  ags_document_destroy(NULL);
  return 0;
}

static int test_document_create_rejects_invalid_arguments(void) {
  ags_document *document = NULL;
  ags_document_options options;

  EXPECT_EQ_INT(AGS_STATUS_INVALID_ARGUMENT, ags_document_create(NULL, NULL));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_options_init(&options));
  options.struct_size = 0;

  EXPECT_EQ_INT(AGS_STATUS_INVALID_ARGUMENT, ags_document_create(&options, &document));
  EXPECT_TRUE(document == NULL);
  return 0;
}

static int test_document_uses_custom_allocator(void) {
  struct test_allocator_state state = {0, 0, 0};
  ags_allocator allocator = {test_malloc, test_realloc, test_free, &state};
  ags_document_options options;
  ags_document *document = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_options_init(&options));
  options.allocator = &allocator;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_create(&options, &document));
  EXPECT_TRUE(document != NULL);
  EXPECT_EQ_INT(1, state.malloc_calls);
  EXPECT_EQ_INT(0, state.realloc_calls);
  EXPECT_EQ_INT(0, state.free_calls);
  EXPECT_TRUE(ags_document_allocator(document) != &allocator);

  ags_document_destroy(document);
  EXPECT_EQ_INT(1, state.free_calls);
  return 0;
}

static const char *sample_ags_crlf =
  "\"GROUP\",\"PROJ\"\r\n"
  "\"HEADING\",\"PROJ_ID\",\"PROJ_NAME\"\r\n"
  "\"UNIT\",\"\",\"\"\r\n"
  "\"TYPE\",\"ID\",\"X\"\r\n"
  "\"DATA\",\"P1\",\"Site \"\"Alpha\"\"\"\r\n"
  "\"GROUP\",\"LOCA\"\r\n"
  "\"HEADING\",\"LOCA_ID\",\"LOCA_NATE\",\"LOCA_NATN\"\r\n"
  "\"UNIT\",\"\",\"m\",\"m\"\r\n"
  "\"TYPE\",\"ID\",\"2DP\",\"2DP\"\r\n"
  "\"DATA\",\"L1\",\"123.45\",\"456.78\"\r\n"
  "\"DATA\",\"L2\",\"223.45\",\"556.78\"";

static int test_parse_buffer_and_inspect_document(void) {
  ags_document *document = NULL;
  size_t proj_index = 0;
  size_t loca_index = 0;
  const char *proj_name = NULL;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );
  EXPECT_TRUE(document != NULL);
  EXPECT_EQ_SIZE(2u, ags_document_group_count(document));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(document, "PROJ", &proj_index));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(document, "LOCA", &loca_index));
  EXPECT_EQ_INT(AGS_STATUS_NOT_FOUND, ags_document_find_group(document, "MISS", &loca_index));

  proj_name = ags_document_group_name(document, proj_index);
  EXPECT_TRUE(proj_name != NULL);
  EXPECT_STREQ("PROJ", proj_name);
  EXPECT_EQ_SIZE(1u, ags_document_group_line_number(document, proj_index));
  EXPECT_EQ_SIZE(2u, ags_document_group_heading_line_number(document, proj_index));
  EXPECT_EQ_SIZE(3u, ags_document_group_unit_line_number(document, proj_index));
  EXPECT_EQ_SIZE(4u, ags_document_group_type_line_number(document, proj_index));
  EXPECT_EQ_SIZE(2u, ags_document_group_field_count(document, proj_index));
  EXPECT_EQ_SIZE(1u, ags_document_group_row_count(document, proj_index));
  EXPECT_STREQ("PROJ_ID", ags_document_field_name(document, proj_index, 0));
  EXPECT_STREQ("", ags_document_field_unit(document, proj_index, 0));
  EXPECT_STREQ("ID", ags_document_field_type(document, proj_index, 0));
  EXPECT_EQ_SIZE(5u, ags_document_row_line_number(document, proj_index, 0));
  EXPECT_STREQ("P1", ags_document_cell_value(document, proj_index, 0, 0));
  EXPECT_STREQ("Site \"Alpha\"", ags_document_cell_value(document, proj_index, 0, 1));

  EXPECT_EQ_SIZE(6u, ags_document_group_line_number(document, loca_index));
  EXPECT_EQ_SIZE(2u, ags_document_group_row_count(document, loca_index));
  EXPECT_STREQ("LOCA_NATE", ags_document_field_name(document, loca_index, 1));
  EXPECT_STREQ("2DP", ags_document_field_type(document, loca_index, 2));
  EXPECT_STREQ("223.45", ags_document_cell_value(document, loca_index, 1, 1));

  ags_document_destroy(document);
  return 0;
}

static int test_parse_lf_input(void) {
  const char *input =
    "\"GROUP\",\"PROJ\"\n"
    "\"HEADING\",\"PROJ_ID\"\n"
    "\"UNIT\",\"\"\n"
    "\"TYPE\",\"ID\"\n"
    "\"DATA\",\"P1\"";
  ags_document *document = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, strlen(input), NULL, &document));
  EXPECT_EQ_SIZE(1u, ags_document_group_count(document));
  EXPECT_EQ_SIZE(1u, ags_document_group_row_count(document, 0));
  ags_document_destroy(document);
  return 0;
}

static int test_parse_file(void) {
  const char *path = "libags_test_input.ags";
  FILE *file = fopen(path, "wb");
  ags_document *document = NULL;

  EXPECT_TRUE(file != NULL);
  EXPECT_EQ_SIZE(strlen(sample_ags_crlf), fwrite(sample_ags_crlf, 1, strlen(sample_ags_crlf), file));
  EXPECT_EQ_INT(0, fclose(file));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_file(path, NULL, &document));
  EXPECT_EQ_SIZE(2u, ags_document_group_count(document));
  ags_document_destroy(document);

  EXPECT_EQ_INT(0, remove(path));
  return 0;
}

static int test_parse_rejects_duplicate_groups(void) {
  const char *input =
    "\"GROUP\",\"PROJ\"\n"
    "\"HEADING\",\"PROJ_ID\"\n"
    "\"UNIT\",\"\"\n"
    "\"TYPE\",\"ID\"\n"
    "\"GROUP\",\"PROJ\"\n"
    "\"HEADING\",\"PROJ_ID\"\n"
    "\"UNIT\",\"\"\n"
    "\"TYPE\",\"ID\"";
  ags_document *document = NULL;

  EXPECT_EQ_INT(AGS_STATUS_PARSE_ERROR, ags_document_parse_buffer(input, strlen(input), NULL, &document));
  EXPECT_TRUE(document == NULL);
  return 0;
}

static int test_parse_rejects_malformed_row_shape(void) {
  const char *input =
    "\"GROUP\",\"PROJ\"\n"
    "\"HEADING\",\"PROJ_ID\",\"PROJ_NAME\"\n"
    "\"UNIT\",\"\"\n"
    "\"TYPE\",\"ID\",\"X\"\n"
    "\"DATA\",\"P1\",\"Name\"";
  ags_document *document = NULL;

  EXPECT_EQ_INT(AGS_STATUS_PARSE_ERROR, ags_document_parse_buffer(input, strlen(input), NULL, &document));
  EXPECT_TRUE(document == NULL);
  return 0;
}

static int test_serialize_and_round_trip(void) {
  ags_document *document = NULL;
  ags_document *round_trip = NULL;
  ags_serialize_options options;
  char *serialized = NULL;
  size_t serialized_length = 0;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_serialize_options_init(&options));
  options.newline_mode = AGS_NEWLINE_LF;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_serialize(document, &options, &serialized, &serialized_length)
  );
  EXPECT_TRUE(serialized != NULL);
  EXPECT_TRUE(serialized_length > 0);
  EXPECT_TRUE(strstr(serialized, "\r\n") == NULL);
  EXPECT_TRUE(strstr(serialized, "\"Site \"\"Alpha\"\"\"") != NULL);

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(serialized, serialized_length, NULL, &round_trip)
  );
  EXPECT_EQ_SIZE(2u, ags_document_group_count(round_trip));
  EXPECT_STREQ("556.78", ags_document_cell_value(round_trip, 1, 1, 2));

  ags_document_free_buffer(document, serialized);
  ags_document_destroy(round_trip);
  ags_document_destroy(document);
  return 0;
}

static int test_validate_text_with_clean_fixture(void) {
  char *input = NULL;
  size_t length = 0;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/valid_basic.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_text(input, length, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_EQ_SIZE(0u, ags_validation_report_diagnostic_count(report));

  ags_validation_report_destroy(report);
  free(input);
  return 0;
}

static int test_validate_text_with_invalid_fixture(void) {
  char *input = NULL;
  size_t length = 0;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/invalid_raw.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_text(input, length, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(ags_validation_report_diagnostic_count(report) >= 5u);
  EXPECT_TRUE(report_has_rule(report, "1", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "2a", AGS_DIAGNOSTIC_WARNING));
  EXPECT_TRUE(report_has_rule(report, "3", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "4.1", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "4.2", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "5", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  free(input);
  return 0;
}

static int test_validate_document_with_invalid_fixture(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/invalid_parsed_duplicate_heading.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(ags_validation_report_diagnostic_count(report) >= 5u);
  EXPECT_TRUE(report_has_rule(report, "19a", AGS_DIAGNOSTIC_WARNING));
  EXPECT_TRUE(report_has_rule(report, "7", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "19b", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "8", AGS_DIAGNOSTIC_ERROR));

  EXPECT_STREQ("AB12", ags_validation_report_diagnostic_group(report, 0));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_clean_sample(void) {
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_EQ_SIZE(0u, ags_validation_report_diagnostic_count(report));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  return 0;
}

static int test_validate_file_helpers(void) {
  ags_validation_report *report = NULL;
  ags_document *dictionary_document = NULL;
  ags_validate_options options;
  ags_validation_diagnostic_export diagnostic_export;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_file("tests/fixtures/valid_basic.ags", NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_EQ_SIZE(0u, ags_validation_report_diagnostic_count(report));
  ags_validation_report_destroy(report);
  report = NULL;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_validate_file("tests/fixtures/invalid_raw.ags", NULL, &report)
  );
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(ags_validation_report_diagnostic_count(report) > 0u);
  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_validation_report_get_diagnostic_export(report, 0u, &diagnostic_export)
  );
  EXPECT_TRUE(diagnostic_export.line_number > 0u);
  EXPECT_TRUE(diagnostic_export.message.data != NULL);
  ags_validation_report_destroy(report);
  report = NULL;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_validate_file_with_dictionary("tests/fixtures/dictionary_custom_valid.ags", NULL, &report)
  );
  EXPECT_TRUE(report != NULL);
  EXPECT_EQ_SIZE(0u, ags_validation_report_diagnostic_count(report));
  ags_validation_report_destroy(report);
  report = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_options_init(&options));
  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_dictionary_load_file("tests/fixtures/dictionary_custom_valid.ags", NULL, &dictionary_document)
  );
  options.dictionary_document = dictionary_document;
  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_validate_file_with_dictionary("tests/fixtures/dictionary_invalid_heading_rules.ags", &options, &report)
  );
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(ags_validation_report_diagnostic_count(report) > 0u);
  EXPECT_TRUE(report_has_rule(report, "9", AGS_DIAGNOSTIC_ERROR));
  ags_validation_report_destroy(report);
  ags_document_destroy(dictionary_document);
  return 0;
}

static int test_dictionary_version_helpers(void) {
  ags_document *document = NULL;
  const char *version = NULL;

  EXPECT_STREQ("4.1.1", ags_dictionary_latest_version());
  EXPECT_TRUE(ags_dictionary_version_is_supported("4.0"));
  EXPECT_TRUE(ags_dictionary_version_is_supported("4.0.4"));
  EXPECT_TRUE(ags_dictionary_version_is_supported("4.1"));
  EXPECT_TRUE(ags_dictionary_version_is_supported("4.1.1"));
  EXPECT_TRUE(!ags_dictionary_version_is_supported("9.9"));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_create(NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_dictionary_resolve_version(document, NULL, &version));
  EXPECT_STREQ("4.1.1", version);
  ags_document_destroy(document);

  document = NULL;
  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(
      "\"GROUP\",\"TRAN\"\n"
      "\"HEADING\",\"TRAN_AGS\"\n"
      "\"UNIT\",\"\"\n"
      "\"TYPE\",\"X\"\n"
      "\"DATA\",\"4.0\"",
      strlen(
        "\"GROUP\",\"TRAN\"\n"
        "\"HEADING\",\"TRAN_AGS\"\n"
        "\"UNIT\",\"\"\n"
        "\"TYPE\",\"X\"\n"
        "\"DATA\",\"4.0\""
      ),
      NULL,
      &document
    )
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_dictionary_resolve_version(document, NULL, &version));
  EXPECT_STREQ("4.0.3", version);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_dictionary_resolve_version(document, "4.1", &version));
  EXPECT_STREQ("4.1", version);
  EXPECT_EQ_INT(AGS_STATUS_NOT_FOUND, ags_dictionary_resolve_version(document, "9.9", &version));
  ags_document_destroy(document);
  return 0;
}

static int test_dictionary_loaders(void) {
  static const char *versions[] = {"4.0.3", "4.0.4", "4.1", "4.1.1"};
  size_t version_index = 0;
  char *fixture = NULL;
  size_t fixture_length = 0;
  ags_document *document = NULL;
  const char *path = "libags_dictionary_fixture.ags";
  FILE *file = NULL;

  for (version_index = 0; version_index < sizeof(versions) / sizeof(versions[0]); ++version_index) {
    size_t dict_group_index = 0;
    EXPECT_EQ_INT(AGS_STATUS_OK, ags_dictionary_load_bundled(versions[version_index], NULL, &document));
    EXPECT_TRUE(document != NULL);
    EXPECT_TRUE(ags_document_group_count(document) > 0u);
    EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(document, "DICT", &dict_group_index));
    ags_document_destroy(document);
    document = NULL;
  }

  fixture = load_fixture("tests/fixtures/dictionary_custom_valid.ags", &fixture_length);
  EXPECT_TRUE(fixture != NULL);

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_dictionary_load_buffer(fixture, fixture_length, NULL, &document)
  );
  EXPECT_TRUE(document != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(document, "NGRP", &version_index));
  ags_document_destroy(document);
  document = NULL;

  file = fopen(path, "wb");
  EXPECT_TRUE(file != NULL);
  EXPECT_EQ_SIZE(fixture_length, fwrite(fixture, 1, fixture_length, file));
  EXPECT_EQ_INT(0, fclose(file));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_dictionary_load_file(path, NULL, &document));
  EXPECT_TRUE(document != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(document, "RLNK", &version_index));
  ags_document_destroy(document);
  document = NULL;

  EXPECT_EQ_INT(0, remove(path));
  free(fixture);
  return 0;
}

static int test_validate_document_with_custom_dictionary_fixture(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_custom_valid.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_EQ_SIZE(0u, ags_validation_report_diagnostic_count(report));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_dictionary_heading_rules(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_invalid_heading_rules.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(report_has_rule(report, "9", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_dictionary_required_override(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_invalid_required_override.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(report_has_rule(report, "10b", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_dictionary_duplicate_keys(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_invalid_duplicate_keys.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(report_has_rule(report, "10a", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_dictionary_parent_links(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_invalid_parent_link.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(report_has_rule(report, "10c", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_dictionary_record_links(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_invalid_record_link.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(report_has_rule(report, "11", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_dictionary_refs(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_invalid_refs.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(report_has_rule(report, "15", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "16", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "17", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static int test_validate_document_with_dictionary_required_groups(void) {
  char *input = NULL;
  size_t length = 0;
  ags_document *document = NULL;
  ags_validation_report *report = NULL;

  input = load_fixture("tests/fixtures/dictionary_invalid_required_groups.ags", &length);
  EXPECT_TRUE(input != NULL);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_parse_buffer(input, length, NULL, &document));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_validate_document_with_dictionary(document, NULL, &report));
  EXPECT_TRUE(report != NULL);
  EXPECT_TRUE(report_has_rule(report, "13", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "15", AGS_DIAGNOSTIC_ERROR));
  EXPECT_TRUE(report_has_rule(report, "16", AGS_DIAGNOSTIC_WARNING));
  EXPECT_TRUE(report_has_rule(report, "17", AGS_DIAGNOSTIC_ERROR));

  ags_validation_report_destroy(report);
  ags_document_destroy(document);
  free(input);
  return 0;
}

static ags_status test_duplicate_heading_resolver(
  void *user_data,
  const char *group_name,
  const char *original_name,
  size_t duplicate_index,
  const ags_allocator *allocator,
  char **out_heading_name
) {
  char buffer[64];
  int written = 0;
  char *copy = NULL;

  (void)user_data;
  (void)group_name;

  if (allocator == NULL || out_heading_name == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  written = snprintf(buffer, sizeof(buffer), "%s_CB%zu", original_name, duplicate_index);
  if (written < 0 || (size_t)written >= sizeof(buffer)) {
    return AGS_STATUS_INTERNAL_ERROR;
  }

  copy = (char *)allocator->malloc_fn(allocator->user_data, (size_t)written + 1u);
  if (copy == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memcpy(copy, buffer, (size_t)written + 1u);
  *out_heading_name = copy;
  return AGS_STATUS_OK;
}

static int test_table_export_import_round_trip(void) {
  ags_document *document = NULL;
  ags_document *round_trip = NULL;
  ags_table *proj_table = NULL;
  ags_table *loca_table = NULL;
  const ags_table *tables[2];
  const char *const *loca_nate_values = NULL;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_from_group(document, 0, NULL, &proj_table));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_from_group(document, 1, NULL, &loca_table));

  EXPECT_STREQ("PROJ", ags_table_group_name(proj_table));
  EXPECT_EQ_SIZE(3u, ags_table_column_count(loca_table));
  EXPECT_EQ_SIZE(2u, ags_table_row_count(loca_table));
  EXPECT_STREQ("LOCA_NATE", ags_table_column_name(loca_table, 1));
  EXPECT_STREQ("2DP", ags_table_column_type(loca_table, 1));

  loca_nate_values = ags_table_column_values(loca_table, 1);
  EXPECT_TRUE(loca_nate_values != NULL);
  EXPECT_STREQ("123.45", loca_nate_values[0]);
  EXPECT_STREQ("223.45", loca_nate_values[1]);
  EXPECT_STREQ("556.78", ags_table_cell_value(loca_table, 1, 2));

  tables[0] = proj_table;
  tables[1] = loca_table;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_from_tables(tables, 2u, NULL, &round_trip));
  EXPECT_EQ_SIZE(2u, ags_document_group_count(round_trip));
  EXPECT_STREQ("PROJ", ags_document_group_name(round_trip, 0));
  EXPECT_STREQ("LOCA", ags_document_group_name(round_trip, 1));
  EXPECT_STREQ("Site \"Alpha\"", ags_document_cell_value(round_trip, 0, 0, 1));
  EXPECT_STREQ("223.45", ags_document_cell_value(round_trip, 1, 1, 1));

  ags_table_destroy(loca_table);
  ags_table_destroy(proj_table);
  ags_document_destroy(round_trip);
  ags_document_destroy(document);
  return 0;
}

static int test_table_collection_and_summaries(void) {
  ags_table_collection *collection = NULL;
  const ags_table *loca_table = NULL;
  ags_table_summary summary;
  ags_column_summary column_summary;
  ags_table_summary_export summary_export;
  ags_column_summary_export column_export;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_table_collection_from_buffer(
      sample_ags_crlf,
      strlen(sample_ags_crlf),
      NULL,
      NULL,
      &collection
    )
  );
  EXPECT_TRUE(collection != NULL);
  EXPECT_EQ_SIZE(2u, ags_table_collection_count(collection));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_collection_get_summary(collection, 0u, &summary));
  EXPECT_EQ_SIZE(0u, summary.group_index);
  EXPECT_STREQ("PROJ", summary.group_name);
  EXPECT_EQ_SIZE(2u, summary.column_count);
  EXPECT_EQ_SIZE(1u, summary.row_count);
  EXPECT_EQ_SIZE(1u, summary.group_line_number);
  EXPECT_TRUE(summary.has_source_metadata != 0);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_collection_get_summary(collection, 1u, &summary));
  EXPECT_EQ_SIZE(1u, summary.group_index);
  EXPECT_STREQ("LOCA", summary.group_name);
  EXPECT_EQ_SIZE(3u, summary.column_count);
  EXPECT_EQ_SIZE(2u, summary.row_count);
  EXPECT_EQ_SIZE(2u, summary.numeric_column_count);
  EXPECT_EQ_SIZE(2u, summary.geometry_candidate_count);

  loca_table = ags_table_collection_get(collection, 1u);
  EXPECT_TRUE(loca_table != NULL);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_get_column_summary(loca_table, 1u, &column_summary));
  EXPECT_STREQ("LOCA_NATE", column_summary.column_name);
  EXPECT_EQ_INT(AGS_COLUMN_CLASS_GEOMETRY_EASTING_CANDIDATE, column_summary.column_class);
  EXPECT_EQ_SIZE(0u, column_summary.null_count);
  EXPECT_EQ_SIZE(2u, column_summary.non_null_count);
  EXPECT_TRUE(column_summary.is_numeric != 0);
  EXPECT_TRUE(column_summary.can_derive_geometry != 0);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_collection_get_summary_export(collection, 1u, &summary_export));
  EXPECT_TRUE(string_view_matches(summary_export.group_name, "LOCA"));
  EXPECT_EQ_SIZE(2u, summary_export.numeric_column_count);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_get_column_summary_export(loca_table, 2u, &column_export));
  EXPECT_TRUE(string_view_matches(column_export.column_name, "LOCA_NATN"));
  EXPECT_EQ_INT(AGS_COLUMN_CLASS_GEOMETRY_NORTHING_CANDIDATE, column_export.column_class);
  EXPECT_TRUE(column_export.is_numeric != 0);
  EXPECT_TRUE(column_export.can_derive_geometry != 0);

  ags_table_collection_destroy(collection);
  return 0;
}

static int test_table_collection_duplicate_heading_rename(void) {
  ags_table_collection *collection = NULL;
  ags_table_options options;
  const ags_table *table = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_options_init(&options));
  options.duplicate_heading_policy = AGS_DUPLICATE_HEADING_RENAME;
  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_table_collection_from_file(
      "tests/fixtures/invalid_parsed_duplicate_heading.ags",
      NULL,
      &options,
      &collection
    )
  );
  EXPECT_TRUE(collection != NULL);
  EXPECT_EQ_SIZE(1u, ags_table_collection_count(collection));

  table = ags_table_collection_get(collection, 0u);
  EXPECT_TRUE(table != NULL);
  EXPECT_STREQ("AB12_VAL", ags_table_column_name(table, 1u));
  EXPECT_STREQ("AB12_VAL_2", ags_table_column_name(table, 2u));

  ags_table_collection_destroy(collection);
  return 0;
}

static int test_duplicate_heading_policies(void) {
  const char *column_names[3] = {"COL", "COL", "COL"};
  const char *units[3] = {"", "", ""};
  const char *types[3] = {"X", "X", "X"};
  ags_table_options options;
  ags_table *table = NULL;

  EXPECT_EQ_INT(AGS_STATUS_INVALID_ARGUMENT, ags_table_create("DUPL", 3u, column_names, units, types, NULL, &table));
  EXPECT_TRUE(table == NULL);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_options_init(&options));
  options.duplicate_heading_policy = AGS_DUPLICATE_HEADING_RENAME;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_create("DUPL", 3u, column_names, units, types, &options, &table));
  EXPECT_STREQ("COL", ags_table_column_name(table, 0));
  EXPECT_STREQ("COL_2", ags_table_column_name(table, 1));
  EXPECT_STREQ("COL_3", ags_table_column_name(table, 2));
  ags_table_destroy(table);
  table = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_options_init(&options));
  options.duplicate_heading_policy = AGS_DUPLICATE_HEADING_CALLBACK;
  options.duplicate_heading_resolver = test_duplicate_heading_resolver;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_create("DUPL", 3u, column_names, units, types, &options, &table));
  EXPECT_STREQ("COL", ags_table_column_name(table, 0));
  EXPECT_STREQ("COL_CB2", ags_table_column_name(table, 1));
  EXPECT_STREQ("COL_CB3", ags_table_column_name(table, 2));

  ags_table_destroy(table);
  return 0;
}

static int test_numeric_column_helpers(void) {
  const char *column_names[1] = {"VAL"};
  const char *units[1] = {""};
  const char *types[1] = {"2DP"};
  const char *row1[1] = {"1.23"};
  const char *row2[1] = {""};
  ags_table *table = NULL;
  ags_numeric_column *numeric = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_create("NUMR", 1u, column_names, units, types, NULL, &table));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_append_row(table, row1, 1u));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_append_row(table, row2, 1u));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_column_to_numeric(table, 0, &numeric));
  EXPECT_EQ_SIZE(2u, ags_numeric_column_count(numeric));
  EXPECT_TRUE(!ags_numeric_column_is_null(numeric, 0));
  EXPECT_TRUE(ags_numeric_column_is_null(numeric, 1));
  EXPECT_TRUE(ags_numeric_column_value(numeric, 0) > 1.229 && ags_numeric_column_value(numeric, 0) < 1.231);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_numeric_column_set_value(numeric, 0, 9.5));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_numeric_column_set_value(numeric, 1, 11.0));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_column_from_numeric(table, 0, numeric));
  EXPECT_STREQ("9.50", ags_table_cell_value(table, 0, 0));
  EXPECT_STREQ("11.00", ags_table_cell_value(table, 1, 0));

  ags_numeric_column_destroy(numeric);
  ags_table_destroy(table);
  return 0;
}

static const char *reordered_standard_sort_sample =
  "\"GROUP\",\"LOCA\"\n"
  "\"HEADING\",\"LOCA_ID\",\"LOCA_NATE\",\"LOCA_NATN\"\n"
  "\"UNIT\",\"\",\"m\",\"m\"\n"
  "\"TYPE\",\"ID\",\"2DP\",\"2DP\"\n"
  "\"DATA\",\"L1\",\"123.45\",\"456.78\"\n"
  "\"GROUP\",\"PROJ\"\n"
  "\"HEADING\",\"PROJ_ID\",\"PROJ_NAME\"\n"
  "\"UNIT\",\"\",\"\"\n"
  "\"TYPE\",\"ID\",\"X\"\n"
  "\"DATA\",\"P1\",\"Project One\"";

static const char *hierarchical_sort_sample =
  "\"GROUP\",\"DICT\"\n"
  "\"HEADING\",\"DICT_TYPE\",\"DICT_GRP\",\"DICT_HDNG\",\"DICT_STAT\",\"DICT_DTYP\",\"DICT_DESC\",\"DICT_UNIT\",\"DICT_EXMP\",\"DICT_PGRP\",\"DICT_REM\",\"FILE_FSET\"\n"
  "\"UNIT\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"\n"
  "\"TYPE\",\"PA\",\"X\",\"X\",\"PA\",\"PT\",\"X\",\"PU\",\"X\",\"X\",\"X\",\"X\"\n"
  "\"DATA\",\"GROUP\",\"NGRP\",\"\",\"\",\"\",\"Named group\",\"\",\"\",\"-\",\"\",\"\"\n"
  "\"DATA\",\"HEADING\",\"NGRP\",\"NGRP_ID\",\"KEY+REQUIRED\",\"ID\",\"Named group identifier\",\"\",\"NG1\",\"\",\"\",\"\"\n"
  "\"DATA\",\"GROUP\",\"CHLD\",\"\",\"\",\"\",\"Child group\",\"\",\"\",\"NGRP\",\"\",\"\"\n"
  "\"DATA\",\"HEADING\",\"CHLD\",\"NGRP_ID\",\"KEY+REQUIRED\",\"ID\",\"Parent identifier\",\"\",\"NG1\",\"\",\"\",\"\"\n"
  "\"DATA\",\"HEADING\",\"CHLD\",\"CHLD_ID\",\"KEY+REQUIRED\",\"ID\",\"Child identifier\",\"\",\"CH1\",\"\",\"\",\"\"\n"
  "\"GROUP\",\"CHLD\"\n"
  "\"HEADING\",\"NGRP_ID\",\"CHLD_ID\"\n"
  "\"UNIT\",\"\",\"\"\n"
  "\"TYPE\",\"ID\",\"ID\"\n"
  "\"DATA\",\"NG1\",\"CH1\"\n"
  "\"GROUP\",\"NGRP\"\n"
  "\"HEADING\",\"NGRP_ID\"\n"
  "\"UNIT\",\"\"\n"
  "\"TYPE\",\"ID\"\n"
  "\"DATA\",\"NG1\"";

static int test_document_sorting_helpers(void) {
  ags_document *document = NULL;
  ags_document *sorted = NULL;
  ags_sort_options options;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(
      reordered_standard_sort_sample,
      strlen(reordered_standard_sort_sample),
      NULL,
      &document
    )
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_sort_options_init(&options));

  options.strategy = AGS_SORT_INPUT;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_sort_groups(document, &options, &sorted));
  EXPECT_STREQ("LOCA", ags_document_group_name(sorted, 0));
  EXPECT_STREQ("PROJ", ags_document_group_name(sorted, 1));
  ags_document_destroy(sorted);
  sorted = NULL;

  options.strategy = AGS_SORT_ALPHABETICAL;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_sort_groups(document, &options, &sorted));
  EXPECT_STREQ("LOCA", ags_document_group_name(sorted, 0));
  EXPECT_STREQ("PROJ", ags_document_group_name(sorted, 1));
  ags_document_destroy(sorted);
  sorted = NULL;

  options.strategy = AGS_SORT_DICTIONARY;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_sort_groups(document, &options, &sorted));
  EXPECT_STREQ("PROJ", ags_document_group_name(sorted, 0));
  EXPECT_STREQ("LOCA", ags_document_group_name(sorted, 1));
  ags_document_destroy(sorted);
  ags_document_destroy(document);
  return 0;
}

static int test_document_hierarchical_sort(void) {
  ags_document *document = NULL;
  ags_document *sorted = NULL;
  ags_sort_options options;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(
      hierarchical_sort_sample,
      strlen(hierarchical_sort_sample),
      NULL,
      &document
    )
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_sort_options_init(&options));
  options.strategy = AGS_SORT_HIERARCHICAL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_sort_groups(document, &options, &sorted));
  EXPECT_STREQ("DICT", ags_document_group_name(sorted, 0));
  EXPECT_STREQ("NGRP", ags_document_group_name(sorted, 1));
  EXPECT_STREQ("CHLD", ags_document_group_name(sorted, 2));

  ags_document_destroy(sorted);
  ags_document_destroy(document);
  return 0;
}

static int test_geometry_derivation_helpers(void) {
  ags_document *document = NULL;
  ags_table *table = NULL;
  ags_table *bad_table = NULL;
  ags_geometry_column *geometry = NULL;
  ags_geometry_column *wkb_geometry = NULL;
  ags_geometry_column *warn_geometry = NULL;
  ags_geometry_options options;
  const char *column_names[2] = {"X", "Y"};
  const char *units[2] = {"m", "m"};
  const char *types[2] = {"2DP", "2DP"};
  const char *bad_row[2] = {"bad", "200.0"};
  size_t wkb_length = 0;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_from_group(document, 1, NULL, &table));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_derive_geometry(table, NULL, &geometry));
  EXPECT_STREQ("GEOMETRY", ags_geometry_column_name(geometry));
  EXPECT_EQ_INT(AGS_GEOMETRY_WKT, ags_geometry_column_encoding(geometry));
  EXPECT_EQ_SIZE(2u, ags_geometry_column_row_count(geometry));
  EXPECT_EQ_SIZE(0u, ags_geometry_column_invalid_row_count(geometry));
  EXPECT_STREQ("POINT (123.45 456.78)", ags_geometry_column_wkt(geometry, 0));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_geometry_options_init(&options));
  options.encoding = AGS_GEOMETRY_WKB;
  options.easting_column_name = "LOCA_NATE";
  options.northing_column_name = "LOCA_NATN";
  options.geometry_column_name = "geom";
  options.srid = 27700;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_derive_geometry(table, &options, &wkb_geometry));
  EXPECT_STREQ("geom", ags_geometry_column_name(wkb_geometry));
  EXPECT_EQ_INT(27700, ags_geometry_column_srid(wkb_geometry));
  EXPECT_TRUE(ags_geometry_column_wkb(wkb_geometry, 0, &wkb_length) != NULL);
  EXPECT_EQ_SIZE(21u, wkb_length);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_create("BADG", 2u, column_names, units, types, NULL, &bad_table));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_append_row(bad_table, bad_row, 2u));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_geometry_options_init(&options));
  options.easting_column_name = "X";
  options.northing_column_name = "Y";
  options.invalid_coordinate_policy = AGS_INVALID_COORDINATES_WARN;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_derive_geometry(bad_table, &options, &warn_geometry));
  EXPECT_EQ_SIZE(1u, ags_geometry_column_invalid_row_count(warn_geometry));
  EXPECT_TRUE(ags_geometry_column_is_null(warn_geometry, 0));

  ags_geometry_column_destroy(warn_geometry);
  ags_table_destroy(bad_table);
  ags_geometry_column_destroy(wkb_geometry);
  ags_geometry_column_destroy(geometry);
  ags_table_destroy(table);
  ags_document_destroy(document);
  return 0;
}

static int test_document_export_long_table(void) {
  ags_document *document = NULL;
  ags_table *table = NULL;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_export_long_table(document, NULL, &table));
  EXPECT_TRUE(table != NULL);
  EXPECT_STREQ("AGS_LONG", ags_table_group_name(table));
  EXPECT_EQ_SIZE(9u, ags_table_column_count(table));
  EXPECT_EQ_SIZE(8u, ags_table_row_count(table));
  EXPECT_STREQ("0", ags_table_cell_value(table, 0u, 0u));
  EXPECT_STREQ("PROJ", ags_table_cell_value(table, 0u, 1u));
  EXPECT_STREQ("0", ags_table_cell_value(table, 0u, 2u));
  EXPECT_STREQ("PROJ_ID", ags_table_cell_value(table, 0u, 5u));
  EXPECT_STREQ("P1", ags_table_cell_value(table, 0u, 8u));
  EXPECT_STREQ("LOCA_NATN", ags_table_cell_value(table, 7u, 5u));
  EXPECT_STREQ("556.78", ags_table_cell_value(table, 7u, 8u));

  ags_table_destroy(table);
  ags_document_destroy(document);
  return 0;
}

static int test_ffi_borrowed_views_and_abi(void) {
  ags_document *document = NULL;
  ags_table *table = NULL;
  ags_string_view view;

  EXPECT_TRUE(ags_ffi_supports_abi(AGS_ABI_VERSION));
  EXPECT_TRUE(!ags_ffi_supports_abi(AGS_ABI_VERSION + 1u));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_status_string_view(AGS_STATUS_PARSE_ERROR, &view));
  EXPECT_TRUE(string_view_matches(view, "parse error"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_version_string_view(&view));
  EXPECT_TRUE(string_view_matches(view, "0.1.0"));

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_group_name_view(document, 0, &view));
  EXPECT_TRUE(string_view_matches(view, "PROJ"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_field_name_view(document, 1, 1, &view));
  EXPECT_TRUE(string_view_matches(view, "LOCA_NATE"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_field_unit_view(document, 1, 2, &view));
  EXPECT_TRUE(string_view_matches(view, "m"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_field_type_view(document, 1, 1, &view));
  EXPECT_TRUE(string_view_matches(view, "2DP"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_cell_value_view(document, 0, 0, 1, &view));
  EXPECT_TRUE(string_view_matches(view, "Site \"Alpha\""));
  EXPECT_TRUE(view.data == ags_document_cell_value(document, 0, 0, 1));
  EXPECT_EQ_SIZE(2u, ags_document_group_count(document));
  EXPECT_TRUE(string_view_matches(view, "Site \"Alpha\""));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_from_group(document, 1, NULL, &table));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_group_name_view(table, &view));
  EXPECT_TRUE(string_view_matches(view, "LOCA"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_column_name_view(table, 1, &view));
  EXPECT_TRUE(string_view_matches(view, "LOCA_NATE"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_cell_value_view(table, 1, 1, &view));
  EXPECT_TRUE(string_view_matches(view, "223.45"));
  EXPECT_TRUE(view.data == ags_table_cell_value(table, 1, 1));

  ags_table_destroy(table);
  ags_document_destroy(document);
  return 0;
}

static int test_ffi_row_cursor(void) {
  ags_document *document = NULL;
  ags_row_cursor cursor;
  ags_string_view view;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_row_cursor_init(&cursor, document, 1));
  EXPECT_TRUE(!ags_row_cursor_is_valid(&cursor));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_row_cursor_next(&cursor));
  EXPECT_TRUE(ags_row_cursor_is_valid(&cursor));
  EXPECT_EQ_SIZE(0u, ags_row_cursor_row_index(&cursor));
  EXPECT_EQ_SIZE(10u, ags_row_cursor_line_number(&cursor));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_row_cursor_cell_value_view(&cursor, 0, &view));
  EXPECT_TRUE(string_view_matches(view, "L1"));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_row_cursor_cell_value_view(&cursor, 2, &view));
  EXPECT_TRUE(string_view_matches(view, "456.78"));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_row_cursor_next(&cursor));
  EXPECT_EQ_SIZE(1u, ags_row_cursor_row_index(&cursor));
  EXPECT_EQ_SIZE(11u, ags_row_cursor_line_number(&cursor));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_row_cursor_cell_value_view(&cursor, 1, &view));
  EXPECT_TRUE(string_view_matches(view, "223.45"));

  EXPECT_EQ_INT(AGS_STATUS_NOT_FOUND, ags_row_cursor_next(&cursor));
  EXPECT_TRUE(!ags_row_cursor_is_valid(&cursor));
  EXPECT_EQ_SIZE((size_t)-1, ags_row_cursor_row_index(&cursor));
  EXPECT_EQ_INT(AGS_STATUS_NOT_FOUND, ags_row_cursor_cell_value_view(&cursor, 0, &view));

  EXPECT_EQ_INT(AGS_STATUS_NOT_FOUND, ags_row_cursor_init(&cursor, document, 99u));
  ags_document_destroy(document);
  return 0;
}

static int test_ffi_table_numeric_and_geometry_exports(void) {
  ags_document *document = NULL;
  ags_table *table = NULL;
  ags_numeric_column *numeric = NULL;
  ags_geometry_column *wkt_geometry = NULL;
  ags_geometry_column *wkb_geometry = NULL;
  ags_table_column_export column_export;
  ags_numeric_export numeric_export;
  ags_geometry_export geometry_export;
  ags_string_view string_view;
  ags_bytes_view bytes_view;
  ags_geometry_options geometry_options;
  const unsigned char *wkb_row0 = NULL;
  size_t wkb_length = 0;

  EXPECT_EQ_INT(
    AGS_STATUS_OK,
    ags_document_parse_buffer(sample_ags_crlf, strlen(sample_ags_crlf), NULL, &document)
  );
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_from_group(document, 1, NULL, &table));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_get_column_export(table, 1, &column_export));
  EXPECT_TRUE(string_view_matches(column_export.group_name, "LOCA"));
  EXPECT_TRUE(string_view_matches(column_export.column_name, "LOCA_NATE"));
  EXPECT_TRUE(string_view_matches(column_export.unit, "m"));
  EXPECT_TRUE(string_view_matches(column_export.type, "2DP"));
  EXPECT_EQ_SIZE(2u, column_export.row_count);
  EXPECT_TRUE(column_export.values == ags_table_column_values(table, 1));
  EXPECT_STREQ("123.45", column_export.values[0]);
  EXPECT_STREQ("223.45", column_export.values[1]);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_column_to_numeric(table, 1, &numeric));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_numeric_column_get_export(numeric, &numeric_export));
  EXPECT_EQ_SIZE(2u, numeric_export.count);
  EXPECT_TRUE(numeric_export.values != NULL);
  EXPECT_TRUE(numeric_export.is_null != NULL);
  EXPECT_TRUE(numeric_export.values[0] > 123.44 && numeric_export.values[0] < 123.46);
  EXPECT_TRUE(numeric_export.is_null[1] == 0u);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_numeric_column_set_value(numeric, 0, 999.0));
  EXPECT_TRUE(numeric_export.values[0] > 998.99 && numeric_export.values[0] < 999.01);

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_derive_geometry(table, NULL, &wkt_geometry));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_geometry_column_get_export(wkt_geometry, &geometry_export));
  EXPECT_TRUE(string_view_matches(geometry_export.column_name, "GEOMETRY"));
  EXPECT_EQ_INT(AGS_GEOMETRY_WKT, geometry_export.encoding);
  EXPECT_EQ_SIZE(2u, geometry_export.row_count);
  EXPECT_EQ_SIZE(0u, geometry_export.invalid_row_count);
  EXPECT_TRUE(geometry_export.is_null != NULL);
  EXPECT_TRUE(geometry_export.wkt_values != NULL);
  EXPECT_STREQ("POINT (123.45 456.78)", geometry_export.wkt_values[0]);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_geometry_column_wkt_view(wkt_geometry, 0, &string_view));
  EXPECT_TRUE(string_view_matches(string_view, "POINT (123.45 456.78)"));
  EXPECT_TRUE(string_view.data == geometry_export.wkt_values[0]);
  EXPECT_EQ_INT(AGS_STATUS_NOT_FOUND, ags_geometry_column_wkb_view(wkt_geometry, 0, &bytes_view));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_geometry_options_init(&geometry_options));
  geometry_options.encoding = AGS_GEOMETRY_WKB;
  geometry_options.easting_column_name = "LOCA_NATE";
  geometry_options.northing_column_name = "LOCA_NATN";
  geometry_options.geometry_column_name = "geom";
  geometry_options.srid = 27700;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_table_derive_geometry(table, &geometry_options, &wkb_geometry));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_geometry_column_get_export(wkb_geometry, &geometry_export));
  EXPECT_TRUE(string_view_matches(geometry_export.column_name, "geom"));
  EXPECT_EQ_INT(AGS_GEOMETRY_WKB, geometry_export.encoding);
  EXPECT_EQ_INT(27700, geometry_export.srid);
  EXPECT_TRUE(geometry_export.wkb_values != NULL);
  EXPECT_TRUE(geometry_export.wkb_lengths != NULL);
  EXPECT_EQ_SIZE(21u, geometry_export.wkb_lengths[0]);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_geometry_column_wkb_view(wkb_geometry, 0, &bytes_view));
  wkb_row0 = ags_geometry_column_wkb(wkb_geometry, 0, &wkb_length);
  EXPECT_TRUE(bytes_view_matches(bytes_view, wkb_row0, wkb_length));
  EXPECT_TRUE(bytes_view.data == geometry_export.wkb_values[0]);
  EXPECT_EQ_SIZE(21u, bytes_view.length);

  ags_geometry_column_destroy(wkb_geometry);
  ags_geometry_column_destroy(wkt_geometry);
  ags_numeric_column_destroy(numeric);
  ags_table_destroy(table);
  ags_document_destroy(document);
  return 0;
}

static ags_status test_merge_value_resolver(
  void *user_data,
  const char *group_name,
  const char *field_name,
  const char *existing_value,
  const char *incoming_value,
  const ags_allocator *allocator,
  char **out_value
) {
  char buffer[256];
  int written = 0;
  char *copy = NULL;

  (void)user_data;
  (void)group_name;
  (void)field_name;

  if (allocator == NULL || out_value == NULL) {
    return AGS_STATUS_INVALID_ARGUMENT;
  }

  written = snprintf(buffer, sizeof(buffer), "%s|%s", existing_value, incoming_value);
  if (written < 0 || (size_t)written >= sizeof(buffer)) {
    return AGS_STATUS_INTERNAL_ERROR;
  }

  copy = (char *)allocator->malloc_fn(allocator->user_data, (size_t)written + 1u);
  if (copy == NULL) {
    return AGS_STATUS_NO_MEMORY;
  }

  memcpy(copy, buffer, (size_t)written + 1u);
  *out_value = copy;
  return AGS_STATUS_OK;
}

static int test_document_merge_keyed_rows_and_provenance(void) {
  ags_document *left = NULL;
  ags_document *right = NULL;
  const ags_document *documents[2];
  ags_merge_result *result = NULL;
  const ags_document *merged = NULL;
  size_t proj_index = 0;
  size_t ngrp_index = 0;
  size_t unit_index = 0;

  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_input_blank_project.ags", &left));
  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_input_b.ags", &right));

  documents[0] = left;
  documents[1] = right;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_merge(documents, 2u, NULL, &result));
  EXPECT_TRUE(result != NULL);
  merged = ags_merge_result_document(result);
  EXPECT_TRUE(merged != NULL);
  EXPECT_EQ_SIZE(0u, ags_merge_result_diagnostic_count(result));

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(merged, "PROJ", &proj_index));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(merged, "NGRP", &ngrp_index));
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(merged, "UNIT", &unit_index));

  EXPECT_EQ_SIZE(1u, ags_document_group_row_count(merged, proj_index));
  EXPECT_STREQ("Project B", ags_document_cell_value(merged, proj_index, 0, 1));
  EXPECT_EQ_SIZE(2u, ags_merge_result_row_source_count(result, proj_index, 0));
  EXPECT_EQ_SIZE(0u, ags_merge_result_row_source_document(result, proj_index, 0, 0));
  EXPECT_EQ_SIZE(1u, ags_merge_result_row_source_document(result, proj_index, 0, 1));

  EXPECT_EQ_SIZE(2u, ags_document_group_row_count(merged, ngrp_index));
  EXPECT_STREQ("NG1", ags_document_cell_value(merged, ngrp_index, 0, 0));
  EXPECT_STREQ("NG2", ags_document_cell_value(merged, ngrp_index, 1, 0));
  EXPECT_EQ_SIZE(1u, ags_merge_result_row_source_count(result, ngrp_index, 0));
  EXPECT_EQ_SIZE(1u, ags_merge_result_row_source_count(result, ngrp_index, 1));

  EXPECT_EQ_SIZE(3u, ags_document_group_row_count(merged, unit_index));

  ags_merge_result_destroy(result);
  ags_document_destroy(right);
  ags_document_destroy(left);
  return 0;
}

static int test_document_merge_singleton_policies_and_callback(void) {
  ags_document *left = NULL;
  ags_document *right = NULL;
  const ags_document *documents[2];
  ags_merge_result *result = NULL;
  const ags_document *merged = NULL;
  ags_merge_options options;
  size_t proj_index = 0;

  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_input_a.ags", &left));
  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_input_b.ags", &right));
  documents[0] = left;
  documents[1] = right;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_merge_options_init(&options));
  options.singleton_group_policy = AGS_MERGE_CONFLICT_KEEP_LAST;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_merge(documents, 2u, &options, &result));
  merged = ags_merge_result_document(result);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(merged, "PROJ", &proj_index));
  EXPECT_STREQ("Project B", ags_document_cell_value(merged, proj_index, 0, 1));
  ags_merge_result_destroy(result);
  result = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_merge_options_init(&options));
  options.singleton_group_policy = AGS_MERGE_CONFLICT_CALLBACK;
  options.value_resolver = test_merge_value_resolver;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_merge(documents, 2u, &options, &result));
  merged = ags_merge_result_document(result);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(merged, "PROJ", &proj_index));
  EXPECT_STREQ("Project A|Project B", ags_document_cell_value(merged, proj_index, 0, 1));

  ags_merge_result_destroy(result);
  ags_document_destroy(right);
  ags_document_destroy(left);
  return 0;
}

static int test_document_merge_metadata_group_policy(void) {
  ags_document *left = NULL;
  ags_document *right = NULL;
  const ags_document *documents[2];
  ags_merge_result *result = NULL;
  const ags_document *merged = NULL;
  ags_merge_options options;
  size_t unit_index = 0;

  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_input_a.ags", &left));
  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_unit_conflict.ags", &right));
  documents[0] = left;
  documents[1] = right;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_merge_options_init(&options));
  options.singleton_group_policy = AGS_MERGE_CONFLICT_KEEP_LAST;
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_merge(documents, 2u, &options, &result));
  EXPECT_TRUE(result != NULL);
  merged = ags_merge_result_document(result);
  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_find_group(merged, "UNIT", &unit_index));
  EXPECT_STREQ("metre alternate", ags_document_cell_value(merged, unit_index, 0, 1));

  ags_merge_result_destroy(result);
  ags_document_destroy(right);
  ags_document_destroy(left);
  return 0;
}

static int test_document_merge_missing_parent_diagnostic(void) {
  ags_document *left = NULL;
  ags_document *right = NULL;
  const ags_document *documents[2];
  ags_merge_result *result = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_input_a.ags", &left));
  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_missing_parent.ags", &right));
  documents[0] = left;
  documents[1] = right;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_merge(documents, 2u, NULL, &result));
  EXPECT_TRUE(result != NULL);
  EXPECT_TRUE(merge_result_has_diagnostic(result, "CHLD", AGS_DIAGNOSTIC_ERROR, "parent group"));

  ags_merge_result_destroy(result);
  ags_document_destroy(right);
  ags_document_destroy(left);
  return 0;
}

static int test_document_merge_conflicting_custom_dictionary_diagnostic(void) {
  ags_document *left = NULL;
  ags_document *right = NULL;
  const ags_document *documents[2];
  ags_merge_result *result = NULL;

  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_input_a.ags", &left));
  EXPECT_EQ_INT(AGS_STATUS_OK, parse_document_fixture("tests/fixtures/merge_dict_conflict.ags", &right));
  documents[0] = left;
  documents[1] = right;

  EXPECT_EQ_INT(AGS_STATUS_OK, ags_document_merge(documents, 2u, NULL, &result));
  EXPECT_TRUE(result != NULL);
  EXPECT_TRUE(merge_result_has_diagnostic(result, "DICT", AGS_DIAGNOSTIC_ERROR, "dictionary definition"));

  ags_merge_result_destroy(result);
  ags_document_destroy(right);
  ags_document_destroy(left);
  return 0;
}

int main(void) {
  RUN_TEST(test_version_queries);
  RUN_TEST(test_status_helpers);
  RUN_TEST(test_default_allocator);
  RUN_TEST(test_document_options_init);
  RUN_TEST(test_document_create_and_destroy_default_allocator);
  RUN_TEST(test_document_create_rejects_invalid_arguments);
  RUN_TEST(test_document_uses_custom_allocator);
  RUN_TEST(test_parse_buffer_and_inspect_document);
  RUN_TEST(test_parse_lf_input);
  RUN_TEST(test_parse_file);
  RUN_TEST(test_parse_rejects_duplicate_groups);
  RUN_TEST(test_parse_rejects_malformed_row_shape);
  RUN_TEST(test_serialize_and_round_trip);
  RUN_TEST(test_validate_text_with_clean_fixture);
  RUN_TEST(test_validate_text_with_invalid_fixture);
  RUN_TEST(test_validate_document_with_invalid_fixture);
  RUN_TEST(test_validate_document_with_clean_sample);
  RUN_TEST(test_validate_file_helpers);
  RUN_TEST(test_dictionary_version_helpers);
  RUN_TEST(test_dictionary_loaders);
  RUN_TEST(test_validate_document_with_custom_dictionary_fixture);
  RUN_TEST(test_validate_document_with_dictionary_heading_rules);
  RUN_TEST(test_validate_document_with_dictionary_required_override);
  RUN_TEST(test_validate_document_with_dictionary_duplicate_keys);
  RUN_TEST(test_validate_document_with_dictionary_parent_links);
  RUN_TEST(test_validate_document_with_dictionary_record_links);
  RUN_TEST(test_validate_document_with_dictionary_refs);
  RUN_TEST(test_validate_document_with_dictionary_required_groups);
  RUN_TEST(test_table_export_import_round_trip);
  RUN_TEST(test_table_collection_and_summaries);
  RUN_TEST(test_table_collection_duplicate_heading_rename);
  RUN_TEST(test_duplicate_heading_policies);
  RUN_TEST(test_numeric_column_helpers);
  RUN_TEST(test_document_sorting_helpers);
  RUN_TEST(test_document_hierarchical_sort);
  RUN_TEST(test_geometry_derivation_helpers);
  RUN_TEST(test_document_export_long_table);
  RUN_TEST(test_ffi_borrowed_views_and_abi);
  RUN_TEST(test_ffi_row_cursor);
  RUN_TEST(test_ffi_table_numeric_and_geometry_exports);
  RUN_TEST(test_document_merge_keyed_rows_and_provenance);
  RUN_TEST(test_document_merge_singleton_policies_and_callback);
  RUN_TEST(test_document_merge_metadata_group_policy);
  RUN_TEST(test_document_merge_missing_parent_diagnostic);
  RUN_TEST(test_document_merge_conflicting_custom_dictionary_diagnostic);

  if (tests_failed != 0) {
    fprintf(stderr, "%d of %d tests failed\n", tests_failed, tests_run);
    return 1;
  }

  printf("%d tests passed\n", tests_run);
  return 0;
}
