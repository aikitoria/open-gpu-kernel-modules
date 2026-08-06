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

#ifndef KERN_BUS_BAR1_P2P_POLICY_H
#define KERN_BUS_BAR1_P2P_POLICY_H

/*
 * Display-aware placement is additive for default-enabled devices with a
 * non-empty aligned client FB range and a non-empty aligned static BAR1
 * window. Partial windows are safe because each external mapping is checked
 * against the selected static BAR1 DMA window before its addresses are used.
 */
#define KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(defaultEnabled, clientFbSize, maxStaticMapSize) \
    ((defaultEnabled) && ((clientFbSize) != 0) && ((maxStaticMapSize) != 0))

#endif // KERN_BUS_BAR1_P2P_POLICY_H
