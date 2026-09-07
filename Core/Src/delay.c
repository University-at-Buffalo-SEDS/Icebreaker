#include "delay.h"
#include "main.h"

static uint32_t cycles_per_us;

bool delay_init(void)
{
    cycles_per_us = 0U;
    SystemCoreClockUpdate();
    if (SystemCoreClock < 1000000U || SystemCoreClock % 1000000U != 0U) {
        return false;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    __DSB();
    __ISB();
    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U) {
        return false;
    }
    /* Preserve the counter value so other cycle-counter users are unaffected. */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
    uint32_t start = DWT->CYCCNT;
    for (uint32_t i = 0U; i < 32U; ++i) {
        __NOP();
    }
    if (DWT->CYCCNT == start) {
        return false;
    }

    cycles_per_us = SystemCoreClock / 1000000U;
    return true;
}

void delay_us(uint32_t microseconds)
{
    if (microseconds == 0U) {
        return;
    }
    if (cycles_per_us == 0U) {
        Error_Handler();
        return;
    }

    /* Bound multiplication and keep each wait below half the counter range. */
    const uint32_t max_chunk_us = (UINT32_MAX / 2U) / cycles_per_us;
    while (microseconds != 0U) {
        uint32_t chunk = microseconds > max_chunk_us ? max_chunk_us : microseconds;
        uint32_t cycles = chunk * cycles_per_us;
        uint32_t start = DWT->CYCCNT;
        /* Unsigned subtraction also works when the 32-bit counter wraps. */
        while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
            __NOP();
        }
        microseconds -= chunk;
    }
}
