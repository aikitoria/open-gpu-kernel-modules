/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stdint.h>

#include "../kernel-open/nvidia/dmabuf-gdr-topology-policy.h"

int main(void)
{
    uint64_t barStart = UINT64_C(0x26000000000);
    uint64_t barSize = UINT64_C(0x400000000);
    uint64_t exactMask = barStart + barSize - 1;
    uint64_t wideMask = UINT64_C(0xffffffffffffffff);

    assert(DMABUF_GDR_BAR_ADDRESSABLE(barStart, barSize, exactMask));
    assert(DMABUF_GDR_BAR_ADDRESSABLE(barStart, barSize, wideMask));
    assert(!DMABUF_GDR_BAR_ADDRESSABLE(barStart, barSize, exactMask - 1));
    assert(!DMABUF_GDR_BAR_ADDRESSABLE(barStart, UINT64_C(0), wideMask));
    assert(!DMABUF_GDR_BAR_ADDRESSABLE(UINT64_MAX - UINT64_C(0x1000),
                                         UINT64_C(0x2000), UINT64_MAX));

    assert(DMABUF_GDR_TOPOLOGY_ALLOWED(1, 1, 4,
                                         barStart, barSize, wideMask));
    assert(!DMABUF_GDR_TOPOLOGY_ALLOWED(0, 1, 4,
                                          barStart, barSize, wideMask));
    assert(!DMABUF_GDR_TOPOLOGY_ALLOWED(1, 0, 4,
                                          barStart, barSize, wideMask));
    assert(!DMABUF_GDR_TOPOLOGY_ALLOWED(1, 1, -1,
                                          barStart, barSize, wideMask));
    assert(!DMABUF_GDR_TOPOLOGY_ALLOWED(1, 1, 4,
                                          barStart, barSize, exactMask - 1));

    return 0;
}
