/*
 * SPDX-FileCopyrightText: Copyright (c) 2019-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef _NV_PCI_H_
#define _NV_PCI_H_

#include <linux/pci.h>
#include "nv-linux.h"

#define NV_GPU_BAR1 1
#define NV_GPU_BAR3 3

int nv_pci_register_driver(void);
void nv_pci_unregister_driver(void);
int nv_pci_count_devices(void);
NvU8 nv_find_pci_capability(struct pci_dev *, NvU8);
int nvidia_dev_get_pci_info(const NvU8 *, struct pci_dev **, NvU64 *, NvU64 *);
nv_linux_state_t * find_pci(NvU32, NvU8, NvU8, NvU8);
NvBool nv_pci_is_valid_topology_for_direct_pci(nv_state_t *, struct pci_dev *, NvBool *);
NvBool nv_pci_has_common_pci_switch(nv_state_t *nv, struct pci_dev *);
void nv_pci_tegra_boost_clocks(struct device *dev);

/* Register Block Identifier (RBI) */
enum cxl_regloc_type {
    CXL_REGLOC_RBI_EMPTY = 0,
    CXL_REGLOC_RBI_COMPONENT,
    CXL_REGLOC_RBI_VIRT,
    CXL_REGLOC_RBI_MEMDEV,
    CXL_REGLOC_RBI_PMU,
    CXL_REGLOC_RBI_TYPES
};

#define CXL_CM_OFFSET 0x1000
#define CXL_CM_CAP_HDR_OFFSET 0x0
#define CXL_CM_CAP_HDR_ID_MASK GENMASK(15, 0)
#define CM_CAP_HDR_CAP_ID 1
#define CXL_CM_CAP_HDR_ARRAY_SIZE_MASK GENMASK(31, 24)
#define CXL_CM_CAP_PTR_MASK GENMASK(31, 20)
#define CXL_CM_CAP_CAP_ID_HDM 0x5
#define CXL_HDM_DECODER_CAP_OFFSET 0x0
#define CXL_HDM_DECODER0_BASE_LOW_OFFSET(i) (0x20 * (i) + 0x10)
#define CXL_HDM_DECODER0_BASE_HIGH_OFFSET(i) (0x20 * (i) + 0x14)
#define CXL_HDM_DECODER0_SIZE_LOW_OFFSET(i) (0x20 * (i) + 0x18)
#define CXL_HDM_DECODER0_SIZE_HIGH_OFFSET(i) (0x20 * (i) + 0x1c)
#define CXL_HDM_DECODER0_CTRL_OFFSET(i) (0x20 * (i) + 0x20)
#define CXL_HDM_DECODER0_CTRL_COMMITTED BIT(10)

#endif
