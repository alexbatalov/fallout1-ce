#include "plib/gnw/vcr.h"

#include <stdlib.h>

#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"

namespace fallout {

static bool vcr_create_buffer();
static bool vcr_destroy_buffer();
static bool vcr_clear_buffer();
static bool vcr_load_buffer();

// 0x51E2F0
VcrEntry* vcr_buffer = NULL;

// number of entries in vcr_buffer
// 0x51E2F4
int vcr_buffer_index = 0;

// 0x51E2F8
unsigned int vcr_state = VCR_STATE_TURNED_OFF;

// 0x51E2FC
unsigned int vcr_time = 0;

// 0x51E300
unsigned int vcr_counter = 0;

// 0x51E304
unsigned int vcr_terminate_flags = 0;

// 0x51E308
int vcr_terminated_condition = VCR_PLAYBACK_COMPLETION_REASON_NONE;

// 0x51E30C
static unsigned int vcr_start_time = 0;

// 0x51E310
static int vcr_registered_atexit = 0;

// 0x51E314
static DB_FILE* vcr_file = NULL;

// 0x51E318
static int vcr_buffer_end = 0;

// 0x51E31C
static VcrPlaybackCompletionCallback* vcr_notify_callback = NULL;

// 0x51E320
static unsigned int vcr_temp_terminate_flags = 0;

// 0x51E324
static int vcr_old_layout = 0;

// 0x6AD940
static VcrEntry vcr_last_play_event;

// 0x4D2680
bool vcr_record(const char* fileName)
{
    if (vcr_state != VCR_STATE_TURNED_OFF) {
        return false;
    }

    if (fileName == NULL) {
        return false;
    }

    // NOTE: Uninline.
    if (!vcr_create_buffer()) {
        return false;
    }

    vcr_file = db_fopen(fileName, "wb");
    if (vcr_file == NULL) {
        // NOTE: Uninline.
        vcr_destroy_buffer();
        return false;
    }

    if (vcr_registered_atexit == 0) {
        vcr_registered_atexit = atexit(vcr_stop);
    }

    VcrEntry* vcrEntry = &(vcr_buffer[vcr_buffer_index]);
    vcrEntry->type = VCR_ENTRY_TYPE_INITIAL_STATE;
    vcrEntry->time = 0;
    vcrEntry->counter = 0;
    vcrEntry->initial.keyboardLayout = kb_get_layout();

    while (mouse_get_buttons() != 0) {
        mouse_info();
    }

    mouse_get_position(&(vcrEntry->initial.mouseX), &(vcrEntry->initial.mouseY));

    vcr_counter = 1;
    vcr_buffer_index++;
    vcr_start_time = get_time();
    kb_clear();
    vcr_state = VCR_STATE_RECORDING;

    return true;
}

// 0x4D27EC
bool vcr_play(const char* fileName, unsigned int terminationFlags, VcrPlaybackCompletionCallback* callback)
{
    if (vcr_state != VCR_STATE_TURNED_OFF) {
        return false;
    }

    if (fileName == NULL) {
        return false;
    }

    // NOTE: Uninline.
    if (!vcr_create_buffer()) {
        return false;
    }

    vcr_file = db_fopen(fileName, "rb");
    if (vcr_file == NULL) {
        // NOTE: Uninline.
        vcr_destroy_buffer();
        return false;
    }

    if (!vcr_load_buffer()) {
        db_fclose(vcr_file);
        // NOTE: Uninline.
        vcr_destroy_buffer();
        return false;
    }

    while (mouse_get_buttons() != 0) {
        mouse_info();
    }

    kb_clear();

    vcr_temp_terminate_flags = terminationFlags;
    vcr_notify_callback = callback;
    vcr_terminated_condition = VCR_PLAYBACK_COMPLETION_REASON_COMPLETED;
    vcr_terminate_flags = 0;
    vcr_counter = 0;
    vcr_time = 0;
    vcr_start_time = get_time();
    vcr_state = VCR_STATE_PLAYING;
    vcr_last_play_event.time = 0;
    vcr_last_play_event.counter = 0;

    return true;
}

// 0x4D28F4
void vcr_stop()
{
    if (vcr_state == VCR_STATE_RECORDING || vcr_state == VCR_STATE_PLAYING) {
        vcr_state |= VCR_STATE_STOP_REQUESTED;
    }

    kb_clear();
}

// 0x4D2918
int vcr_status()
{
    return vcr_state;
}

// 0x4D2930
int vcr_update()
{
    if ((vcr_state & VCR_STATE_STOP_REQUESTED) != 0) {
        vcr_state &= ~VCR_STATE_STOP_REQUESTED;

        switch (vcr_state) {
        case VCR_STATE_RECORDING:
            vcr_dump_buffer();

            db_fclose(vcr_file);
            vcr_file = NULL;

            // NOTE: Uninline.
            vcr_destroy_buffer();

            break;
        case VCR_STATE_PLAYING:
            db_fclose(vcr_file);
            vcr_file = NULL;

            // NOTE: Uninline.
            vcr_destroy_buffer();

            kb_set_layout(vcr_old_layout);

            if (vcr_notify_callback != NULL) {
                vcr_notify_callback(vcr_terminated_condition);
            }
            break;
        }

        vcr_state = VCR_STATE_TURNED_OFF;
    }

    switch (vcr_state) {
    case VCR_STATE_RECORDING:
        vcr_counter++;
        vcr_time = elapsed_time(vcr_start_time);
        if (vcr_buffer_index == VCR_BUFFER_CAPACITY - 1) {
            vcr_dump_buffer();
        }
        break;
    case VCR_STATE_PLAYING:
        if (vcr_buffer_index < vcr_buffer_end || vcr_load_buffer()) {
            VcrEntry* vcrEntry = &(vcr_buffer[vcr_buffer_index]);
            if (vcr_last_play_event.counter < vcrEntry->counter) {
                if (vcrEntry->time > vcr_last_play_event.time) {
                    unsigned int delay = vcr_last_play_event.time;
                    delay += (vcr_counter - vcr_last_play_event.counter)
                        * (vcrEntry->time - vcr_last_play_event.time)
                        / (vcrEntry->counter - vcr_last_play_event.counter);

                    while (elapsed_time(vcr_start_time) < delay) {
                    }
                }
            }

            vcr_counter++;

            int rc = 0;
            while (vcr_counter >= vcr_buffer[vcr_buffer_index].counter) {
                vcr_time = elapsed_time(vcr_start_time);
                if (vcr_time > vcr_buffer[vcr_buffer_index].time + 5
                    || vcr_time < vcr_buffer[vcr_buffer_index].time - 5) {
                    vcr_start_time += vcr_time - vcr_buffer[vcr_buffer_index].time;
                }

                switch (vcr_buffer[vcr_buffer_index].type) {
                case VCR_ENTRY_TYPE_INITIAL_STATE:
                    vcr_state = VCR_STATE_TURNED_OFF;
                    vcr_old_layout = kb_get_layout();
                    kb_set_layout(vcr_buffer[vcr_buffer_index].initial.keyboardLayout);
                    while (mouse_get_buttons() != 0) {
                        mouse_info();
                    }
                    vcr_state = VCR_ENTRY_TYPE_INITIAL_STATE;
                    mouse_hide();
                    mouse_set_position(vcr_buffer[vcr_buffer_index].initial.mouseX, vcr_buffer[vcr_buffer_index].initial.mouseY);
                    mouse_show();
                    kb_clear();
                    vcr_terminate_flags = vcr_temp_terminate_flags;
                    vcr_start_time = get_time();
                    vcr_counter = 0;
                    break;
                case VCR_ENTRY_TYPE_KEYBOARD_EVENT:
                    if (1) {
                        KeyboardData keyboardData;
                        keyboardData.key = vcr_buffer[vcr_buffer_index].keyboardEvent.key;
                        kb_simulate_key(&keyboardData);
                    }
                    break;
                case VCR_ENTRY_TYPE_MOUSE_EVENT:
                    rc = 3;
                    mouse_simulate_input(vcr_buffer[vcr_buffer_index].mouseEvent.dx, vcr_buffer[vcr_buffer_index].mouseEvent.dy, vcr_buffer[vcr_buffer_index].mouseEvent.buttons);
                    break;
                }

                vcr_last_play_event = vcr_buffer[vcr_buffer_index];
                vcr_buffer_index++;
            }

            return rc;
        } else {
            // NOTE: Uninline.
            vcr_stop();
        }
        break;
    }

    return 0;
}

// NOTE: Inlined.
//
// 0x4D2C64
static bool vcr_create_buffer()
{
    if (vcr_buffer == NULL) {
        vcr_buffer = (VcrEntry*)mem_malloc(sizeof(*vcr_buffer) * VCR_BUFFER_CAPACITY);
        if (vcr_buffer == NULL) {
            return false;
        }
    }

    // NOTE: Uninline.
    vcr_clear_buffer();

    return true;
}

// NOTE: Inlined.
//
// 0x4D2C98
static bool vcr_destroy_buffer()
{
    if (vcr_buffer == NULL) {
        return false;
    }

    // NOTE: Uninline.
    vcr_clear_buffer();

    mem_free(vcr_buffer);
    vcr_buffer = NULL;

    return true;
}

// 0x4D2CD0
static bool vcr_clear_buffer()
{
    if (vcr_buffer == NULL) {
        return false;
    }

    vcr_buffer_index = 0;

    return true;
}

// 0x4D2CF0
bool vcr_dump_buffer()
{
    if (vcr_buffer == NULL) {
        return false;
    }

    if (vcr_file == NULL) {
        return false;
    }

    for (int index = 0; index < vcr_buffer_index; index++) {
        if (!vcr_save_record(&(vcr_buffer[index]), vcr_file)) {
            return false;
        }
    }

    // NOTE: Uninline.
    if (!vcr_clear_buffer()) {
        return false;
    }

    return true;
}

// 0x4D2D74
static bool vcr_load_buffer()
{
    if (vcr_file == NULL) {
        return false;
    }

    // NOTE: Uninline.
    if (!vcr_clear_buffer()) {
        return false;
    }

    for (vcr_buffer_end = 0; vcr_buffer_end < VCR_BUFFER_CAPACITY; vcr_buffer_end++) {
        if (!vcr_load_record(&(vcr_buffer[vcr_buffer_end]), vcr_file)) {
            break;
        }
    }

    if (vcr_buffer_end == 0) {
        return false;
    }

    return true;
}

// 0x4D2E00
bool vcr_save_record(VcrEntry* vcrEntry, DB_FILE* stream)
{
    if (db_fwriteUInt32(stream, vcrEntry->type) == -1) return false;
    if (db_fwriteUInt32(stream, vcrEntry->time) == -1) return false;
    if (db_fwriteUInt32(stream, vcrEntry->counter) == -1) return false;

    switch (vcrEntry->type) {
    case VCR_ENTRY_TYPE_INITIAL_STATE:
        if (db_fwriteInt32(stream, vcrEntry->initial.mouseX) == -1) return false;
        if (db_fwriteInt32(stream, vcrEntry->initial.mouseY) == -1) return false;
        if (db_fwriteInt32(stream, vcrEntry->initial.keyboardLayout) == -1) return false;
        return true;
    case VCR_ENTRY_TYPE_KEYBOARD_EVENT:
        if (db_fwriteInt16(stream, vcrEntry->keyboardEvent.key) == -1) return false;
        return true;
    case VCR_ENTRY_TYPE_MOUSE_EVENT:
        if (db_fwriteInt32(stream, vcrEntry->mouseEvent.dx) == -1) return false;
        if (db_fwriteInt32(stream, vcrEntry->mouseEvent.dy) == -1) return false;
        if (db_fwriteInt32(stream, vcrEntry->mouseEvent.buttons) == -1) return false;
        return true;
    }

    return false;
}

// 0x4D2EE4
bool vcr_load_record(VcrEntry* vcrEntry, DB_FILE* stream)
{
    if (db_freadUInt32(stream, &(vcrEntry->type)) == -1) return false;
    if (db_freadUInt32(stream, &(vcrEntry->time)) == -1) return false;
    if (db_freadUInt32(stream, &(vcrEntry->counter)) == -1) return false;

    switch (vcrEntry->type) {
    case VCR_ENTRY_TYPE_INITIAL_STATE:
        if (db_freadInt32(stream, &(vcrEntry->initial.mouseX)) == -1) return false;
        if (db_freadInt32(stream, &(vcrEntry->initial.mouseY)) == -1) return false;
        if (db_freadInt32(stream, &(vcrEntry->initial.keyboardLayout)) == -1) return false;
        return true;
    case VCR_ENTRY_TYPE_KEYBOARD_EVENT:
        if (db_freadInt16(stream, &(vcrEntry->keyboardEvent.key)) == -1) return false;
        return true;
    case VCR_ENTRY_TYPE_MOUSE_EVENT:
        if (db_freadInt32(stream, &(vcrEntry->mouseEvent.dx)) == -1) return false;
        if (db_freadInt32(stream, &(vcrEntry->mouseEvent.dy)) == -1) return false;
        if (db_freadInt32(stream, &(vcrEntry->mouseEvent.buttons)) == -1) return false;
        return true;
    }

    return false;
}

} // namespace fallout
