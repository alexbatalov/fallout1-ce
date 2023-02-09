#include "game/textobj.h"

#include <string.h>

#include "game/gconfig.h"
#include "game/object.h"
#include "game/tile.h"
#include "game/wordwrap.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/text.h"

namespace fallout {

// The maximum number of text objects that can exist at the same time.
#define TEXT_OBJECTS_MAX_COUNT 20

typedef enum TextObjectFlags {
    TEXT_OBJECT_MARKED_FOR_REMOVAL = 0x01,
    TEXT_OBJECT_UNBOUNDED = 0x02,
} TextObjectFlags;

typedef struct TextObject {
    int flags;
    Object* owner;
    unsigned int time;
    int linesCount;
    int sx;
    int sy;
    int tile;
    int x;
    int y;
    int width;
    int height;
    unsigned char* data;
} TextObject;

static void text_object_bk();
static void text_object_get_offset(TextObject* textObject);

// 0x508324
static int text_object_index = 0;

// 0x508328
static unsigned int text_object_base_delay = 3500;

// 0x50832C
static unsigned int text_object_line_delay = 1399;

// 0x665210
static TextObject* text_object_list[TEXT_OBJECTS_MAX_COUNT];

// 0x665260
static int display_width;

// 0x665264
static int display_height;

// 0x665268
static unsigned char* display_buffer;

// 0x66526C
static bool text_object_enabled;

// 0x665270
static bool text_object_initialized;

// 0x49CD80
int text_object_init(unsigned char* windowBuffer, int width, int height)
{
    double textBaseDelay;
    double textLineDelay;

    if (text_object_initialized) {
        return -1;
    }

    display_buffer = windowBuffer;
    display_width = width;
    display_height = height;
    text_object_index = 0;

    add_bk_process(text_object_bk);

    if (!config_get_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, &textBaseDelay)) {
        textBaseDelay = 3.5;
    }

    if (!config_get_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, &textLineDelay)) {
        textLineDelay = 1.399993896484375;
    }

    text_object_base_delay = (unsigned int)(textBaseDelay * 1000.0);
    text_object_line_delay = (unsigned int)(textLineDelay * 1000.0);

    text_object_enabled = true;
    text_object_initialized = true;

    return 0;
}

// 0x49CE64
int text_object_reset()
{
    int index;

    if (!text_object_initialized) {
        return -1;
    }

    for (index = 0; index < text_object_index; index++) {
        mem_free(text_object_list[index]->data);
        mem_free(text_object_list[index]);
    }

    text_object_index = 0;
    add_bk_process(text_object_bk);

    return 0;
}

// 0x49CEC8
void text_object_exit()
{
    if (text_object_initialized) {
        text_object_reset();
        remove_bk_process(text_object_bk);
        text_object_initialized = false;
    }
}

// 0x49CEEC
void text_object_disable()
{
    text_object_enabled = false;
}

// 0x49CEF8
void text_object_enable()
{
    text_object_enabled = true;
}

// NOTE: Unused.
//
// 0x49CF04
int text_object_is_enabled()
{
    return text_object_enabled;
}

// 0x49CF0C
void text_object_set_base_delay(double value)
{
    if (value < 1.0) {
        value = 1.0;
    }

    text_object_base_delay = (int)(value * 1000.0);
}

// 0x49CF50
unsigned int text_object_get_base_delay()
{
    return text_object_base_delay / 1000;
}

// 0x49CF64
void text_object_set_line_delay(double value)
{
    if (value < 0.0) {
        value = 0.0;
    }

    text_object_line_delay = (int)(value * 1000.0);
}

// 0x49CFA0
unsigned int text_object_get_line_delay()
{
    return text_object_line_delay / 1000;
}

// 0x49CFB4
int text_object_create(Object* object, char* string, int font, int color, int a5, Rect* rect)
{
    if (!text_object_initialized) {
        return -1;
    }

    if (text_object_index >= TEXT_OBJECTS_MAX_COUNT - 1) {
        return -1;
    }

    if (string == NULL) {
        return -1;
    }

    if (*string == '\0') {
        return -1;
    }

    TextObject* textObject = (TextObject*)mem_malloc(sizeof(*textObject));
    if (textObject == NULL) {
        return -1;
    }

    memset(textObject, 0, sizeof(*textObject));

    int oldFont = text_curr();
    text_font(font);

    short beginnings[WORD_WRAP_MAX_COUNT];
    short count;
    if (word_wrap(string, 200, beginnings, &count) != 0) {
        text_font(oldFont);
        return -1;
    }

    textObject->linesCount = count - 1;
    if (textObject->linesCount < 1) {
        debug_printf("**Error in text_object_create()\n");
    }

    textObject->width = 0;

    for (int index = 0; index < textObject->linesCount; index++) {
        char* ending = string + beginnings[index + 1];
        char* beginning = string + beginnings[index];
        if (ending[-1] == ' ') {
            --ending;
        }

        char c = *ending;
        *ending = '\0';

        // NOTE: Calls [text_width] twice, probably result of using min/max macro
        int width = text_width(beginning);
        if (width >= textObject->width) {
            textObject->width = width;
        }

        *ending = c;
    }

    textObject->height = (text_height() + 1) * textObject->linesCount;

    if (a5 != -1) {
        textObject->width += 2;
        textObject->height += 2;
    }

    int size = textObject->width * textObject->height;
    textObject->data = (unsigned char*)mem_malloc(size);
    if (textObject->data == NULL) {
        text_font(oldFont);
        return -1;
    }

    memset(textObject->data, 0, size);

    unsigned char* dest = textObject->data;
    int skip = textObject->width * (text_height() + 1);

    if (a5 != -1) {
        dest += textObject->width;
    }

    for (int index = 0; index < textObject->linesCount; index++) {
        char* beginning = string + beginnings[index];
        char* ending = string + beginnings[index + 1];
        if (ending[-1] == ' ') {
            --ending;
        }

        char c = *ending;
        *ending = '\0';

        int width = text_width(beginning);
        text_to_buf(dest + (textObject->width - width) / 2, beginning, textObject->width, textObject->width, color);

        *ending = c;

        dest += skip;
    }

    if (a5 != -1) {
        buf_outline(textObject->data, textObject->width, textObject->height, textObject->width, a5);
    }

    if (object != NULL) {
        textObject->tile = object->tile;
    } else {
        textObject->flags |= TEXT_OBJECT_UNBOUNDED;
        textObject->tile = tile_center_tile;
    }

    text_object_get_offset(textObject);

    if (rect != NULL) {
        rect->ulx = textObject->x;
        rect->uly = textObject->y;
        rect->lrx = textObject->x + textObject->width - 1;
        rect->lry = textObject->y + textObject->height - 1;
    }

    text_object_remove(object);

    textObject->owner = object;
    textObject->time = get_bk_time();

    text_object_list[text_object_index] = textObject;
    text_object_index++;

    text_font(oldFont);

    return 0;
}

// 0x49D330
void text_object_render(Rect* rect)
{
    int index;
    TextObject* textObject;
    Rect textObjectRect;

    if (!text_object_initialized) {
        return;
    }

    for (index = 0; index < text_object_index; index++) {
        textObject = text_object_list[index];
        tile_coord(textObject->tile, &(textObject->x), &(textObject->y), map_elevation);
        textObject->x += textObject->sx;
        textObject->y += textObject->sy;

        textObjectRect.ulx = textObject->x;
        textObjectRect.uly = textObject->y;
        textObjectRect.lrx = textObject->width + textObject->x - 1;
        textObjectRect.lry = textObject->height + textObject->y - 1;
        if (rect_inside_bound(&textObjectRect, rect, &textObjectRect) == 0) {
            trans_buf_to_buf(textObject->data + textObject->width * (textObjectRect.uly - textObject->y) + (textObjectRect.ulx - textObject->x),
                textObjectRect.lrx - textObjectRect.ulx + 1,
                textObjectRect.lry - textObjectRect.uly + 1,
                textObject->width,
                display_buffer + display_width * textObjectRect.uly + textObjectRect.ulx,
                display_width);
        }
    }
}

// 0x49D438
int text_object_count()
{
    return text_object_index;
}

// 0x49D440
static void text_object_bk()
{
    if (!text_object_enabled) {
        return;
    }

    bool textObjectsRemoved = false;
    Rect dirtyRect;

    for (int index = 0; index < text_object_index; index++) {
        TextObject* textObject = text_object_list[index];

        unsigned int delay = text_object_line_delay * textObject->linesCount + text_object_base_delay;
        if ((textObject->flags & TEXT_OBJECT_MARKED_FOR_REMOVAL) != 0 || (elapsed_tocks(get_bk_time(), textObject->time) > delay)) {
            tile_coord(textObject->tile, &(textObject->x), &(textObject->y), map_elevation);
            textObject->x += textObject->sx;
            textObject->y += textObject->sy;

            Rect textObjectRect;
            textObjectRect.ulx = textObject->x;
            textObjectRect.uly = textObject->y;
            textObjectRect.lrx = textObject->width + textObject->x - 1;
            textObjectRect.lry = textObject->height + textObject->y - 1;

            if (textObjectsRemoved) {
                rect_min_bound(&dirtyRect, &textObjectRect, &dirtyRect);
            } else {
                rectCopy(&dirtyRect, &textObjectRect);
                textObjectsRemoved = true;
            }

            mem_free(textObject->data);
            mem_free(textObject);

            memmove(&(text_object_list[index]), &(text_object_list[index + 1]), sizeof(*text_object_list) * (text_object_index - index - 1));

            text_object_index--;
            index--;
        }
    }

    if (textObjectsRemoved) {
        tile_refresh_rect(&dirtyRect, map_elevation);
    }
}

// Finds best position for placing text object.
//
// 0x49D59C
static void text_object_get_offset(TextObject* textObject)
{
    int tileScreenX;
    int tileScreenY;
    tile_coord(textObject->tile, &tileScreenX, &tileScreenY, map_elevation);
    textObject->x = tileScreenX + 16 - textObject->width / 2;
    textObject->y = tileScreenY;

    if ((textObject->flags & TEXT_OBJECT_UNBOUNDED) == 0) {
        textObject->y -= textObject->height + 60;
    }

    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x -= textObject->width / 2;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x += textObject->width;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x = tileScreenX - 16 - textObject->width;
    textObject->y = tileScreenY - 16 - textObject->height;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x += textObject->width + 64;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x = tileScreenX + 16 - textObject->width / 2;
    textObject->y = tileScreenY;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x -= textObject->width / 2;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x += textObject->width;
    if ((textObject->x >= 0 && textObject->x + textObject->width - 1 < display_width)
        && (textObject->y >= 0 && textObject->y + textObject->height - 1 < display_height)) {
        textObject->sx = textObject->x - tileScreenX;
        textObject->sy = textObject->y - tileScreenY;
        return;
    }

    textObject->x = tileScreenX + 16 - textObject->width / 2;
    textObject->y = tileScreenY - (textObject->height + 60);
    textObject->sx = textObject->x - tileScreenX;
    textObject->sy = textObject->y - tileScreenY;
}

// Marks text objects attached to `object` for removal.
//
// 0x49D848
void text_object_remove(Object* object)
{
    int index;

    for (index = 0; index < text_object_index; index++) {
        if (text_object_list[index]->owner == object) {
            text_object_list[index]->flags |= TEXT_OBJECT_MARKED_FOR_REMOVAL;
        }
    }
}

} // namespace fallout
