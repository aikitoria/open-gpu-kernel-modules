/* SPDX-License-Identifier: MIT */

#include <assert.h>

#include "../src/nvidia/src/kernel/gpu/bus/arch/turing/bar1_p2p_policy.h"

int main(void)
{
    const unsigned long long clientFbSize = 16ULL << 30;
    const unsigned long long partialStaticSize = 15ULL << 30;

    assert(!KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(0, clientFbSize,
                                                  partialStaticSize));
    assert(!KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, 0,
                                                  partialStaticSize));
    assert(!KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, clientFbSize, 0));

    assert(KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, clientFbSize,
                                                 partialStaticSize));
    assert(KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, clientFbSize,
                                                 clientFbSize));
    assert(KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, clientFbSize,
                                                 clientFbSize + (1ULL << 30)));

    return 0;
}
