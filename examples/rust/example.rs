use std::ffi::CStr;
use std::slice;

use libags_sys::{
    ags_document, ags_document_cell_value_view, ags_document_destroy, ags_document_parse_buffer,
    ags_row_cursor, ags_row_cursor_cell_value_view, ags_row_cursor_init, ags_row_cursor_next,
    ags_string_view, AGS_STATUS_OK,
};

unsafe fn string_view_as_str(view: ags_string_view) -> &'static str {
    let bytes = slice::from_raw_parts(view.data as *const u8, view.length);
    std::str::from_utf8_unchecked(bytes)
}

unsafe fn parse_document(input: &CStr) -> *mut ags_document {
    let mut document = std::ptr::null_mut();
    let status = ags_document_parse_buffer(
        input.as_ptr(),
        input.to_bytes().len(),
        std::ptr::null(),
        &mut document,
    );
    assert_eq!(status, AGS_STATUS_OK);
    document
}

unsafe fn iter_loca_ids(document: *mut ags_document) {
    let mut cursor = ags_row_cursor {
        document,
        group_index: 1,
        row_index: 0,
        has_row: 0,
    };
    let mut view = ags_string_view {
        data: std::ptr::null(),
        length: 0,
    };

    assert_eq!(ags_row_cursor_init(&mut cursor, document, 1), AGS_STATUS_OK);
    while ags_row_cursor_next(&mut cursor) == AGS_STATUS_OK {
      assert_eq!(ags_row_cursor_cell_value_view(&cursor, 0, &mut view), AGS_STATUS_OK);
      println!("LOCA_ID={}", string_view_as_str(view));
    }

    let mut proj_name = ags_string_view {
        data: std::ptr::null(),
        length: 0,
    };
    assert_eq!(ags_document_cell_value_view(document, 0, 0, 1, &mut proj_name), AGS_STATUS_OK);
    println!("PROJ_NAME={}", string_view_as_str(proj_name));
}

fn main() {
    let input = std::ffi::CString::new(
        "\"GROUP\",\"PROJ\"\r\n\
         \"HEADING\",\"PROJ_ID\",\"PROJ_NAME\"\r\n\
         \"UNIT\",\"\",\"\"\r\n\
         \"TYPE\",\"ID\",\"X\"\r\n\
         \"DATA\",\"P1\",\"Example\"\r\n\
         \"GROUP\",\"LOCA\"\r\n\
         \"HEADING\",\"LOCA_ID\",\"LOCA_NATE\",\"LOCA_NATN\"\r\n\
         \"UNIT\",\"\",\"m\",\"m\"\r\n\
         \"TYPE\",\"ID\",\"2DP\",\"2DP\"\r\n\
         \"DATA\",\"L1\",\"123.45\",\"456.78\"",
    )
    .unwrap();

    unsafe {
        let document = parse_document(&input);
        iter_loca_ids(document);
        ags_document_destroy(document);
    }
}
