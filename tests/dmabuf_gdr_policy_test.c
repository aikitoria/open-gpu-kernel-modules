/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stdint.h>

#include "../src/nvidia/arch/nvalloc/unix/include/dmabuf_gdr_policy.h"

int main(void)
{
    uint64_t windowStart = UINT64_C(0x20000000);
    uint64_t windowSize = UINT64_C(0x3dfe00000);
    uint64_t windowEnd = windowStart + windowSize;
    uint64_t emptySize = 0;

    assert(DMABUF_GDR_NONCOHERENT_ALLOWED(1, 0, 1, 1, 0, 0));
    assert(!DMABUF_GDR_NONCOHERENT_ALLOWED(0, 0, 1, 1, 0, 0));
    assert(!DMABUF_GDR_NONCOHERENT_ALLOWED(1, 1, 1, 1, 0, 0));
    assert(!DMABUF_GDR_NONCOHERENT_ALLOWED(1, 0, 0, 1, 0, 0));
    assert(!DMABUF_GDR_NONCOHERENT_ALLOWED(1, 0, 1, 0, 0, 0));
    assert(!DMABUF_GDR_NONCOHERENT_ALLOWED(1, 0, 1, 1, 1, 0));
    assert(!DMABUF_GDR_NONCOHERENT_ALLOWED(1, 0, 1, 1, 0, 1));

    assert(DMABUF_GDR_RANGE_CONTAINED(windowStart, UINT64_C(0x1000),
                                        windowStart, windowSize));
    assert(DMABUF_GDR_RANGE_CONTAINED(windowEnd - UINT64_C(0x1000),
                                        UINT64_C(0x1000), windowStart, windowSize));
    assert(!DMABUF_GDR_RANGE_CONTAINED(windowEnd - UINT64_C(0x1000),
                                         UINT64_C(0x2000), windowStart, windowSize));
    assert(!DMABUF_GDR_RANGE_CONTAINED(windowEnd, UINT64_C(0x1000),
                                         windowStart, windowSize));
    assert(!DMABUF_GDR_RANGE_CONTAINED(windowStart - UINT64_C(0x1000),
                                         UINT64_C(0x1000), windowStart, windowSize));
    assert(!DMABUF_GDR_RANGE_CONTAINED(windowStart, emptySize,
                                         windowStart, windowSize));
    assert(!DMABUF_GDR_RANGE_CONTAINED(UINT64_MAX - UINT64_C(0x1000),
                                         UINT64_C(0x2000), windowStart, windowSize));

    return 0;
}
