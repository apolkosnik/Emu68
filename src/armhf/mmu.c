/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include "mmu.h"
#include "support.h"
#include "tlsf.h"
#include "devicetree.h"

/*
 * ARM32 uses a flat first-level page table of 4096 x 1 MB section
 * descriptors.  The table is defined in start.c and exported here
 * so that mmu_map() can update entries at run-time.
 */
extern uint32_t mmu_table[4096];

struct MemoryBlock *sys_memory;

void mmu_init()
{
    of_node_t *e = dt_find_node("/memory");

    if (e)
    {
        of_property_t *p = dt_find_property(e, "reg");
        uint32_t *range = p->op_value;
        int size_cells = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);
        int address_cells = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
        int block_size = 4 * (size_cells + address_cells);
        int block_count = p->op_length / block_size;

        sys_memory = tlsf_malloc(tlsf, (1 + block_count) * sizeof(struct MemoryBlock));
        for (int block = 0; block < block_count; block++)
        {
            uintptr_t addr = 0;
            uintptr_t size = 0;

            /* On ARM32, uintptr_t is 32-bit. Use only the last cell. */
            for (int i = 0; i < address_cells; i++)
            {
                addr = BE32(range[i]);
            }
            for (int i = 0; i < size_cells; i++)
            {
                size = BE32(range[i + address_cells]);
            }

#ifdef PISTORM
            /* Adjust base and size of the memory block */
            if (addr < 0x01000000) {
                size -= 0x01000000 - addr;
                addr = 0x01000000;
            }
#endif

            sys_memory[block].mb_Base = addr;
            sys_memory[block].mb_Size = size;

            range += block_size / 4;
        }

        sys_memory[block_count].mb_Base = 0;
        sys_memory[block_count].mb_Size = 0;
    }
}

/* Trivial virtual to physical translator, fetches data from MMU table and assumes 1M pages */
uintptr_t mmu_virt2phys(uintptr_t virt_addr)
{
    uint32_t page = virt_addr >> 20;
    uint32_t offset = virt_addr & 0x000fffff;

    offset |= mmu_table[page] & 0xfff00000;

    return offset;
}

void mmu_map(uintptr_t phys, uintptr_t virt, uintptr_t length, uint32_t attr_low, uint32_t attr_high)
{
    uintptr_t phys_base = phys & ~0x000fffffu;
    uintptr_t virt_base = virt & ~0x000fffffu;
    uintptr_t end = (virt + length + 0x000fffffu) & ~0x000fffffu;
    (void)attr_high;

    while (virt_base < end)
    {
        mmu_table[virt_base >> 20] = (phys_base & 0xfff00000u) | (attr_low & 0x000fffffu);
        phys_base += 0x00100000u;
        virt_base += 0x00100000u;
    }

    arm_flush_cache((uintptr_t)mmu_table, sizeof(uint32_t) * 4096);
    asm volatile("dsb" ::: "memory");
    asm volatile("mcr p15,0,%0,c8,c7,0" :: "r"(0) : "memory");
    asm volatile("dsb\n\tisb" ::: "memory");
}

void mmu_unmap(uintptr_t virt, uintptr_t length)
{
    (void)virt;
    (void)length;
}
