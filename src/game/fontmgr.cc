#include "game/fontmgr.h"

#include <stdio.h>
#include <string.h>

#include "int/memdbg.h"
#include "plib/color/color.h"
#include "plib/db/db.h"
#include "plib/gnw/text.h"

namespace fallout {

// The maximum number of interface fonts.
#define INTERFACE_FONT_MAX 16

typedef struct InterfaceFontGlyph {
    short width;
    short height;
    int offset;
} InterfaceFontGlyph;

typedef struct InterfaceFontDescriptor {
    short maxHeight;
    short letterSpacing;
    short wordSpacing;
    short lineSpacing;
    short field_8;
    short field_A;
    InterfaceFontGlyph glyphs[256];
    unsigned char* data;
} InterfaceFontDescriptor;

static int FMLoadFont(int font);
static void swapUInt32(unsigned int* value);
static void swapUInt16(unsigned short* value);

static void swapInt32(int* value);
static void swapInt16(short* value);

// 0x504F88
static bool gFMInit = false;

// 0x504F8C
static int gNumFonts = 0;

// 0x584B08
static InterfaceFontDescriptor gFontCache[INTERFACE_FONT_MAX];

// 0x58CC08
static int gCurrentFontNum;

// 0x58CC0C
static InterfaceFontDescriptor* gCurrentFont;

// 0x43A780
int FMInit()
{
    int currentFont = -1;

    for (int font = 0; font < INTERFACE_FONT_MAX; font++) {
        if (FMLoadFont(font) == -1) {
            gFontCache[font].maxHeight = 0;
            gFontCache[font].data = NULL;
        } else {
            ++gNumFonts;

            if (currentFont == -1) {
                currentFont = font;
            }
        }
    }

    if (currentFont == -1) {
        return -1;
    }

    gFMInit = true;

    FMtext_font(currentFont + 100);

    return 0;
}

// 0x43A7EC
void FMExit()
{
    for (int font = 0; font < INTERFACE_FONT_MAX; font++) {
        if (gFontCache[font].data != NULL) {
            myfree(gFontCache[font].data, __FILE__, __LINE__); // FONTMGR.C, 124
        }
    }
}

// 0x43A820
static int FMLoadFont(int font_index)
{
    InterfaceFontDescriptor* fontDescriptor = &(gFontCache[font_index]);

    char path[56];
    snprintf(path, sizeof(path), "font%d.aaf", font_index);

    DB_FILE* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    int fileSize = db_filelength(stream);

    int sig;
    if (db_fread(&sig, 4, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }

    swapInt32(&sig);
    if (sig != 0x41414646) {
        db_fclose(stream);
        return -1;
    }

    if (db_fread(&(fontDescriptor->maxHeight), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    swapInt16(&(fontDescriptor->maxHeight));

    if (db_fread(&(fontDescriptor->letterSpacing), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    swapInt16(&(fontDescriptor->letterSpacing));

    if (db_fread(&(fontDescriptor->wordSpacing), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    swapInt16(&(fontDescriptor->wordSpacing));

    if (db_fread(&(fontDescriptor->lineSpacing), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    swapInt16(&(fontDescriptor->lineSpacing));

    for (int index = 0; index < 256; index++) {
        InterfaceFontGlyph* glyph = &(fontDescriptor->glyphs[index]);

        if (db_fread(&(glyph->width), 2, 1, stream) != 1) {
            db_fclose(stream);
            return -1;
        }
        swapInt16(&(glyph->width));

        if (db_fread(&(glyph->height), 2, 1, stream) != 1) {
            db_fclose(stream);
            return -1;
        }
        swapInt16(&(glyph->height));

        if (db_fread(&(glyph->offset), 4, 1, stream) != 1) {
            db_fclose(stream);
            return -1;
        }
        swapInt32(&(glyph->offset));
    }

    int glyphDataSize = fileSize - 2060;

    fontDescriptor->data = (unsigned char*)mymalloc(glyphDataSize, __FILE__, __LINE__); // FONTMGR.C, 259
    if (fontDescriptor->data == NULL) {
        db_fclose(stream);
        return -1;
    }

    if (db_fread(fontDescriptor->data, glyphDataSize, 1, stream) != 1) {
        myfree(fontDescriptor->data, __FILE__, __LINE__); // FONTMGR.C, 268
        db_fclose(stream);
        return -1;
    }

    db_fclose(stream);

    return 0;
}

// 0x43AC20
void FMtext_font(int font)
{
    if (!gFMInit) {
        return;
    }

    font -= 100;

    if (gFontCache[font].data != NULL) {
        gCurrentFontNum = font;
        gCurrentFont = &(gFontCache[font]);
    }
}

// 0x43AC68
int FMtext_height()
{
    if (!gFMInit) {
        return 0;
    }

    return gCurrentFont->lineSpacing + gCurrentFont->maxHeight;
}

// 0x43AC88
int FMtext_width(const char* string)
{
    if (!gFMInit) {
        return 0;
    }

    int stringWidth = 0;

    while (*string != '\0') {
        unsigned char ch = (unsigned char)(*string++);

        int characterWidth;
        if (ch == ' ') {
            characterWidth = gCurrentFont->wordSpacing;
        } else {
            characterWidth = gCurrentFont->glyphs[ch].width;
        }

        stringWidth += characterWidth + gCurrentFont->letterSpacing;
    }

    return stringWidth;
}

// 0x43ACE0
int FMtext_char_width(char c)
{
    int width;

    if (!gFMInit) {
        return 0;
    }

    if (c == ' ') {
        width = gCurrentFont->wordSpacing;
    } else {
        width = gCurrentFont->glyphs[c].width;
    }

    return width;
}

// 0x43AD14
int FMtext_mono_width(const char* str)
{
    if (!gFMInit) {
        return 0;
    }

    return FMtext_max() * strlen(str);
}

// 0x43AD44
int FMtext_spacing()
{
    if (!gFMInit) {
        return 0;
    }

    return gCurrentFont->letterSpacing;
}

// 0x43AD5C
int FMtext_size(const char* str)
{
    if (!gFMInit) {
        return 0;
    }

    return FMtext_width(str) * FMtext_height();
}

// 0x43AD7C
int FMtext_max()
{
    if (!gFMInit) {
        return 0;
    }

    int v1;
    if (gCurrentFont->wordSpacing <= gCurrentFont->field_8) {
        v1 = gCurrentFont->lineSpacing;
    } else {
        v1 = gCurrentFont->letterSpacing;
    }

    return v1 + gCurrentFont->maxHeight;
}

// 0x43ADB0
int FMtext_curr()
{
    return gCurrentFontNum;
}

// 0x43ADB8
void FMtext_to_buf(unsigned char* buf, const char* string, int length, int pitch, int color)
{
    if (!gFMInit) {
        return;
    }

    if ((color & FONT_SHADOW) != 0) {
        color &= ~FONT_SHADOW;
        // NOTE: Other font options preserved. This is different from text font
        // shadows.
        FMtext_to_buf(buf + pitch + 1, string, length, pitch, (color & ~0xFF) | colorTable[0]);
    }

    unsigned char* palette = getColorBlendTable(color & 0xFF);

    int monospacedCharacterWidth;
    if ((color & FONT_MONO) != 0) {
        // NOTE: Uninline.
        monospacedCharacterWidth = FMtext_max();
    }

    unsigned char* ptr = buf;
    while (*string != '\0') {
        unsigned char ch = static_cast<unsigned char>(*string++);

        int characterWidth;
        if (ch == ' ') {
            characterWidth = gCurrentFont->wordSpacing;
        } else {
            characterWidth = gCurrentFont->glyphs[ch].width;
        }

        unsigned char* end;
        if ((color & FONT_MONO) != 0) {
            end = ptr + monospacedCharacterWidth;
            ptr += (monospacedCharacterWidth - characterWidth - gCurrentFont->letterSpacing) / 2;
        } else {
            end = ptr + characterWidth + gCurrentFont->letterSpacing;
        }

        if (end - buf > length) {
            break;
        }

        InterfaceFontGlyph* glyph = &(gCurrentFont->glyphs[ch]);
        unsigned char* glyphDataPtr = gCurrentFont->data + glyph->offset;

        // Skip blank pixels (difference between font's line height and glyph height).
        ptr += (gCurrentFont->maxHeight - glyph->height) * pitch;

        for (int y = 0; y < glyph->height; y++) {
            for (int x = 0; x < glyph->width; x++) {
                unsigned char byte = *glyphDataPtr++;

                *ptr++ = palette[(byte << 8) + *ptr];
            }

            ptr += pitch - glyph->width;
        }

        ptr = end;
    }

    if ((color & FONT_UNDERLINE) != 0) {
        int length = ptr - buf;
        unsigned char* underlinePtr = buf + pitch * (gCurrentFont->maxHeight - 1);
        for (int index = 0; index < length; index++) {
            *underlinePtr++ = color & 0xFF;
        }
    }

    freeColorBlendTable(color & 0xFF);
}

// 0x43AFF0
int FMtext_leading()
{
    if (!gFMInit) {
        return 0;
    }

    return gCurrentFont->lineSpacing;
}

// 0x43B008
int FMtext_space_width()
{
    if (!gFMInit) {
        return 0;
    }

    return gCurrentFont->wordSpacing;
}

// 0x442520
static void swapUInt32(unsigned int* value)
{
    unsigned int swapped = *value;
    unsigned short high = swapped >> 16;
    // NOTE: Uninline.
    swapUInt16(&high);
    unsigned short low = swapped & 0xFFFF;
    // NOTE: Uninline.
    swapUInt16(&low);
    *value = (low << 16) | high;
}

// 0x442568
static void swapUInt16(unsigned short* value)
{
    unsigned short swapped = *value;
    swapped = (swapped >> 8) | (swapped << 8);
    *value = swapped;
}

static void swapInt32(int* value)
{
    swapUInt32(reinterpret_cast<unsigned int*>(value));
}

static void swapInt16(short* value)
{
    swapUInt16(reinterpret_cast<unsigned short*>(value));
}

} // namespace fallout
