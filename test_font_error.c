/*******************************************************************************
 * Test font file with missing comma error
 * This file is used to test the parser's error detection
 ******************************************************************************/

#include "lvgl.h"

/*-----------------
 *    BITMAPS
 *----------------*/

/* Bpp: 4 */
static const uint8_t glyph_bitmap[] = {
    /* U+0041 "A" */
    0x00, 0x12, 0x00, 0x00, 0x56, 0x00,  // Correct format
    0x09 0xab, 0xcd, 0x00,               // ERROR: Missing comma after 0x09
    0xef, 0x00
};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 192, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 65, .range_length = 1, .glyph_id_start = 0,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*--------------------
 *  FONT DESCRIPTOR
 *-------------------*/

const lv_font_t test_font_error = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 16,
    .base_line = 12,
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -2,
    .underline_thickness = 1,
    .dsc = &font_dsc
};

static lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kerning = 0,
    .glyph_cnt = 1,
    .bitmap_format = 0
};
