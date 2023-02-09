#ifndef FALLOUT_GAME_COUNTER_H_
#define FALLOUT_GAME_COUNTER_H_

namespace fallout {

typedef void(CounterOutputFunc)(double a1);

void counter_on(CounterOutputFunc* outputFunc);
void counter_off();

} // namespace fallout

#endif /* FALLOUT_GAME_COUNTER_H_ */
