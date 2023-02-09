#ifndef FALLOUT_PLIB_GNW_TEXT_H_
#define FALLOUT_PLIB_GNW_TEXT_H_

namespace fallout {

#define FONT_SHADOW 0x10000
#define FONT_UNDERLINE 0x20000
#define FONT_MONO 0x40000

typedef void text_font_func(int font);
typedef void text_to_buf_func(unsigned char* buf, const char* str, int swidth, int fullw, int color);
typedef int text_height_func();
typedef int text_width_func(const char* str);
typedef int text_char_width_func(char c);
typedef int text_mono_width_func(const char* str);
typedef int text_spacing_func();
typedef int text_size_func(const char* str);
typedef int text_max_func();

typedef struct FontMgr {
    int low_font_num;
    int high_font_num;
    text_font_func* text_font;
    text_to_buf_func* text_to_buf;
    text_height_func* text_height;
    text_width_func* text_width;
    text_char_width_func* text_char_width;
    text_mono_width_func* text_mono_width;
    text_spacing_func* text_spacing;
    text_size_func* text_size;
    text_max_func* text_max;
} FontMgr;

typedef FontMgr* FontMgrPtr;

typedef struct FontInfo {
    // The width of the glyph in pixels.
    int width;

    // Data offset into [Font.data].
    int offset;
} FontInfo;

typedef struct Font {
    // The number of glyphs in the font.
    int num;

    // The height of the font.
    int height;

    // Horizontal spacing between characters in pixels.
    int spacing;

    FontInfo* info;
    unsigned char* data;
} Font;

extern text_to_buf_func* text_to_buf;
extern text_height_func* text_height;
extern text_width_func* text_width;
extern text_char_width_func* text_char_width;
extern text_mono_width_func* text_mono_width;
extern text_spacing_func* text_spacing;
extern text_size_func* text_size;
extern text_max_func* text_max;

int GNW_text_init();
void GNW_text_exit();
int text_add_manager(FontMgrPtr mgr);
int text_remove_manager(int font_num);
int text_curr();
void text_font(int font_num);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_TEXT_H_ */
