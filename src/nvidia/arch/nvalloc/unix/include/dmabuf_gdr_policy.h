/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Duc P. Tran
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef DMABUF_GDR_POLICY_H
#define DMABUF_GDR_POLICY_H

/*
 * The exception is deliberately narrower than the stock coherent path. It is
 * available only for an explicitly enabled non-coherent FORCE_PCIE export
 * backed by an active static BAR1 mapping, with BAR1 and MIG exclusions
 * retained. Coherent GPUs continue to use the stock path.
 */
#define DMABUF_GDR_NONCOHERENT_ALLOWED(enabled, coherent, forcePcie, \
                                        staticBar1Enabled, bar1Disabled, migEnabled) \
    ((enabled) && !(coherent) && (forcePcie) && (staticBar1Enabled) && \
     !(bar1Disabled) && !(migEnabled))

/*
 * The RmGpuDirectRdmaForceSPA hypervisor workaround is a coherent-platform
 * address-translation override, not a DMA-BUF GDR capability requirement.
 * It must stay unreachable for non-coherent GPUs regardless of whether the
 * experimental non-coherent FORCE_PCIE path is permitted.
 */
#define DMABUF_GDR_USE_GRDMA_SPA(forcePcie, coherent, forceSpa) \
    ((forcePcie) && (coherent) && (forceSpa))

/*
 * Overflow-safe half-open range containment. Subtraction is performed only
 * after ordering checks, and size is compared against the remaining window.
 */
#define DMABUF_GDR_RANGE_CONTAINED(start, size, windowStart, windowSize) \
    (((size) != 0) && ((windowSize) != 0) && ((start) >= (windowStart)) && \
     (((start) - (windowStart)) <= (windowSize)) && \
     ((size) <= ((windowSize) - ((start) - (windowStart)))))

#endif // DMABUF_GDR_POLICY_H
