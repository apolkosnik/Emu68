/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _MMU_H
#define _MMU_H

#include <stdint.h>

#ifndef __aarch64__
/*
 * ARM32 uses 1 MB section descriptors in the early MMU setup. These macros
 * keep the board code source-compatible with the AArch64 path while mapping
 * to the section flags already used in start_rpi.c.
 */
#ifndef MMU_ACCESS
#define MMU_ACCESS      0x0000
#endif
#ifndef MMU_ISHARE
#define MMU_ISHARE      0x0000
#endif
#ifndef MMU_READ_ONLY
#define MMU_READ_ONLY   0x0000
#endif
#ifndef MMU_ALLOW_EL0
#define MMU_ALLOW_EL0   0x0000
#endif
#ifndef MMU_ATTR_CACHED
#define MMU_ATTR_CACHED 0x1c0e
#endif
#ifndef MMU_ATTR_DEVICE
#define MMU_ATTR_DEVICE 0x0c06
#endif
#endif

void mmu_init();
uintptr_t mmu_virt2phys(uintptr_t addr);
void mmu_map(uintptr_t phys, uintptr_t virt, uintptr_t length, uint32_t attr_low, uint32_t attr_high);

#endif /* _MMU_H */
