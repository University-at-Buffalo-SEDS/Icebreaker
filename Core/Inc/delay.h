#ifndef ICEBREAKER_DELAY_H
#define ICEBREAKER_DELAY_H

#include <stdbool.h>
#include <stdint.h>

/* Call after clock configuration, and again if the core clock changes.
 * Returns false if the clock is not a whole MHz or CYCCNT is unavailable.
 */
bool delay_init(void);

/* Blocking minimum delay in microseconds; zero returns immediately.
 * Requires successful delay_init(). Interrupts remain enabled and may extend
 * the delay. Do not stop/reset CYCCNT or change the clock during a call.
 */
void delay_us(uint32_t microseconds);

#endif
