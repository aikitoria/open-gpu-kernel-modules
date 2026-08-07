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

    /* forcePcie=1, coherent=1, forceSpa=1 -> SPA */
    assert(DMABUF_GDR_USE_GRDMA_SPA(1, 1, 1));
    /* forcePcie=1, coherent=1, forceSpa=0 -> normal FB base */
    assert(!DMABUF_GDR_USE_GRDMA_SPA(1, 1, 0));
    /* forcePcie=1, coherent=0, forceSpa=1 -> normal FB base (regression case) */
    assert(!DMABUF_GDR_USE_GRDMA_SPA(1, 0, 1));
    /* forcePcie=1, coherent=0, forceSpa=0 -> normal FB base */
    assert(!DMABUF_GDR_USE_GRDMA_SPA(1, 0, 0));
    /* forcePcie=0 never selects SPA regardless of coherence/forceSpa */
    assert(!DMABUF_GDR_USE_GRDMA_SPA(0, 1, 1));
    assert(!DMABUF_GDR_USE_GRDMA_SPA(0, 0, 1));

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
