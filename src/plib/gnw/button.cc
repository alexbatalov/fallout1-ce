#include "plib/gnw/button.h"

#include "plib/color/color.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/text.h"

namespace fallout {

// The maximum number of button groups.
#define BUTTON_GROUP_LIST_CAPACITY 64

// 0x53A258
static int last_button_winID = -1;

// 0x6AC2D0
static ButtonGroup btn_grp[BUTTON_GROUP_LIST_CAPACITY];

static Button* button_create(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, int flags, unsigned char* up, unsigned char* dn, unsigned char* hover);
static bool button_under_mouse(Button* button, Rect* rect);
static int button_check_group(Button* button);
static void button_draw(Button* button, Window* window, unsigned char* data, bool draw, Rect* bound, bool sound);

// 0x4C4320
int win_register_button(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, unsigned char* up, unsigned char* dn, unsigned char* hover, int flags)
{
    Window* w = GNW_find(win);

    if (!GNW_win_init_flag) {
        return -1;
    }

    if (w == NULL) {
        return -1;
    }

    if (up == NULL && (dn != NULL || hover != NULL)) {
        return -1;
    }

    Button* button = button_create(win, x, y, width, height, mouseEnterEventCode, mouseExitEventCode, mouseDownEventCode, mouseUpEventCode, flags | BUTTON_FLAG_GRAPHIC, up, dn, hover);
    if (button == NULL) {
        return -1;
    }

    button_draw(button, w, button->normalImage, false, NULL, false);

    return button->id;
}

// 0x4C43C8
int win_register_text_button(int win, int x, int y, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, const char* title, int flags)
{
    Window* w = GNW_find(win);

    if (!GNW_win_init_flag) {
        return -1;
    }

    if (w == NULL) {
        return -1;
    }

    int buttonWidth = text_width(title) + 16;
    int buttonHeight = text_height() + 7;
    unsigned char* normal = (unsigned char*)mem_malloc(buttonWidth * buttonHeight);
    if (normal == NULL) {
        return -1;
    }

    unsigned char* pressed = (unsigned char*)mem_malloc(buttonWidth * buttonHeight);
    if (pressed == NULL) {
        mem_free(normal);
        return -1;
    }

    if (w->color == 256 && GNW_texture != NULL) {
        // TODO: Incomplete.
    } else {
        buf_fill(normal, buttonWidth, buttonHeight, buttonWidth, w->color);
        buf_fill(pressed, buttonWidth, buttonHeight, buttonWidth, w->color);
    }

    lighten_buf(normal, buttonWidth, buttonHeight, buttonWidth);

    text_to_buf(normal + buttonWidth * 3 + 8, title, buttonWidth, buttonWidth, colorTable[GNW_wcolor[3]]);
    draw_shaded_box(normal,
        buttonWidth,
        2,
        2,
        buttonWidth - 3,
        buttonHeight - 3,
        colorTable[GNW_wcolor[1]],
        colorTable[GNW_wcolor[2]]);
    draw_shaded_box(normal,
        buttonWidth,
        1,
        1,
        buttonWidth - 2,
        buttonHeight - 2,
        colorTable[GNW_wcolor[1]],
        colorTable[GNW_wcolor[2]]);
    draw_box(normal, buttonWidth, 0, 0, buttonWidth - 1, buttonHeight - 1, colorTable[0]);

    text_to_buf(pressed + buttonWidth * 4 + 9, title, buttonWidth, buttonWidth, colorTable[GNW_wcolor[3]]);
    draw_shaded_box(pressed,
        buttonWidth,
        2,
        2,
        buttonWidth - 3,
        buttonHeight - 3,
        colorTable[GNW_wcolor[2]],
        colorTable[GNW_wcolor[1]]);
    draw_shaded_box(pressed,
        buttonWidth,
        1,
        1,
        buttonWidth - 2,
        buttonHeight - 2,
        colorTable[GNW_wcolor[2]],
        colorTable[GNW_wcolor[1]]);
    draw_box(pressed, buttonWidth, 0, 0, buttonWidth - 1, buttonHeight - 1, colorTable[0]);

    Button* button = button_create(win,
        x,
        y,
        buttonWidth,
        buttonHeight,
        mouseEnterEventCode,
        mouseExitEventCode,
        mouseDownEventCode,
        mouseUpEventCode,
        flags,
        normal,
        pressed,
        NULL);
    if (button == NULL) {
        mem_free(normal);
        mem_free(pressed);
        return -1;
    }

    button_draw(button, w, button->normalImage, false, NULL, false);

    return button->id;
}

// 0x4C4734
int win_register_button_disable(int btn, unsigned char* up, unsigned char* down, unsigned char* hover)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Button* button = GNW_find_button(btn, NULL);
    if (button == NULL) {
        return -1;
    }

    button->disabledNormalImage = up;
    button->disabledPressedImage = down;
    button->disabledHoverImage = hover;

    return 0;
}

// 0x4C4768
int win_register_button_image(int btn, unsigned char* up, unsigned char* down, unsigned char* hover, bool draw)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    if (up == NULL && (down != NULL || hover != NULL)) {
        return -1;
    }

    Window* w;
    Button* button = GNW_find_button(btn, &w);
    if (button == NULL) {
        return -1;
    }

    if ((button->flags & BUTTON_FLAG_GRAPHIC) == 0) {
        return -1;
    }

    unsigned char* data = button->currentImage;
    if (data == button->normalImage) {
        button->currentImage = up;
    } else if (data == button->pressedImage) {
        button->currentImage = down;
    } else if (data == button->hoverImage) {
        button->currentImage = hover;
    }

    button->normalImage = up;
    button->pressedImage = down;
    button->hoverImage = hover;

    button_draw(button, w, button->currentImage, draw, NULL, false);

    return 0;
}

// 0x4C4810
int win_register_button_func(int btn, ButtonCallback* mouseEnterProc, ButtonCallback* mouseExitProc, ButtonCallback* mouseDownProc, ButtonCallback* mouseUpProc)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Button* button = GNW_find_button(btn, NULL);
    if (button == NULL) {
        return -1;
    }

    button->mouseEnterProc = mouseEnterProc;
    button->mouseExitProc = mouseExitProc;
    button->leftMouseDownProc = mouseDownProc;
    button->leftMouseUpProc = mouseUpProc;

    return 0;
}

// 0x4C4850
int win_register_right_button(int btn, int rightMouseDownEventCode, int rightMouseUpEventCode, ButtonCallback* rightMouseDownProc, ButtonCallback* rightMouseUpProc)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Button* button = GNW_find_button(btn, NULL);
    if (button == NULL) {
        return -1;
    }

    button->rightMouseDownEventCode = rightMouseDownEventCode;
    button->rightMouseUpEventCode = rightMouseUpEventCode;
    button->rightMouseDownProc = rightMouseDownProc;
    button->rightMouseUpProc = rightMouseUpProc;

    if (rightMouseDownEventCode != -1 || rightMouseUpEventCode != -1 || rightMouseDownProc != NULL || rightMouseUpProc != NULL) {
        button->flags |= BUTTON_FLAG_RIGHT_MOUSE_BUTTON_CONFIGURED;
    } else {
        button->flags &= ~BUTTON_FLAG_RIGHT_MOUSE_BUTTON_CONFIGURED;
    }

    return 0;
}

// 0x4C48B0
int win_register_button_sound_func(int btn, ButtonCallback* pressSoundFunc, ButtonCallback* releaseSoundFunc)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Button* button = GNW_find_button(btn, NULL);
    if (button == NULL) {
        return -1;
    }

    button->pressSoundFunc = pressSoundFunc;
    button->releaseSoundFunc = releaseSoundFunc;

    return 0;
}

// 0x4C48E0
int win_register_button_mask(int btn, unsigned char* mask)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Button* button = GNW_find_button(btn, NULL);
    if (button == NULL) {
        return -1;
    }

    button->mask = mask;

    return 0;
}

// 0x4C490C
static Button* button_create(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, int flags, unsigned char* up, unsigned char* dn, unsigned char* hover)
{
    Window* w = GNW_find(win);
    if (w == NULL) {
        return NULL;
    }

    Button* button = (Button*)mem_malloc(sizeof(*button));
    if (button == NULL) {
        return NULL;
    }

    if ((flags & BUTTON_FLAG_0x01) == 0) {
        if ((flags & BUTTON_FLAG_0x02) != 0) {
            flags &= ~BUTTON_FLAG_0x02;
        }

        if ((flags & BUTTON_FLAG_0x04) != 0) {
            flags &= ~BUTTON_FLAG_0x04;
        }
    }

    // NOTE: Uninline.
    int buttonId = button_new_id();

    button->id = buttonId;
    button->flags = flags;
    button->rect.ulx = x;
    button->rect.uly = y;
    button->rect.lrx = x + width - 1;
    button->rect.lry = y + height - 1;
    button->mouseEnterEventCode = mouseEnterEventCode;
    button->mouseExitEventCode = mouseExitEventCode;
    button->lefMouseDownEventCode = mouseDownEventCode;
    button->leftMouseUpEventCode = mouseUpEventCode;
    button->rightMouseDownEventCode = -1;
    button->rightMouseUpEventCode = -1;
    button->normalImage = up;
    button->pressedImage = dn;
    button->hoverImage = hover;
    button->disabledNormalImage = NULL;
    button->disabledPressedImage = NULL;
    button->disabledHoverImage = NULL;
    button->currentImage = NULL;
    button->mask = NULL;
    button->mouseEnterProc = NULL;
    button->mouseExitProc = NULL;
    button->leftMouseDownProc = NULL;
    button->leftMouseUpProc = NULL;
    button->rightMouseDownProc = NULL;
    button->rightMouseUpProc = NULL;
    button->pressSoundFunc = NULL;
    button->releaseSoundFunc = NULL;
    button->buttonGroup = NULL;
    button->prev = NULL;

    button->next = w->buttonListHead;
    if (button->next != NULL) {
        button->next->prev = button;
    }
    w->buttonListHead = button;

    return button;
}

// 0x4C4A9C
bool win_button_down(int btn)
{
    if (!GNW_win_init_flag) {
        return false;
    }

    Button* button = GNW_find_button(btn, NULL);
    if (button == NULL) {
        return false;
    }

    if ((button->flags & BUTTON_FLAG_0x01) != 0 && (button->flags & BUTTON_FLAG_CHECKED) != 0) {
        return true;
    }

    return false;
}

// 0x4C4AC8
int GNW_check_buttons(Window* w, int* keyCodePtr)
{
    Rect v58;
    Button* hoveredButton;
    Button* clickedButton;
    Button* button;

    if ((w->flags & WINDOW_HIDDEN) != 0) {
        return -1;
    }

    button = w->buttonListHead;
    hoveredButton = w->hoveredButton;
    clickedButton = w->clickedButton;

    if (hoveredButton != NULL) {
        rectCopy(&v58, &(hoveredButton->rect));
        rectOffset(&v58, w->rect.ulx, w->rect.uly);
    } else if (clickedButton != NULL) {
        rectCopy(&v58, &(clickedButton->rect));
        rectOffset(&v58, w->rect.ulx, w->rect.uly);
    }

    *keyCodePtr = -1;

    if (mouse_click_in(w->rect.ulx, w->rect.uly, w->rect.lrx, w->rect.lry)) {
        int mouseEvent = mouse_get_buttons();
        if ((w->flags & WINDOW_FLAG_0x40) || (mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) == 0) {
            if (mouseEvent == 0) {
                w->clickedButton = NULL;
            }
        } else {
            win_show(w->id);
        }

        if (hoveredButton != NULL) {
            if (!button_under_mouse(hoveredButton, &v58)) {
                if (!(hoveredButton->flags & BUTTON_FLAG_DISABLED)) {
                    *keyCodePtr = hoveredButton->mouseExitEventCode;
                }

                if ((hoveredButton->flags & BUTTON_FLAG_0x01) && (hoveredButton->flags & BUTTON_FLAG_CHECKED)) {
                    button_draw(hoveredButton, w, hoveredButton->pressedImage, true, NULL, true);
                } else {
                    button_draw(hoveredButton, w, hoveredButton->normalImage, true, NULL, true);
                }

                w->hoveredButton = NULL;

                last_button_winID = w->id;

                if (!(hoveredButton->flags & BUTTON_FLAG_DISABLED)) {
                    if (hoveredButton->mouseExitProc != NULL) {
                        hoveredButton->mouseExitProc(hoveredButton->id, *keyCodePtr);
                        if (!(hoveredButton->flags & BUTTON_FLAG_0x40)) {
                            *keyCodePtr = -1;
                        }
                    }
                }
                return 0;
            }
            button = hoveredButton;
        } else if (clickedButton != NULL) {
            if (button_under_mouse(clickedButton, &v58)) {
                if (!(clickedButton->flags & BUTTON_FLAG_DISABLED)) {
                    *keyCodePtr = clickedButton->mouseEnterEventCode;
                }

                if ((clickedButton->flags & BUTTON_FLAG_0x01) && (clickedButton->flags & BUTTON_FLAG_CHECKED)) {
                    button_draw(clickedButton, w, clickedButton->pressedImage, true, NULL, true);
                } else {
                    button_draw(clickedButton, w, clickedButton->normalImage, true, NULL, true);
                }

                w->hoveredButton = clickedButton;

                last_button_winID = w->id;

                if (!(clickedButton->flags & BUTTON_FLAG_DISABLED)) {
                    if (clickedButton->mouseEnterProc != NULL) {
                        clickedButton->mouseEnterProc(clickedButton->id, *keyCodePtr);
                        if (!(clickedButton->flags & BUTTON_FLAG_0x40)) {
                            *keyCodePtr = -1;
                        }
                    }
                }
                return 0;
            }
        }

        int v25 = last_button_winID;
        if (last_button_winID != -1 && last_button_winID != w->id) {
            Window* v26 = GNW_find(last_button_winID);
            if (v26 != NULL) {
                last_button_winID = -1;

                Button* v28 = v26->hoveredButton;
                if (v28 != NULL) {
                    if (!(v28->flags & BUTTON_FLAG_DISABLED)) {
                        *keyCodePtr = v28->mouseExitEventCode;
                    }

                    if ((v28->flags & BUTTON_FLAG_0x01) && (v28->flags & BUTTON_FLAG_CHECKED)) {
                        button_draw(v28, v26, v28->pressedImage, true, NULL, true);
                    } else {
                        button_draw(v28, v26, v28->normalImage, true, NULL, true);
                    }

                    v26->clickedButton = NULL;
                    v26->hoveredButton = NULL;

                    if (!(v28->flags & BUTTON_FLAG_DISABLED)) {
                        if (v28->mouseExitProc != NULL) {
                            v28->mouseExitProc(v28->id, *keyCodePtr);
                            if (!(v28->flags & BUTTON_FLAG_0x40)) {
                                *keyCodePtr = -1;
                            }
                        }
                    }
                    return 0;
                }
            }
        }

        ButtonCallback* cb = NULL;

        while (button != NULL) {
            if (!(button->flags & BUTTON_FLAG_DISABLED)) {
                rectCopy(&v58, &(button->rect));
                rectOffset(&v58, w->rect.ulx, w->rect.uly);
                if (button_under_mouse(button, &v58)) {
                    if (!(button->flags & BUTTON_FLAG_DISABLED)) {
                        if ((mouseEvent & MOUSE_EVENT_ANY_BUTTON_DOWN) != 0) {
                            if ((mouseEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0 && (button->flags & BUTTON_FLAG_RIGHT_MOUSE_BUTTON_CONFIGURED) == 0) {
                                button = NULL;
                                break;
                            }

                            if (button != w->hoveredButton && button != w->clickedButton) {
                                break;
                            }

                            w->clickedButton = button;
                            w->hoveredButton = button;

                            if ((button->flags & BUTTON_FLAG_0x01) != 0) {
                                if ((button->flags & BUTTON_FLAG_0x02) != 0) {
                                    if ((button->flags & BUTTON_FLAG_CHECKED) != 0) {
                                        if (!(button->flags & BUTTON_FLAG_0x04)) {
                                            if (button->buttonGroup != NULL) {
                                                button->buttonGroup->currChecked--;
                                            }

                                            if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                                                *keyCodePtr = button->leftMouseUpEventCode;
                                                cb = button->leftMouseUpProc;
                                            } else {
                                                *keyCodePtr = button->rightMouseUpEventCode;
                                                cb = button->rightMouseUpProc;
                                            }

                                            button->flags &= ~BUTTON_FLAG_CHECKED;
                                        }
                                    } else {
                                        if (button_check_group(button) == -1) {
                                            button = NULL;
                                            break;
                                        }

                                        if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                                            *keyCodePtr = button->lefMouseDownEventCode;
                                            cb = button->leftMouseDownProc;
                                        } else {
                                            *keyCodePtr = button->rightMouseDownEventCode;
                                            cb = button->rightMouseDownProc;
                                        }

                                        button->flags |= BUTTON_FLAG_CHECKED;
                                    }
                                }
                            } else {
                                if (button_check_group(button) == -1) {
                                    button = NULL;
                                    break;
                                }

                                if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                                    *keyCodePtr = button->lefMouseDownEventCode;
                                    cb = button->leftMouseDownProc;
                                } else {
                                    *keyCodePtr = button->rightMouseDownEventCode;
                                    cb = button->rightMouseDownProc;
                                }
                            }

                            button_draw(button, w, button->pressedImage, true, NULL, true);
                            break;
                        }

                        Button* v49 = w->clickedButton;
                        if (button == v49 && (mouseEvent & MOUSE_EVENT_ANY_BUTTON_UP) != 0) {
                            w->clickedButton = NULL;
                            w->hoveredButton = v49;

                            if (v49->flags & BUTTON_FLAG_0x01) {
                                if (!(v49->flags & BUTTON_FLAG_0x02)) {
                                    if (v49->flags & BUTTON_FLAG_CHECKED) {
                                        if (!(v49->flags & BUTTON_FLAG_0x04)) {
                                            if (v49->buttonGroup != NULL) {
                                                v49->buttonGroup->currChecked--;
                                            }

                                            if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                                                *keyCodePtr = button->leftMouseUpEventCode;
                                                cb = button->leftMouseUpProc;
                                            } else {
                                                *keyCodePtr = button->rightMouseUpEventCode;
                                                cb = button->rightMouseUpProc;
                                            }

                                            button->flags &= ~BUTTON_FLAG_CHECKED;
                                        }
                                    } else {
                                        if (button_check_group(v49) == -1) {
                                            button = NULL;
                                            button_draw(v49, w, v49->normalImage, true, NULL, true);
                                            break;
                                        }

                                        if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                                            *keyCodePtr = v49->lefMouseDownEventCode;
                                            cb = v49->leftMouseDownProc;
                                        } else {
                                            *keyCodePtr = v49->rightMouseDownEventCode;
                                            cb = v49->rightMouseDownProc;
                                        }

                                        v49->flags |= BUTTON_FLAG_CHECKED;
                                    }
                                }
                            } else {
                                if (v49->flags & BUTTON_FLAG_CHECKED) {
                                    if (v49->buttonGroup != NULL) {
                                        v49->buttonGroup->currChecked--;
                                    }
                                }

                                if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                                    *keyCodePtr = v49->leftMouseUpEventCode;
                                    cb = v49->leftMouseUpProc;
                                } else {
                                    *keyCodePtr = v49->rightMouseUpEventCode;
                                    cb = v49->rightMouseUpProc;
                                }
                            }

                            if (button->hoverImage != NULL) {
                                button_draw(button, w, button->hoverImage, true, NULL, true);
                            } else {
                                button_draw(button, w, button->normalImage, true, NULL, true);
                            }
                            break;
                        }
                    }

                    if (w->hoveredButton == NULL && mouseEvent == 0) {
                        w->hoveredButton = button;
                        if (!(button->flags & BUTTON_FLAG_DISABLED)) {
                            *keyCodePtr = button->mouseEnterEventCode;
                            cb = button->mouseEnterProc;
                        }

                        button_draw(button, w, button->hoverImage, true, NULL, true);
                    }
                    break;
                }
            }
            button = button->next;
        }

        if (button != NULL) {
            if ((button->flags & BUTTON_FLAG_0x10) != 0
                && (mouseEvent & MOUSE_EVENT_ANY_BUTTON_DOWN) != 0
                && (mouseEvent & MOUSE_EVENT_ANY_BUTTON_REPEAT) == 0) {
                win_drag(w->id);
                button_draw(button, w, button->normalImage, true, NULL, true);
            }
        } else if ((w->flags & WINDOW_FLAG_0x80) != 0) {
            v25 |= mouseEvent << 8;
            if ((mouseEvent & MOUSE_EVENT_ANY_BUTTON_DOWN) != 0
                && (mouseEvent & MOUSE_EVENT_ANY_BUTTON_REPEAT) == 0) {
                win_drag(w->id);
            }
        }

        last_button_winID = w->id;

        if (button != NULL) {
            if (cb != NULL) {
                cb(button->id, *keyCodePtr);
                if (!(button->flags & BUTTON_FLAG_0x40)) {
                    *keyCodePtr = -1;
                }
            }
        }

        return 0;
    }

    if (hoveredButton != NULL) {
        *keyCodePtr = hoveredButton->mouseExitEventCode;

        unsigned char* data;
        if ((hoveredButton->flags & BUTTON_FLAG_0x01) && (hoveredButton->flags & BUTTON_FLAG_CHECKED)) {
            data = hoveredButton->pressedImage;
        } else {
            data = hoveredButton->normalImage;
        }

        button_draw(hoveredButton, w, data, true, NULL, true);

        w->hoveredButton = NULL;
    }

    if (*keyCodePtr != -1) {
        last_button_winID = w->id;

        if ((hoveredButton->flags & BUTTON_FLAG_DISABLED) == 0) {
            if (hoveredButton->mouseExitProc != NULL) {
                hoveredButton->mouseExitProc(hoveredButton->id, *keyCodePtr);
                if (!(hoveredButton->flags & BUTTON_FLAG_0x40)) {
                    *keyCodePtr = -1;
                }
            }
        }
        return 0;
    }

    if (hoveredButton != NULL) {
        if ((hoveredButton->flags & BUTTON_FLAG_DISABLED) == 0) {
            if (hoveredButton->mouseExitProc != NULL) {
                hoveredButton->mouseExitProc(hoveredButton->id, *keyCodePtr);
            }
        }
    }

    return -1;
}

// 0x4C52CC
static bool button_under_mouse(Button* button, Rect* rect)
{
    if (!mouse_click_in(rect->ulx, rect->uly, rect->lrx, rect->lry)) {
        return false;
    }

    if (button->mask == NULL) {
        return true;
    }

    int x;
    int y;
    mouse_get_position(&x, &y);
    x -= rect->ulx;
    y -= rect->uly;

    int width = button->rect.lrx - button->rect.ulx + 1;
    return button->mask[width * y + x] != 0;
}

// 0x4C5334
int win_button_winID(int btn)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Window* w;
    if (GNW_find_button(btn, &w) == NULL) {
        return -1;
    }

    return w->id;
}

// 0x4C536C
int win_last_button_winID()
{
    return last_button_winID;
}

// 0x4C5374
int win_delete_button(int btn)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Window* w;
    Button* button = GNW_find_button(btn, &w);
    if (button == NULL) {
        return -1;
    }

    if (button->prev != NULL) {
        button->prev->next = button->next;
    } else {
        w->buttonListHead = button->next;
    }

    if (button->next != NULL) {
        button->next->prev = button->prev;
    }

    win_fill(w->id, button->rect.ulx, button->rect.uly, button->rect.lrx - button->rect.ulx + 1, button->rect.lry - button->rect.uly + 1, w->color);

    if (button == w->hoveredButton) {
        w->hoveredButton = NULL;
    }

    if (button == w->clickedButton) {
        w->clickedButton = NULL;
    }

    GNW_delete_button(button);

    return 0;
}

// 0x4C542C
void GNW_delete_button(Button* button)
{
    if ((button->flags & BUTTON_FLAG_GRAPHIC) == 0) {
        if (button->normalImage != NULL) {
            mem_free(button->normalImage);
        }

        if (button->pressedImage != NULL) {
            mem_free(button->pressedImage);
        }

        if (button->hoverImage != NULL) {
            mem_free(button->hoverImage);
        }

        if (button->disabledNormalImage != NULL) {
            mem_free(button->disabledNormalImage);
        }

        if (button->disabledPressedImage != NULL) {
            mem_free(button->disabledPressedImage);
        }

        if (button->disabledHoverImage != NULL) {
            mem_free(button->disabledHoverImage);
        }
    }

    ButtonGroup* buttonGroup = button->buttonGroup;
    if (buttonGroup != NULL) {
        for (int index = 0; index < buttonGroup->buttonsLength; index++) {
            if (button == buttonGroup->buttons[index]) {
                for (; index < buttonGroup->buttonsLength - 1; index++) {
                    buttonGroup->buttons[index] = buttonGroup->buttons[index + 1];
                }

                buttonGroup->buttonsLength--;

                break;
            }
        }
    }

    mem_free(button);
}

// 0x4C54E8
void win_delete_button_win(int btn, int inputEvent)
{
    Button* button;
    Window* w;

    button = GNW_find_button(btn, &w);
    if (button != NULL) {
        win_delete(w->id);
        GNW_add_input_buffer(inputEvent);
    }
}

// 0x4C5510
int button_new_id()
{
    int btn;

    btn = 1;
    while (GNW_find_button(btn, NULL) != NULL) {
        btn++;
    }

    return btn;
}

// 0x4C552C
int win_enable_button(int btn)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Window* w;
    Button* button = GNW_find_button(btn, &w);
    if (button == NULL) {
        return -1;
    }

    if ((button->flags & BUTTON_FLAG_DISABLED) != 0) {
        button->flags &= ~BUTTON_FLAG_DISABLED;
        button_draw(button, w, button->currentImage, true, NULL, false);
    }

    return 0;
}

// 0x4C5588
int win_disable_button(int btn)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Window* w;
    Button* button = GNW_find_button(btn, &w);
    if (button == NULL) {
        return -1;
    }

    if ((button->flags & BUTTON_FLAG_DISABLED) == 0) {
        button->flags |= BUTTON_FLAG_DISABLED;

        button_draw(button, w, button->currentImage, true, NULL, false);

        if (button == w->hoveredButton) {
            if (w->hoveredButton->mouseExitEventCode != -1) {
                GNW_add_input_buffer(w->hoveredButton->mouseExitEventCode);
                w->hoveredButton = NULL;
            }
        }
    }

    return 0;
}

// 0x4C560C
int win_set_button_rest_state(int btn, bool checked, int flags)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Window* w;
    Button* button = GNW_find_button(btn, &w);
    if (button == NULL) {
        return -1;
    }

    if ((button->flags & BUTTON_FLAG_0x01) != 0) {
        int keyCode = -1;

        if ((button->flags & BUTTON_FLAG_CHECKED) != 0) {
            if (!checked) {
                button->flags &= ~BUTTON_FLAG_CHECKED;

                if ((flags & 0x02) == 0) {
                    button_draw(button, w, button->normalImage, true, NULL, false);
                }

                if (button->buttonGroup != NULL) {
                    button->buttonGroup->currChecked--;
                }

                keyCode = button->leftMouseUpEventCode;
            }
        } else {
            if (checked) {
                button->flags |= BUTTON_FLAG_CHECKED;

                if ((flags & 0x02) == 0) {
                    button_draw(button, w, button->pressedImage, true, NULL, false);
                }

                if (button->buttonGroup != NULL) {
                    button->buttonGroup->currChecked++;
                }

                keyCode = button->lefMouseDownEventCode;
            }
        }

        if (keyCode != -1) {
            if ((flags & 0x01) != 0) {
                GNW_add_input_buffer(keyCode);
            }
        }
    }

    return 0;
}

// 0x4C56E4
int win_group_check_buttons(int buttonCount, int* btns, int maxChecked, RadioButtonCallback* func)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    if (buttonCount >= BUTTON_GROUP_BUTTON_LIST_CAPACITY) {
        return -1;
    }

    for (int groupIndex = 0; groupIndex < BUTTON_GROUP_LIST_CAPACITY; groupIndex++) {
        ButtonGroup* buttonGroup = &(btn_grp[groupIndex]);
        if (buttonGroup->buttonsLength == 0) {
            buttonGroup->currChecked = 0;

            for (int buttonIndex = 0; buttonIndex < buttonCount; buttonIndex++) {
                Button* button = GNW_find_button(btns[buttonIndex], NULL);
                if (button == NULL) {
                    return -1;
                }

                buttonGroup->buttons[buttonIndex] = button;

                button->buttonGroup = buttonGroup;

                if ((button->flags & BUTTON_FLAG_CHECKED) != 0) {
                    buttonGroup->currChecked++;
                }
            }

            buttonGroup->buttonsLength = buttonCount;
            buttonGroup->maxChecked = maxChecked;
            buttonGroup->func = func;
            return 0;
        }
    }

    return -1;
}

// 0x4C57A4
int win_group_radio_buttons(int buttonCount, int* btns)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    if (win_group_check_buttons(buttonCount, btns, 1, NULL) == -1) {
        return -1;
    }

    Button* button = GNW_find_button(btns[0], NULL);
    ButtonGroup* buttonGroup = button->buttonGroup;

    for (int index = 0; index < buttonGroup->buttonsLength; index++) {
        Button* v1 = buttonGroup->buttons[index];
        v1->flags |= BUTTON_FLAG_RADIO;
    }

    return 0;
}

// 0x4C57FC
static int button_check_group(Button* button)
{
    if (button->buttonGroup == NULL) {
        return 0;
    }

    if ((button->flags & BUTTON_FLAG_RADIO) != 0) {
        if (button->buttonGroup->currChecked > 0) {
            for (int index = 0; index < button->buttonGroup->buttonsLength; index++) {
                Button* otherButton = button->buttonGroup->buttons[index];
                if ((otherButton->flags & BUTTON_FLAG_CHECKED) != 0) {
                    otherButton->flags &= ~BUTTON_FLAG_CHECKED;

                    Window* w;
                    GNW_find_button(otherButton->id, &w);
                    button_draw(otherButton, w, otherButton->normalImage, true, NULL, true);

                    if (otherButton->leftMouseUpProc != NULL) {
                        otherButton->leftMouseUpProc(otherButton->id, otherButton->leftMouseUpEventCode);
                    }
                }
            }
        }

        if ((button->flags & BUTTON_FLAG_CHECKED) == 0) {
            button->buttonGroup->currChecked++;
        }

        return 0;
    }

    if (button->buttonGroup->currChecked < button->buttonGroup->maxChecked) {
        if ((button->flags & BUTTON_FLAG_CHECKED) == 0) {
            button->buttonGroup->currChecked++;
        }

        return 0;
    }

    if (button->buttonGroup->func != NULL) {
        button->buttonGroup->func(button->id);
    }

    return -1;
}

// 0x4C58C0
static void button_draw(Button* button, Window* w, unsigned char* data, bool draw, Rect* bound, bool sound)
{
    unsigned char* previousImage = NULL;
    if (data != NULL) {
        Rect v2;
        rectCopy(&v2, &(button->rect));
        rectOffset(&v2, w->rect.ulx, w->rect.uly);

        Rect v3;
        if (bound != NULL) {
            if (rect_inside_bound(&v2, bound, &v2) == -1) {
                return;
            }

            rectCopy(&v3, &v2);
            rectOffset(&v3, -w->rect.ulx, -w->rect.uly);
        } else {
            rectCopy(&v3, &(button->rect));
        }

        if (data == button->normalImage && (button->flags & BUTTON_FLAG_CHECKED)) {
            data = button->pressedImage;
        }

        if (button->flags & BUTTON_FLAG_DISABLED) {
            if (data == button->normalImage) {
                data = button->disabledNormalImage;
            } else if (data == button->pressedImage) {
                data = button->disabledPressedImage;
            } else if (data == button->hoverImage) {
                data = button->disabledHoverImage;
            }
        } else {
            if (data == button->disabledNormalImage) {
                data = button->normalImage;
            } else if (data == button->disabledPressedImage) {
                data = button->pressedImage;
            } else if (data == button->disabledHoverImage) {
                data = button->hoverImage;
            }
        }

        if (data) {
            if (!draw) {
                int width = button->rect.lrx - button->rect.ulx + 1;
                if ((button->flags & BUTTON_FLAG_TRANSPARENT) != 0) {
                    trans_buf_to_buf(
                        data + (v3.uly - button->rect.uly) * width + v3.ulx - button->rect.ulx,
                        v3.lrx - v3.ulx + 1,
                        v3.lry - v3.uly + 1,
                        width,
                        w->buffer + w->width * v3.uly + v3.ulx,
                        w->width);
                } else {
                    buf_to_buf(
                        data + (v3.uly - button->rect.uly) * width + v3.ulx - button->rect.ulx,
                        v3.lrx - v3.ulx + 1,
                        v3.lry - v3.uly + 1,
                        width,
                        w->buffer + w->width * v3.uly + v3.ulx,
                        w->width);
                }
            }

            previousImage = button->currentImage;
            button->currentImage = data;

            if (draw) {
                GNW_win_refresh(w, &v2, 0);
            }
        }
    }

    if (sound) {
        if (previousImage != data) {
            if (data == button->pressedImage && button->pressSoundFunc != NULL) {
                button->pressSoundFunc(button->id, button->lefMouseDownEventCode);
            } else if (data == button->normalImage && button->releaseSoundFunc != NULL) {
                button->releaseSoundFunc(button->id, button->leftMouseUpEventCode);
            }
        }
    }
}

// 0x4C5B10
void GNW_button_refresh(Window* w, Rect* rect)
{
    Button* button = w->buttonListHead;
    if (button != NULL) {
        while (button->next != NULL) {
            button = button->next;
        }
    }

    while (button != NULL) {
        button_draw(button, w, button->currentImage, false, rect, false);
        button = button->prev;
    }
}

// 0x4C5B58
int win_button_press_and_release(int btn)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Window* w;
    Button* button = GNW_find_button(btn, &w);
    if (button == NULL) {
        return -1;
    }

    button_draw(button, w, button->pressedImage, true, NULL, true);

    if (button->leftMouseDownProc != NULL) {
        button->leftMouseDownProc(btn, button->lefMouseDownEventCode);

        if ((button->flags & BUTTON_FLAG_0x40) != 0) {
            GNW_add_input_buffer(button->lefMouseDownEventCode);
        }
    } else {
        if (button->lefMouseDownEventCode != -1) {
            GNW_add_input_buffer(button->lefMouseDownEventCode);
        }
    }

    button_draw(button, w, button->normalImage, true, NULL, true);

    if (button->leftMouseUpProc != NULL) {
        button->leftMouseUpProc(btn, button->leftMouseUpEventCode);

        if ((button->flags & BUTTON_FLAG_0x40) != 0) {
            GNW_add_input_buffer(button->leftMouseUpEventCode);
        }
    } else {
        if (button->leftMouseUpEventCode != -1) {
            GNW_add_input_buffer(button->leftMouseUpEventCode);
        }
    }

    return 0;
}

} // namespace fallout
