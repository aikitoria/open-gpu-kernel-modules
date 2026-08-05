/* SPDX-License-Identifier: MIT */

#include <assert.h>

#include "../src/nvidia/src/kernel/gpu/bus/arch/turing/bar1_p2p_policy.h"

int main(void)
{
    int defaultEnabled;
    int fullCoverage;
    int gb206Exception;

    for (defaultEnabled = 0; defaultEnabled <= 1; ++defaultEnabled)
    {
        for (fullCoverage = 0; fullCoverage <= 1; ++fullCoverage)
        {
            for (gb206Exception = 0; gb206Exception <= 1; ++gb206Exception)
            {
                int oldAccepted = defaultEnabled && gb206Exception;
                int newAccepted = KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(
                    defaultEnabled, fullCoverage, gb206Exception);

                assert(!oldAccepted || newAccepted);
                assert(newAccepted ==
                       (defaultEnabled && (fullCoverage || gb206Exception)));
            }
        }
    }

    assert(KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, 1, 0));
    assert(KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, 0, 1));
    assert(!KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(1, 0, 0));
    assert(!KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(0, 1, 1));

    return 0;
}
