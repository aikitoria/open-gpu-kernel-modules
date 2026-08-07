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

#ifndef DMABUF_GDR_TOPOLOGY_POLICY_H
#define DMABUF_GDR_TOPOLOGY_POLICY_H

/* Overflow-safe check that the importer can address the complete BAR. */
#define DMABUF_GDR_BAR_ADDRESSABLE(barStart, barSize, dmaMask) \
    (((barSize) != 0) && ((barStart) <= (dmaMask)) && \
     (((barSize) - 1) <= ((dmaMask) - (barStart))))

/*
 * Keep the non-coherent topology exception deliberately narrow. Linux must
 * approve the complete P2PDMA path and the importer must be in an identity
 * IOMMU domain. The experimental path then uses dma_map_resource() instead of
 * the stock FORCE_PCIE IOMMU bypass.
 */
#define DMABUF_GDR_TOPOLOGY_ALLOWED(enabled, identityIommu, p2pDistance, \
                                       barStart, barSize, dmaMask) \
    ((enabled) && (identityIommu) && \
     ((p2pDistance) >= 0) && \
     DMABUF_GDR_BAR_ADDRESSABLE((barStart), (barSize), (dmaMask)))

#endif // DMABUF_GDR_TOPOLOGY_POLICY_H
