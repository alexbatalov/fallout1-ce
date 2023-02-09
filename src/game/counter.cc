#include "game/counter.h"

#include <time.h>

#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"

namespace fallout {

static void counter();

// 0x504D28
static int counter_is_on = 0;

// 0x504D2C
static unsigned char count = 0;

// 0x504D30
static clock_t last_time = 0;

// 0x504D34
static CounterOutputFunc* counter_output_func;

// 0x426F10
void counter_on(CounterOutputFunc* outputFunc)
{
    if (!counter_is_on) {
        debug_printf("Turning on counter...\n");
        add_bk_process(counter);
        counter_output_func = outputFunc;
        counter_is_on = 1;
        last_time = clock();
    }
}

// 0x426F54
void counter_off()
{
    if (counter_is_on) {
        remove_bk_process(counter);
        counter_is_on = 0;
    }
}

// 0x426F74
static void counter()
{
    // 0x56BED0
    static clock_t this_time;

    count++;
    if (count == 0) {
        this_time = clock();
        if (counter_output_func != NULL) {
            counter_output_func(256.0 / (this_time - last_time) / 100.0);
        }
        last_time = this_time;
    }
}

} // namespace fallout
