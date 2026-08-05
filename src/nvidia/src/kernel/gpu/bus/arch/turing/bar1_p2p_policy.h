/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Duc P. Tran
 * SPDX-License-Identifier: MIT
 */

#ifndef KERN_BUS_BAR1_P2P_POLICY_H
#define KERN_BUS_BAR1_P2P_POLICY_H

/*
 * Display-aware placement is additive for default-enabled devices whose
 * runtime BAR1 geometry covers all aligned client FB. GB206 keeps its tested
 * partial-window exception; other partial windows are not generalized.
 */
#define KBUS_USE_DISPLAY_AWARE_STATIC_BAR1(defaultEnabled, fullCoverage, gb206Exception) \
    ((defaultEnabled) && ((fullCoverage) || (gb206Exception)))

#endif // KERN_BUS_BAR1_P2P_POLICY_H
