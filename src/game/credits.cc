#include "game/credits.h"

#include <string.h>

#include "game/art.h"
#include "game/cycle.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/message.h"
#include "game/palette.h"
#include "int/sound.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define CREDITS_WINDOW_SCROLLING_DELAY 38

static bool credits_get_next_line(char* dest, int* font, int* color);

// 0x56BEE0
static DB_FILE* credits_file;

// 0x56BEE4
static int name_color;

// 0x56BEE8
static int title_font;

// 0x56BEEC
static int name_font;

// 0x56BEF0
static int title_color;

// 0x426FE0
void credits(const char* filePath, int backgroundFid, bool useReversedStyle)
{
    int oldFont = text_curr();

    loadColorTable("color.pal");

    if (useReversedStyle) {
        title_color = colorTable[18917];
        name_font = 103;
        title_font = 104;
        name_color = colorTable[13673];
    } else {
        title_color = colorTable[13673];
        name_font = 104;
        title_font = 103;
        name_color = colorTable[18917];
    }

    soundUpdate();

    char localizedPath[COMPAT_MAX_PATH];
    if (message_make_path(localizedPath, sizeof(localizedPath), filePath)) {
        credits_file = db_fopen(localizedPath, "rt");
        if (credits_file != NULL) {
            soundUpdate();

            cycle_disable();
            gmouse_set_cursor(MOUSE_CURSOR_NONE);

            bool cursorWasHidden = mouse_hidden();
            if (cursorWasHidden) {
                mouse_show();
            }

            int windowWidth = screenGetWidth();
            int windowHeight = screenGetHeight();
            int window = win_add(0, 0, windowWidth, windowHeight, colorTable[0], 20);
            soundUpdate();
            if (window != -1) {
                unsigned char* windowBuffer = win_get_buf(window);
                if (windowBuffer != NULL) {
                    unsigned char* backgroundBuffer = (unsigned char*)mem_malloc(windowWidth * windowHeight);
                    if (backgroundBuffer) {
                        soundUpdate();

                        memset(backgroundBuffer, colorTable[0], windowWidth * windowHeight);

                        if (backgroundFid != -1) {
                            CacheEntry* backgroundFrmHandle;
                            Art* frm = art_ptr_lock(backgroundFid, &backgroundFrmHandle);
                            if (frm != NULL) {
                                int width = art_frame_width(frm, 0, 0);
                                int height = art_frame_length(frm, 0, 0);
                                unsigned char* backgroundFrmData = art_frame_data(frm, 0, 0);
                                buf_to_buf(backgroundFrmData,
                                    width,
                                    height,
                                    width,
                                    backgroundBuffer + windowWidth * ((windowHeight - height) / 2) + (windowWidth - width) / 2,
                                    windowWidth);
                                art_ptr_unlock(backgroundFrmHandle);
                            }
                        }

                        unsigned char* intermediateBuffer = (unsigned char*)mem_malloc(windowWidth * windowHeight);
                        if (intermediateBuffer != NULL) {
                            memset(intermediateBuffer, 0, windowWidth * windowHeight);

                            text_font(title_font);
                            int titleFontLineHeight = text_height();

                            text_font(name_font);
                            int nameFontLineHeight = text_height();

                            int lineHeight = nameFontLineHeight + (titleFontLineHeight >= nameFontLineHeight ? titleFontLineHeight - nameFontLineHeight : 0);
                            int stringBufferSize = windowWidth * lineHeight;
                            unsigned char* stringBuffer = (unsigned char*)mem_malloc(stringBufferSize);
                            if (stringBuffer != NULL) {
                                const char* boom = "boom";
                                int exploding_head_frame = 0;
                                int exploding_head_cycle = 0;
                                int violence_level = 0;

                                config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violence_level);

                                buf_to_buf(backgroundBuffer,
                                    windowWidth,
                                    windowHeight,
                                    windowWidth,
                                    windowBuffer,
                                    windowWidth);

                                win_draw(window);

                                palette_fade_to(cmap);

                                unsigned char* v40 = intermediateBuffer + windowWidth * windowHeight - windowWidth;
                                char str[260];
                                int font;
                                int color;
                                unsigned int tick = 0;
                                bool stop = false;
                                while (credits_get_next_line(str, &font, &color)) {
                                    text_font(font);

                                    int v19 = text_width(str);
                                    if (v19 >= windowWidth) {
                                        continue;
                                    }

                                    memset(stringBuffer, 0, stringBufferSize);
                                    text_to_buf(stringBuffer, str, windowWidth, windowWidth, color);

                                    unsigned char* dest = intermediateBuffer + windowWidth * windowHeight - windowWidth + (windowWidth - v19) / 2;
                                    unsigned char* src = stringBuffer;
                                    for (int index = 0; index < lineHeight; index++) {
                                        sharedFpsLimiter.mark();

                                        int input = get_input();
                                        if (input != -1) {
                                            if (input != *boom) {
                                                stop = true;
                                                break;
                                            }

                                            boom++;
                                            if (*boom == '\0') {
                                                exploding_head_frame = 1;
                                                boom = "boom";
                                            }
                                        }

                                        memmove(intermediateBuffer, intermediateBuffer + windowWidth, windowWidth * windowHeight - windowWidth);
                                        memcpy(dest, src, v19);

                                        buf_to_buf(backgroundBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        trans_buf_to_buf(intermediateBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        if (violence_level != VIOLENCE_LEVEL_NONE) {
                                            if (exploding_head_frame != 0) {
                                                CacheEntry* exploding_head_key;
                                                int exploding_head_fid = art_id(OBJ_TYPE_INTERFACE, 39, 0, 0, 0);
                                                Art* exploding_head_frm = art_ptr_lock(exploding_head_fid, &exploding_head_key);
                                                if (exploding_head_frm != NULL && exploding_head_frame - 1 < art_frame_max_frame(exploding_head_frm)) {
                                                    int width = art_frame_width(exploding_head_frm, exploding_head_frame - 1, 0);
                                                    int height = art_frame_length(exploding_head_frm, exploding_head_frame - 1, 0);
                                                    unsigned char* logoData = art_frame_data(exploding_head_frm, exploding_head_frame - 1, 0);
                                                    trans_buf_to_buf(logoData,
                                                        width,
                                                        height,
                                                        width,
                                                        windowBuffer + windowWidth * (windowHeight - height) + (windowWidth - width) / 2,
                                                        windowWidth);
                                                    art_ptr_unlock(exploding_head_key);

                                                    if (exploding_head_cycle) {
                                                        exploding_head_frame++;
                                                    }

                                                    exploding_head_cycle = 1 - exploding_head_cycle;
                                                } else {
                                                    exploding_head_frame = 0;
                                                }
                                            }
                                        }

                                        while (elapsed_time(tick) < CREDITS_WINDOW_SCROLLING_DELAY) {
                                        }

                                        tick = get_time();

                                        win_draw(window);

                                        src += windowWidth;

                                        sharedFpsLimiter.throttle();
                                        renderPresent();
                                    }

                                    if (stop) {
                                        break;
                                    }
                                }

                                if (!stop) {
                                    for (int index = 0; index < windowHeight; index++) {
                                        sharedFpsLimiter.mark();

                                        if (get_input() != -1) {
                                            break;
                                        }

                                        memmove(intermediateBuffer, intermediateBuffer + windowWidth, windowWidth * windowHeight - windowWidth);
                                        memset(intermediateBuffer + windowWidth * windowHeight - windowWidth, 0, windowWidth);

                                        buf_to_buf(backgroundBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        trans_buf_to_buf(intermediateBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        while (elapsed_time(tick) < CREDITS_WINDOW_SCROLLING_DELAY) {
                                        }

                                        tick = get_time();

                                        win_draw(window);

                                        sharedFpsLimiter.throttle();
                                        renderPresent();
                                    }
                                }

                                mem_free(stringBuffer);
                            }
                            mem_free(intermediateBuffer);
                        }
                        mem_free(backgroundBuffer);
                    }
                }

                soundUpdate();
                palette_fade_to(black_palette);
                soundUpdate();
                win_delete(window);
            }

            if (cursorWasHidden) {
                mouse_hide();
            }

            gmouse_set_cursor(MOUSE_CURSOR_ARROW);
            cycle_enable();
            db_fclose(credits_file);
        }
    }

    text_font(oldFont);
}

// 0x42777C
static bool credits_get_next_line(char* dest, int* font, int* color)
{
    char string[256];
    while (db_fgets(string, 256, credits_file)) {
        char* pch;
        if (string[0] == ';') {
            continue;
        } else if (string[0] == '@') {
            *font = title_font;
            *color = title_color;
            pch = string + 1;
        } else if (string[0] == '#') {
            *font = name_font;
            *color = colorTable[17969];
            pch = string + 1;
        } else {
            *font = name_font;
            *color = name_color;
            pch = string;
        }

        strcpy(dest, pch);

        return true;
    }

    return false;
}

} // namespace fallout
