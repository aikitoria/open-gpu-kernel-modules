/*
 * SPDX-FileCopyrightText: Copyright (c) 2019-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "core/core.h"
#include "gpu/gpu.h"
#include "gpu/mem_mgr/mem_mgr.h"
#include "gpu/mmu/kern_gmmu.h"
#include "gpu/bus/kern_bus.h"
#include "gpu/gsp/gsp_static_config.h"
#include <ctrl/ctrl2080/ctrl2080fb.h>
#include "gpu/mem_mgr/fermi_dma.h"
#include "gpu/mem_mgr/rm_page_size.h"
#include "nvrm_registry.h"
#include "nvoc/prelude.h"

/*!
 * @brief Size BAR1 directly via PCI config space (write-ones probe), because
 *        KernelBus->pciBarSizes[] is not populated yet when the GSP FB region
 *        table is parsed. Uses the same osPci* accessors as kern_gpu_gb202.c,
 *        but does not cache the handle in pGpu->hPci.
 */
static NvU64
_p2pPciBar1Size
(
    OBJGPU *pGpu
)
{
    void *h = pGpu->hPci;
    NvU32 cmd, lo, hi, slo, shi;
    NvU64 size;
    NvBool bBar1Is64;

    if (h == NULL)
    {
        h = osPciInitHandle(gpuGetDomain(pGpu), gpuGetBus(pGpu),
                            gpuGetDevice(pGpu), 0 /* function */, NULL, NULL);
    }
    if (h == NULL)
        return 0;

    lo = osPciReadDword(h, 0x14);
    bBar1Is64 = ((lo & 0x4) != 0);
    hi = bBar1Is64 ? osPciReadDword(h, 0x18) : 0;

    cmd = osPciReadDword(h, 0x04);

    // Temporarily disable memory space decoding while sizing the BAR.
    osPciWriteDword(h, 0x04, cmd & ~0x2);

    osPciWriteDword(h, 0x14, 0xFFFFFFFF);
    if (bBar1Is64)
        osPciWriteDword(h, 0x18, 0xFFFFFFFF);
    slo = osPciReadDword(h, 0x14) & ~0xFu;
    shi = bBar1Is64 ? osPciReadDword(h, 0x18) : 0;

    osPciWriteDword(h, 0x14, lo);
    if (bBar1Is64)
        osPciWriteDword(h, 0x18, hi);

    osPciWriteDword(h, 0x04, cmd);

    if ((slo == 0) && (shi == 0))
        return 0;

    size = ~(((NvU64)shi << 32) | (NvU64)slo) + 1;
    return (size == 0) ? 0 : size;
}

//
// Registry key RMP2PFbTailReserveMb (see nvrm_registry.h):
//   0 (default) - disabled, no trim
//   1           - adaptive per-GPU trim (exactly the static BAR1 shortfall)
//   N (>1)      - fixed trim of N MiB
//
#define P2P_FB_TAIL_RESERVE_MARGIN_BYTES (16ULL << 20)

/*!
 * @brief Trim the top of the usable FB by a registry-controlled amount so
 *        boards whose BAR1 size equals their VRAM size can satisfy the
 *        static BAR1 P2P requirement.
 *
 *        The static BAR1 requirement (BAR1 >= clientFB + 512 MiB alignment
 *        floor for the console/mailbox area) misses by only a few MiB when
 *        BAR1 == VRAM (e.g. 16 GiB / 16 GiB), so BAR1 P2P can never be
 *        enabled on such boards. Trimming the top of the usable FB by the
 *        shortfall makes the requirement fit, at the cost of that much
 *        usable VRAM (per GPU).
 *
 *        The trim folds the top of the topmost usable region into the
 *        reserved region directly above it (the GSP layout always reserves
 *        the top of the FB), so the heap/PMA can never allocate it. A usable
 *        region on top of the FB is left untrimmed (with a warning), since
 *        shrinking the top region would understate fbAddrSpaceSizeMb.
 */
static void
_p2pApplyFbTailReserve
(
    OBJGPU        *pGpu,
    MemoryManager *pMemoryManager,
    NvU32          numFBRegions
)
{
    NvU32  tailReserveMb = 0;
    NvU32  last          = numFBRegions - 1;
    NvU32  u;
    NvU64  trimBytes     = 0;
    NvU64  bar1Size;
    NvBool bTrimmed      = NV_FALSE;

    if ((osReadRegistryDword(pGpu, NV_REG_STR_RM_P2P_FB_TAIL_RESERVE_MB, &tailReserveMb) != NV_OK) ||
        (tailReserveMb == NV_REG_STR_RM_P2P_FB_TAIL_RESERVE_MB_DISABLED))
    {
        return;
    }

    if (tailReserveMb == NV_REG_STR_RM_P2P_FB_TAIL_RESERVE_MB_ADAPTIVE)
    {
        //
        // Adaptive: trim exactly what static BAR1 needs on this GPU,
        // assuming the worst case 512 MiB console/mailbox alignment floor
        // (so the GPU stays P2P-capable even if it later drives a display),
        // plus margin for the BAR1-mappable length being slightly below the
        // PCI BAR size. GPUs that already fit are not trimmed at all.
        //
        bar1Size = _p2pPciBar1Size(pGpu);
        if (bar1Size == 0)
        {
            KernelBus *pKernelBus = GPU_GET_KERNEL_BUS(pGpu);

            bar1Size = (pKernelBus != NULL) ? kbusGetPciBarSize(pKernelBus, 1) : 0;
        }

        if (bar1Size == 0)
        {
            NV_PRINTF(LEVEL_WARNING,
                      "P2P FB tail reserve: BAR1 size unknown, skipping adaptive trim\n");
            return;
        }

        NvU64 bar1Usable = RM_ALIGN_DOWN(bar1Size, RM_PAGE_SIZE_2M);
        NvU64 required   = pMemoryManager->Ram.fbUsableMemSize + RM_PAGE_SIZE_512M;

        if (required > bar1Usable)
        {
            trimBytes = RM_ALIGN_UP(required + P2P_FB_TAIL_RESERVE_MARGIN_BYTES - bar1Usable,
                                    RM_PAGE_SIZE_2M);
        }
    }
    else
    {
        trimBytes = (NvU64)tailReserveMb << 20;
    }

    if ((trimBytes == 0) || (last < 1))
    {
        return;
    }

    //
    // Fold the trim into the reserved region on top of the FB: find the
    // topmost usable region below the reserved region(s) at the top (the GSP
    // layout always reserves the top of the FB), trim that region's limit,
    // and extend the reserved region directly above it downward, keeping the
    // region chain contiguous. A usable region on top of the FB is left
    // untrimmed, since shrinking the top region would understate
    // fbAddrSpaceSizeMb, which is derived from the top region's limit below.
    //
    if (pMemoryManager->Ram.fbRegion[last].bRsvdRegion)
    {
        u = last;
        while ((u != 0) && pMemoryManager->Ram.fbRegion[u].bRsvdRegion)
        {
            u--;
        }

        if (!pMemoryManager->Ram.fbRegion[u].bRsvdRegion &&
            pMemoryManager->Ram.fbRegion[u + 1].bRsvdRegion &&
            (pMemoryManager->Ram.fbRegion[u + 1].base ==
                pMemoryManager->Ram.fbRegion[u].limit + 1) &&
            (pMemoryManager->Ram.fbRegion[u].limit -
             pMemoryManager->Ram.fbRegion[u].base + 1) > trimBytes)
        {
            pMemoryManager->Ram.fbRegion[u].limit       -= trimBytes;
            pMemoryManager->Ram.fbRegion[u + 1].base    -= trimBytes;
            pMemoryManager->Ram.fbRegion[u + 1].rsvdSize += trimBytes;
            pMemoryManager->Ram.fbUsableMemSize         -= trimBytes;
            pMemoryManager->Ram.reservedMemSize         += trimBytes;
            bTrimmed = NV_TRUE;
        }
    }

    if (!bTrimmed)
    {
        NV_PRINTF(LEVEL_WARNING,
                  "P2P FB tail reserve: no trimmable usable region found below the reserved top, skipping 0x%llx byte trim\n",
                  trimBytes);
    }
}

/*!
 * @brief Initialize FB regions from static info obtained from GSP FW. Also,
 *        initialize region table related RAM fields.
 */
NV_STATUS
memmgrInitBaseFbRegions_FWCLIENT
(
    OBJGPU        *pGpu,
    MemoryManager *pMemoryManager
)
{
    NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS *pFbRegionInfoParams;
    NV2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO *pFbRegionInfo;
    GspStaticConfigInfo *pGSCI = GPU_GET_GSP_STATIC_INFO(pGpu);
    NvU64 bias;
    NvU32 i;

    // sanity checks
    if (pGSCI == NULL)
    {
        NV_PRINTF(LEVEL_ERROR, "Missing static info.\n");

        return NV_ERR_INVALID_STATE;
    }

    pFbRegionInfoParams = &pGSCI->fbRegionInfoParams;
    if (pFbRegionInfoParams->numFBRegions == 0)
    {
        NV_PRINTF(LEVEL_ERROR,
                  "Missing FB region table in GSP Init arguments.\n");

        return NV_ERR_INVALID_PARAMETER;
    }

    if (pFbRegionInfoParams->numFBRegions > MAX_FB_REGIONS)
    {
        NV_PRINTF(LEVEL_ERROR,
                  "Static info struct has more FB regions (%u) than FB supports (%u).\n",
                  pFbRegionInfoParams->numFBRegions, MAX_FB_REGIONS);

        return NV_ERR_INVALID_PARAMETER;
    }

    pMemoryManager->Ram.reservedMemSize = 0;
    pMemoryManager->Ram.fbUsableMemSize = 0;

    // Copy FB regions from static info structure
    for (i = 0; i < pFbRegionInfoParams->numFBRegions; i++)
    {
        pFbRegionInfo = &pFbRegionInfoParams->fbRegion[i];
        pMemoryManager->Ram.fbRegion[i].base               = pFbRegionInfo->base;
        pMemoryManager->Ram.fbRegion[i].limit              = pFbRegionInfo->limit;
        pMemoryManager->Ram.fbRegion[i].bProtected         = pFbRegionInfo->bProtected;
        pMemoryManager->Ram.fbRegion[i].bInternalHeap      = NV_FALSE;
        pMemoryManager->Ram.fbRegion[i].performance        = pFbRegionInfo->performance;
        pMemoryManager->Ram.fbRegion[i].bSupportCompressed = pFbRegionInfo->supportCompressed;
        pMemoryManager->Ram.fbRegion[i].bSupportISO        = pFbRegionInfo->supportISO;
        pMemoryManager->Ram.fbRegion[i].rsvdSize           = pFbRegionInfo->reserved;
        pMemoryManager->Ram.fbRegion[i].regionTag          = pFbRegionInfo->regionTag;
        // CPU-RM is not responsible for saving GSP-RM allocations
        pMemoryManager->Ram.fbRegion[i].bLostOnSuspend     = NV_TRUE;

        if (pFbRegionInfo->reserved)
        {
            pMemoryManager->Ram.fbRegion[i].bRsvdRegion = NV_TRUE;
            pMemoryManager->Ram.reservedMemSize += pMemoryManager->Ram.fbRegion[i].rsvdSize;
        }
        else
        {
            pMemoryManager->Ram.fbRegion[i].bRsvdRegion = NV_FALSE;
            pMemoryManager->Ram.fbUsableMemSize += (pMemoryManager->Ram.fbRegion[i].limit -
                                            pMemoryManager->Ram.fbRegion[i].base + 1);
        }
    }
    pMemoryManager->Ram.numFBRegions = pFbRegionInfoParams->numFBRegions;

    //
    // P2P tail reserve: optionally trim the top of the usable FB (see
    // _p2pApplyFbTailReserve) so BAR1 == VRAM boards can pass the static
    // BAR1 P2P check.
    //
    _p2pApplyFbTailReserve(pGpu, pMemoryManager, pFbRegionInfoParams->numFBRegions);

    // Round up to the closest megabyte.
    bias = (1 << 20) - 1;
    //
    // fbTotalMemSizeMb was set to fbUsableMemSize. However, in RM-offload,
    // GSP-RM reserves some FB regions for its own usage, thus fbUsableMemSize
    // won't represent the exact FB size. Instead, we are taking the FB size
    // from the static info provided by GSP-RM.
    //
    pMemoryManager->Ram.fbTotalMemSizeMb  = (pGSCI->fb_length + bias) >> 20;
    pMemoryManager->Ram.fbAddrSpaceSizeMb =
        (pMemoryManager->Ram.fbRegion[pFbRegionInfoParams->numFBRegions - 1].limit + bias) >> 20;

    NV_ASSERT(pMemoryManager->Ram.fbAddrSpaceSizeMb >= pMemoryManager->Ram.fbTotalMemSizeMb);

    // Dump some stats, region table is dumped in memsysStateLoad
    NV_PRINTF(LEVEL_INFO, "FB Memory from Static info:\n");
    NV_PRINTF(LEVEL_INFO, "Reserved Memory=0x%llx, Usable Memory=0x%llx\n",
              pMemoryManager->Ram.reservedMemSize, pMemoryManager->Ram.fbUsableMemSize);
    NV_PRINTF(LEVEL_INFO, "fbTotalMemSizeMb=0x%llx, fbAddrSpaceSizeMb=0x%llx\n",
              pMemoryManager->Ram.fbTotalMemSizeMb, pMemoryManager->Ram.fbAddrSpaceSizeMb);

    return NV_OK;
}
