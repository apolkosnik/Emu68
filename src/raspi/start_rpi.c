/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "EmuFeatures.h"
#include "boards.h"
#include "mmu.h"
#include "disasm.h"

void _start();

#undef ARM_PERIIOBASE
#define ARM_PERIIOBASE (__arm_periiobase)

#if EMU68_HOST_BIG_ENDIAN
static const char bootstrapName[] = "Emu68 runtime/ARM v7-a BigEndian";
#else
static const char bootstrapName[] = "Emu68 runtime/ARM v7-a LittleEndian";
#endif



extern const struct BuildID g_note_build_id;
extern void put_char(uint8_t chr);
extern uint32_t overlay;

void mmap()
{
    kprintf("[BOOT] called mmap");
}

void printf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf(format, v);
    va_end(v);
}




void start_emu(void *);

uint16_t *framebuffer;
uint32_t pitch;
uint32_t fb_width;
uint32_t fb_height;

void display_logo()
{
    struct Size sz = get_display_size();
    uint32_t start_x, start_y;
    uint16_t *buff;
    int32_t pix_cnt = (uint32_t)EmuLogo.el_Width * (uint32_t)EmuLogo.el_Height;
    uint8_t *rle = EmuLogo.el_Data;
    int x = 0;

    kprintf("[BOOT] Display size is %dx%d\n", sz.width, sz.height);
    fb_width = sz.width;
    fb_height = sz.height;
    init_display(sz, (void**)&framebuffer, &pitch);
    kprintf("[BOOT] Framebuffer @ %08x\n", framebuffer);

    start_x = (sz.width - EmuLogo.el_Width) / 2;
    start_y = (sz.height - EmuLogo.el_Height) / 2;

    kprintf("[BOOT] Logo start coordinate: %dx%d, size: %dx%d\n", start_x, start_y, EmuLogo.el_Width, EmuLogo.el_Height);

    /* First clear the screen. Use color in top left corner of RLE image for that */
    {
        uint8_t gray = rle[0];
        uint16_t color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);

        for (int i=0; i < sz.width * sz.height; i++)
            framebuffer[i] = LE16(color);
    }

    /* Now decode RLE and draw it on the screen */
    buff = (uint16_t *)((uintptr_t)framebuffer + pitch*start_y);
    buff += start_x;

    while(pix_cnt > 0) {
        uint8_t gray = *rle++;
        uint8_t cnt = *rle++;
        uint16_t color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        pix_cnt -= cnt;
        while(cnt--) {
            buff[x++] = LE16(color);
            /* If new line, advance the buffer by pitch and reset x counter */
            if (x >= EmuLogo.el_Width) {
                buff += pitch / 2;
                x = 0;
            }
        }
    }
}

uint32_t top_of_ram;
extern int __bootstrap_end;
extern uint32_t mmu_table[4096];
extern void * mmu_table_ptr;
extern void * tmp_stack_ptr;
extern struct M68KState *__m68k_state;

#ifdef PISTORM
extern void *tlsf;

static uint32_t pistorm_rom_initial_sp;
static uint32_t pistorm_rom_initial_pc;
static int pistorm_rom_exec_entry_boot;
static int pistorm_diagrom_boot;
static uint32_t pistorm_page0_saved_desc;
static uint32_t pistorm_boot_fake_lowram_mb;
static uint32_t pistorm_fake_lowram_mb;
static uintptr_t pistorm_fake_lowram_virt;
static uintptr_t pistorm_fake_lowram_phys;
static uintptr_t pistorm_fake_slowram_virt;
static uintptr_t pistorm_fake_slowram_phys;
static uint16_t pistorm_intena_shadow;
static uint16_t pistorm_intreq_shadow;
static uint16_t pistorm_dmacon_shadow;
static uint16_t pistorm_adkcon_shadow;
static uint32_t pistorm_swap_df0_with_dfx;
static uint32_t pistorm_spoof_df0_id;
static uint32_t pistorm_move_slow_to_chip;
static uint32_t pistorm_block_c0;
static uint32_t pistorm_zorro_disable;
static uint32_t pistorm_qemu_model;
static uint32_t pistorm_qemu_rom_bus_hole;
static int32_t pistorm_qemu_rom_bus_hole_override = -1;
static uint32_t pistorm_last_vpos;
static uint32_t pistorm_mmio_trace;
static uint32_t pistorm_step_trace;
static uint32_t pistorm_tracepc_start;
static uint32_t pistorm_tracepc_end;
static uint32_t pistorm_tracepc_last_pc;
static uint32_t pistorm_tracepc_last_a0;
static uint32_t pistorm_tracepc_last_d1;
static uint16_t pistorm_tracepc_last_sr;
static uint32_t pistorm_step_trace_last_pc;
static uint32_t pistorm_step_trace_last_a0_page;
static uint32_t pistorm_step_trace_last_a3_page;
static uint32_t pistorm_step_trace_last_low0;
static uint16_t pistorm_step_trace_last_intena;
static uint16_t pistorm_step_trace_last_intreq;
static uint8_t pistorm_step_trace_last_ipl;
static uint8_t pistorm_mmio_read_seen[0x200];
static uint8_t pistorm_mmio_write_seen[0x200];
static uint8_t pistorm_cia_read_seen[32];
static uint8_t pistorm_cia_write_seen[32];
static uint8_t pistorm_low_read_seen[0x100];
static uint8_t pistorm_low_write_seen[0x100];
static uint8_t pistorm_low_page_read_seen[512];
static uint8_t pistorm_low_page_write_seen[512];
static uint8_t pistorm_chipwin_debug_seen;
static uint8_t pistorm_special_read_wrapper_probe_seen;
static uint8_t pistorm_special_read_wrapper_entry_seen;
static uint8_t pistorm_special_read_callsite_seen;
static uint8_t pistorm_slot_trace_seen;
static uint8_t pistorm_cia_regs[2][16];
static uint8_t pistorm_cia_icr_mask[2];
static uint8_t pistorm_cia_icr_pending[2];
static uint8_t pistorm_pc_trace_seen;
static uint32_t pistorm_recalc_checksum;
static uint32_t pistorm_allow_fast_rom_qemu_boot;
static uint32_t pistorm_allow_fast_diagrom_qemu_boot;
static uint32_t pistorm_diagrom_menu_handoff_ready;
static uint32_t pistorm_diagrom_menu_mode;
static uint32_t pistorm_diagrom_menu_reported;
static uint32_t pistorm_fast_rom_qemu_boot;
static uint32_t pistorm_fast_rom_preexecbase_boot;
static uint32_t pistorm_fast_rom_low0;
static uint32_t pistorm_fast_rom_a6;
static uint32_t pistorm_fast_rom_saved_a1;
static uint32_t pistorm_fast_rom_saved_a3;
static uint32_t pistorm_fast_rom_residents;
static uint32_t pistorm_fast_rom_lowram_node_finalized;
static uint32_t pistorm_saved_jit_control;
static uint32_t pistorm_bootstrap_jit_clamped;

static int pistorm_rom_is_diagrom(const uint8_t *rom_start, uintptr_t image_size)
{
    static const char diagrom_sig[] = "$VER: DiagROM";
    uintptr_t limit = image_size;

    if (limit > 0x20000u)
        limit = 0x20000u;
    if (limit < sizeof(diagrom_sig) - 1u)
        return 0;

    for (uintptr_t off = 0; off + sizeof(diagrom_sig) - 1u <= limit; ++off)
    {
        if (memcmp(rom_start + off, diagrom_sig, sizeof(diagrom_sig) - 1u) == 0)
            return 1;
    }

    return 0;
}

static inline uint16_t pistorm_debug_read_be16_unaligned(const volatile uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline uint32_t pistorm_debug_read_be32_unaligned(const volatile uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

void pistorm_handle_special_write(uint32_t address, uint32_t value, uint32_t size);
uint32_t pistorm_handle_special_read(uint32_t address, uint32_t size);
void pistorm_update_overlay_state(uint32_t new_overlay);
void pistorm_force_overlay_state(uint32_t new_overlay);
uint32_t pistorm_prepare_protocol_write(uint32_t address, uint32_t value, uint32_t size);
uint32_t pistorm_finalize_protocol_read(uint32_t address, uint32_t value, uint32_t size);

static uint32_t pistorm_debug_read_be32_addr(uint32_t addr)
{
    if (addr > 0x00ffffffu)
        return BE32(*(volatile uint32_t *)(uintptr_t)addr);

    return BE32(pistorm_handle_special_read(addr, 4));
}

static void pistorm_debug_dump_memnode_chain(const char *tag, uint32_t node)
{
    kprintf("[PISTORM:MEMLIST] %s first=%08x\n", tag, node);

    for (unsigned i = 0; i < 6; ++i)
    {
        uint32_t succ;
        uint32_t pred;
        uint32_t lower;
        uint32_t upper;
        uint32_t free;

        if (node == 0)
            break;

        succ = pistorm_debug_read_be32_addr(node + 0x00u);
        pred = pistorm_debug_read_be32_addr(node + 0x04u);
        lower = pistorm_debug_read_be32_addr(node + 0x14u);
        upper = pistorm_debug_read_be32_addr(node + 0x18u);
        free = pistorm_debug_read_be32_addr(node + 0x1cu);

        kprintf("[PISTORM:MEMLIST]   node[%u]=%08x succ=%08x pred=%08x lower=%08x upper=%08x free=%08x\n",
                i, node, succ, pred, lower, upper, free);

        if (succ == node)
        {
            kprintf("[PISTORM:MEMLIST]   node[%u] self-loop detected\n", i);
            break;
        }

        node = succ;
    }
}

uint32_t pistorm_get_tracepc_start(void)
{
    return pistorm_tracepc_start;
}

uint32_t pistorm_get_tracepc_end(void)
{
    return pistorm_tracepc_end;
}

uint32_t pistorm_force_tiny_translation_unit(uint32_t pc)
{
    if (!pistorm_fast_rom_qemu_boot)
        return 0;

    if (pc >= 0x00f81e6cu && pc < 0x00f82020u)
        return 1;

    switch (pc)
    {
        case 0x00f8102cu:
        case 0x00f81202u:
        case 0x00f81d60u:
        case 0x00f803dau:
        case 0x00f81108u:
            return 1;
        default:
            return 0;
    }
}

static inline void pistorm_restore_runtime_jit_control(struct M68KState *ctx, uint32_t pc)
{
    if (ctx == NULL || !pistorm_bootstrap_jit_clamped)
        return;

    ctx->JIT_CONTROL = pistorm_saved_jit_control;
    pistorm_bootstrap_jit_clamped = 0;

    if (pistorm_mmio_trace || pistorm_step_trace || pistorm_tracepc_end > pistorm_tracepc_start)
        kprintf("[BOOT] ARM32 PiStorm restored runtime JIT sizing at PC=%08x control=%08x\n",
            pc, ctx->JIT_CONTROL);
}

extern struct ExpansionBoard **board;
extern int board_idx;

typedef struct
{
    uint32_t latch;
    uint32_t current;
    uint64_t last_cntpct;
} pistorm_cia_timer_state_t;

static pistorm_cia_timer_state_t pistorm_cia_timers[2][2];

typedef struct
{
    uint32_t rt_addr;
    uint32_t name_addr;
    const uint8_t *name;
    int8_t priority;
    uint8_t version;
} pistorm_fast_resident_t;

static const uint8_t *pistorm_fast_rom_ptr(const uint8_t *rom_start, uintptr_t image_size, uint32_t address)
{
    if (rom_start == NULL)
        return NULL;

    if (address >= 0x00f80000u && (uintptr_t)(address - 0x00f80000u) < image_size)
        return rom_start + (address - 0x00f80000u);

    if (address >= 0x00f00000u && (uintptr_t)(address - 0x00f00000u) < image_size)
        return rom_start + (address - 0x00f00000u);

    return NULL;
}

static uint16_t pistorm_fast_rom_word(const uint8_t *rom_start, uintptr_t image_size, uint32_t address)
{
    const uint8_t *p = pistorm_fast_rom_ptr(rom_start, image_size, address);

    if (p == NULL)
        return 0;

    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static uint32_t pistorm_fast_rom_long(const uint8_t *rom_start, uintptr_t image_size, uint32_t address)
{
    const uint8_t *p = pistorm_fast_rom_ptr(rom_start, image_size, address);

    if (p == NULL)
        return 0;

    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static int pistorm_fast_rom_cstr_equal(const uint8_t *a, const uint8_t *b)
{
    if (a == NULL || b == NULL)
        return 0;

    while (*a == *b)
    {
        if (*a == 0)
            return 1;
        a++;
        b++;
    }

    return 0;
}

static int pistorm_m68k_cstr_equal(uint32_t a, uint32_t b)
{
    for (;;)
    {
        uint32_t ca = pistorm_handle_special_read(a, 1) & 0xffu;
        uint32_t cb = pistorm_handle_special_read(b, 1) & 0xffu;

        if (ca != cb)
            return 0;
        if (ca == 0)
            return 1;

        a++;
        b++;
    }
}

static uint32_t pistorm_build_resident_list_from_table(uint32_t table_addr, uint32_t *out_count)
{
    pistorm_fast_resident_t entries[128];
    uint32_t entry_count = 0;
    uint32_t cursor = table_addr;

    while (cursor != 0)
    {
        uint32_t d1 = pistorm_handle_special_read(cursor, 4);

        cursor += 4u;
        if (d1 == 0xffffffffu)
            break;

        if (d1 & 1u)
        {
            cursor = d1 & ~1u;
            continue;
        }

        {
            uint32_t addr = d1;
            uint32_t end = pistorm_handle_special_read(cursor, 4);

            cursor += 4u;
            while ((uint32_t)(end - addr) >= 6u)
            {
                uint32_t match_addr;
                uint32_t endskip;
                uint32_t name_addr;
                uint8_t version;
                int8_t priority;
                int replace = -1;

                if (pistorm_handle_special_read(addr, 2) != 0x4afcu)
                {
                    addr += 2u;
                    continue;
                }

                match_addr = addr;
                if ((uint32_t)pistorm_handle_special_read(addr + 2u, 4) != match_addr)
                {
                    addr += 2u;
                    continue;
                }

                endskip = pistorm_handle_special_read(addr + 6u, 4);
                if (endskip <= addr || endskip > end)
                {
                    addr += 2u;
                    continue;
                }

                name_addr = pistorm_handle_special_read(match_addr + 0x0eu, 4);
                if (name_addr == 0)
                {
                    addr = endskip;
                    continue;
                }

                version = (uint8_t)(pistorm_handle_special_read(match_addr + 0x0bu, 1) & 0xffu);
                priority = (int8_t)(pistorm_handle_special_read(match_addr + 0x0du, 1) & 0xffu);

                for (uint32_t j = 0; j < entry_count; j++)
                {
                    if (!pistorm_m68k_cstr_equal(entries[j].name_addr, name_addr))
                        continue;

                    if (version < entries[j].version)
                    {
                        replace = -2;
                        break;
                    }

                    if (version == entries[j].version && priority < entries[j].priority)
                    {
                        replace = -2;
                        break;
                    }

                    replace = (int)j;
                    break;
                }

                if (replace >= 0)
                {
                    entries[replace].rt_addr = match_addr;
                    entries[replace].name_addr = name_addr;
                    entries[replace].priority = priority;
                    entries[replace].version = version;
                }
                else if (replace != -2 && entry_count < sizeof(entries) / sizeof(entries[0]))
                {
                    entries[entry_count].rt_addr = match_addr;
                    entries[entry_count].name_addr = name_addr;
                    entries[entry_count].priority = priority;
                    entries[entry_count].version = version;
                    entry_count++;
                }

                addr = endskip;
            }
        }
    }

    if (entry_count != 0)
    {
        for (uint32_t i = 1; i < entry_count; i++)
        {
            pistorm_fast_resident_t v = entries[i];
            uint32_t j = i;

            while (j > 0 && entries[j - 1].priority < v.priority)
            {
                entries[j] = entries[j - 1];
                j--;
            }
            entries[j] = v;
        }
    }

    if (out_count)
        *out_count = entry_count;

    if (entry_count == 0)
        return 0;

    {
        uint32_t *list = (uint32_t *)tlsf_malloc(tlsf, (entry_count + 1u) * sizeof(uint32_t));
        if (list == NULL)
            return 0;

        for (uint32_t i = 0; i < entry_count; i++)
            list[i] = BE32(entries[i].rt_addr);
        list[entry_count] = 0;
        return (uint32_t)(uintptr_t)list;
    }
}

static uint32_t pistorm_fast_rom_build_residents(const uint8_t *rom_start, uintptr_t image_size, uint32_t *out_count)
{
    static const struct
    {
        uint32_t start;
        uint32_t end;
    } ranges[] =
    {
        {0x00f80000u, 0x00f84088u},
        {0x00f80000u, 0x01000000u},
        {0x00f00000u, 0x00f80000u},
    };

    pistorm_fast_resident_t entries[96];
    uint32_t entry_count = 0;

    for (uint32_t i = 0; i < sizeof(ranges) / sizeof(ranges[0]); i++)
    {
        uint32_t addr = ranges[i].start;
        uint32_t end = ranges[i].end;

        while ((uint32_t)(end - addr) >= 6u)
        {
            uint32_t match_addr;
            uint32_t endskip;
            uint32_t name_addr;
            const uint8_t *name_ptr;
            const uint8_t *rt_ptr;
            uint8_t version;
            int8_t priority;
            int replace = -1;

            if (pistorm_fast_rom_word(rom_start, image_size, addr) != 0x4afcu)
            {
                addr += 2u;
                continue;
            }

            match_addr = addr;
            if (pistorm_fast_rom_long(rom_start, image_size, addr + 2u) != match_addr)
            {
                addr += 2u;
                continue;
            }

            endskip = pistorm_fast_rom_long(rom_start, image_size, addr + 6u);
            if (endskip <= addr || endskip > end)
            {
                addr += 2u;
                continue;
            }

            rt_ptr = pistorm_fast_rom_ptr(rom_start, image_size, match_addr);
            if (rt_ptr == NULL)
            {
                addr += 2u;
                continue;
            }

            name_addr = pistorm_fast_rom_long(rom_start, image_size, match_addr + 0x0eu);
            name_ptr = pistorm_fast_rom_ptr(rom_start, image_size, name_addr);
            if (name_ptr == NULL)
            {
                addr = endskip;
                continue;
            }

            version = rt_ptr[0x0bu];
            priority = (int8_t)rt_ptr[0x0du];

            for (uint32_t j = 0; j < entry_count; j++)
            {
                if (!pistorm_fast_rom_cstr_equal(entries[j].name, name_ptr))
                    continue;

                if (version < entries[j].version)
                {
                    replace = -2;
                    break;
                }

                if (version == entries[j].version && priority < entries[j].priority)
                {
                    replace = -2;
                    break;
                }

                replace = (int)j;
                break;
            }

                if (replace >= 0)
                {
                    entries[replace].rt_addr = match_addr;
                    entries[replace].name_addr = name_addr;
                    entries[replace].name = name_ptr;
                    entries[replace].priority = priority;
                    entries[replace].version = version;
                }
                else if (replace != -2 && entry_count < sizeof(entries) / sizeof(entries[0]))
                {
                    entries[entry_count].rt_addr = match_addr;
                    entries[entry_count].name_addr = name_addr;
                    entries[entry_count].name = name_ptr;
                    entries[entry_count].priority = priority;
                    entries[entry_count].version = version;
                    entry_count++;
                }

            addr = endskip;
        }
    }

    if (entry_count != 0)
    {
        for (uint32_t i = 1; i < entry_count; i++)
        {
            pistorm_fast_resident_t v = entries[i];
            uint32_t j = i;

            while (j > 0 && entries[j - 1].priority < v.priority)
            {
                entries[j] = entries[j - 1];
                j--;
            }
            entries[j] = v;
        }
    }

    if (out_count)
        *out_count = entry_count;

    if (entry_count == 0)
        return 0;

    {
        uint32_t *list = (uint32_t *)tlsf_malloc(tlsf, (entry_count + 1u) * sizeof(uint32_t));
        if (list == NULL)
            return 0;

        for (uint32_t i = 0; i < entry_count; i++)
            list[i] = BE32(entries[i].rt_addr);
        list[entry_count] = 0;
        return (uint32_t)(uintptr_t)list;
    }
}

static int pistorm_parse_hex32(const char *s, uint32_t *value)
{
    uint32_t out = 0;
    int digits = 0;

    if (s == NULL || value == NULL)
        return 0;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    while (digits < 8)
    {
        uint32_t nibble;
        char c = *s;

        if (c >= '0' && c <= '9')
            nibble = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            nibble = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            nibble = (uint32_t)(c - 'A' + 10);
        else
            break;

        out = (out << 4) | nibble;
        s++;
        digits++;
    }

    if (digits == 0)
        return 0;

    *value = out;
    return 1;
}

static inline uint64_t pistorm_read_cntpct(void)
{
    uint32_t lo;
    uint32_t hi;

    asm volatile("mrrc p15, 0, %0, %1, c14" : "=r"(lo), "=r"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint32_t pistorm_read_cntfrq(void)
{
    uint32_t value;

    asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(value));
    return value;
}

static inline uint32_t pistorm_read_special_word(uint16_t value, uint32_t address, uint32_t size)
{
    if (size == 2)
        return value;

    return (address & 1u) ? (value & 0xffu) : (value >> 8);
}

static inline uint16_t pistorm_apply_setclr(uint16_t shadow, uint16_t value, uint16_t mask)
{
    if (value & 0x8000)
        shadow |= value & mask;
    else
        shadow &= ~(value & mask);

    return shadow;
}

static inline void pistorm_store_special_word(uint32_t address, uint16_t value)
{
    *(volatile uint16_t *)(uintptr_t)address = value;
}

static inline uint8_t pistorm_compute_irq_level(uint16_t intena, uint16_t intreq)
{
    uint16_t pending;

    if ((intena & 0x4000u) == 0)
        return 0;

    pending = intreq & intena & 0x3fffu;

    if (pending & 0x2000u)
        return 6;
    if (pending & 0x1c00u)
        return 5;
    if (pending & 0x0780u)
        return 4;
    if (pending & 0x0078u)
        return 3;
    if (pending & 0x0007u)
        return 1;

    return 0;
}

static inline void pistorm_update_irq_state(void)
{
    if (__m68k_state != NULL)
        __m68k_state->INT.IPL = pistorm_compute_irq_level(pistorm_intena_shadow, pistorm_intreq_shadow);
}

static inline uint16_t pistorm_cia_intreq_bit(uint8_t chip)
{
    /* CIAA contributes to PORTS, CIAB contributes to EXTER. */
    return chip ? 0x2000u : 0x0008u;
}

static inline void pistorm_cia_update_irq(uint8_t chip)
{
    uint8_t pending = (uint8_t)(pistorm_cia_icr_pending[chip] & 0x1fu);
    uint8_t active = (uint8_t)(pending & pistorm_cia_icr_mask[chip]);

    if (active)
        pistorm_intreq_shadow |= pistorm_cia_intreq_bit(chip);
    else
        pistorm_intreq_shadow &= (uint16_t)~pistorm_cia_intreq_bit(chip);

    pistorm_store_special_word(0x00dff01e, pistorm_intreq_shadow);
    pistorm_update_irq_state();
}

static inline void pistorm_cia_raise_icr(uint8_t chip, uint8_t bits)
{
    pistorm_cia_icr_pending[chip] |= (uint8_t)(bits & 0x1fu);
    pistorm_cia_update_irq(chip);
}

static inline uint32_t pistorm_cia_ticks_per_tick(void)
{
    /* PAL CIA timers are clocked from the ~709379 Hz E-clock. */
    uint32_t cntfrq = pistorm_read_cntfrq();
    uint32_t ticks = uidiv(cntfrq ? cntfrq : 1, 709379u).q;

    return ticks ? ticks : 1;
}

static inline void pistorm_cia_refresh_timer(uint8_t chip, uint8_t timer)
{
    pistorm_cia_timer_state_t *state = &pistorm_cia_timers[chip][timer];
    uint8_t control = pistorm_cia_regs[chip][14 + timer];
    uint32_t period;
    uint32_t elapsed;
    uint64_t now;
    uint64_t host_ticks;

    if ((control & 1u) == 0)
        return;

    period = state->latch ? state->latch : 0x10000u;
    if (state->current == 0)
        state->current = period;

    now = pistorm_read_cntpct();
    host_ticks = uldiv(now - state->last_cntpct, pistorm_cia_ticks_per_tick()).q;
    if (host_ticks == 0)
        return;

    if (host_ticks > 0xffffffffu)
        elapsed = 0xffffffffu;
    else
        elapsed = (uint32_t)host_ticks;

    state->last_cntpct = now;

    if (elapsed < state->current)
    {
        state->current -= elapsed;
        return;
    }

    if (control & 0x08u)
    {
        state->current = 0;
        pistorm_cia_regs[chip][14 + timer] = control & ~1u;
        pistorm_cia_raise_icr(chip, (uint8_t)(1u << timer));
        return;
    }

    elapsed -= state->current;
    elapsed %= period;
    state->current = period - elapsed;
    if (state->current == 0)
        state->current = period;

    pistorm_cia_raise_icr(chip, (uint8_t)(1u << timer));
}

static inline void pistorm_cia_write_timer_reg(uint8_t chip, uint8_t reg, uint8_t value)
{
    uint8_t timer = (uint8_t)((reg - 4) >> 1);
    pistorm_cia_timer_state_t *state = &pistorm_cia_timers[chip][timer];
    uint8_t control = pistorm_cia_regs[chip][14 + timer];

    if ((reg & 1u) == 0)
        state->latch = (state->latch & 0xff00u) | value;
    else
        state->latch = (state->latch & 0x00ffu) | ((uint32_t)value << 8);

    if ((control & 1u) == 0)
        state->current = state->latch ? state->latch : 0x10000u;
}

static inline void pistorm_cia_write_control_reg(uint8_t chip, uint8_t timer, uint8_t value)
{
    pistorm_cia_timer_state_t *state = &pistorm_cia_timers[chip][timer];
    uint8_t old_control = pistorm_cia_regs[chip][14 + timer];
    uint8_t new_control = value & 0xefu;

    if (new_control & 0x10u)
        state->current = state->latch ? state->latch : 0x10000u;

    if ((new_control & 1u) && ((old_control & 1u) == 0))
        state->last_cntpct = pistorm_read_cntpct();
    else if ((new_control & 1u) == 0)
        pistorm_cia_refresh_timer(chip, timer);

    pistorm_cia_regs[chip][14 + timer] = new_control;
}

static void pistorm_trace_puts(const char *text)
{
    while (*text)
        put_char((uint8_t)*text++);
}

static void pistorm_trace_put_hex32(uint32_t value)
{
    static const char hex[] = "0123456789abcdef";

    for (int shift = 28; shift >= 0; shift -= 4)
        put_char((uint8_t)hex[(value >> shift) & 0xf]);
}

static inline int pistorm_is_custom_address(uint32_t far)
{
    return far >= 0x00dff000 && far < 0x00dff200;
}

static inline int pistorm_decode_custom_target(uint32_t far, uint32_t *target)
{
    if (pistorm_is_custom_address(far))
    {
        *target = far;
        return 1;
    }

    /*
     * Real Amiga hardware mirrors custom registers through the broad
     * CxFxxx..DxFxxx probe window that Kickstart tests during early bring-up.
     * Under QEMU there is no external bus responding there, so normalize those
     * mirrors back onto the primary DFFxxx block.
     */
    if (far >= 0x00c00000u && far < 0x00e00000u && (far & 0x0000f000u) == 0x0000f000u)
    {
        *target = 0x00dff000u | (far & 0x1ffu);
        return 1;
    }

    return 0;
}

static inline int pistorm_is_qemu_rom_bus_hole(uint32_t far)
{
    /*
     * Under QEMU there is no real PiStorm/Amiga expansion bus behind the
     * broad probe windows the wrapped ROMs touch before a real Amiga chipset
     * or Zorro bus would normally answer. Letting those accesses fall through
     * to raw ARM DRAM makes the ROM "discover" memory that should not exist
     * and sends the later bring-up down the wrong path.
     *
     * Keep 0x00b80000-0x00bfffff intact so the current ARM32 slow-RAM remap
     * and CIA space still work, and keep 0x00daxxxx and above untouched so
     * the early board-specific write to 0x00da8000 still behaves like it does
     * on the current ARM32 path.
     */
    return pistorm_qemu_rom_bus_hole &&
        ((far >= 0x00a00000u && far <= 0x00b7ffffu) ||
         (far >= 0x00c00000u && far <= 0x00d9ffffu));
}

static inline int pistorm_is_cia_address(uint32_t far);

static inline int pistorm_decode_qemu_slowram_window(uint32_t far, uint32_t *target)
{
    uint32_t custom_target;

    /*
     * Under QEMU there is no Amiga motherboard or PiStorm bus behind the
     * broad Cx/Dx probe window. ROM exec-entry images use that window
     * during early memory sizing, and collapsing it onto DFFxxx custom
     * mirrors makes the probe conclude there is no usable RAM.
     *
     * Model that window as separate synthetic slow RAM, not as an alias of
     * low memory at address 0. That keeps 0x00c00000 behaving like trapdoor
     * / slow RAM under QEMU instead of collapsing onto the fake low-RAM
     * backing used for 0x00000000-based bootstrap cells.
     */
    if (!pistorm_qemu_model || pistorm_fake_slowram_virt == 0 || target == NULL)
        return 0;

    if (far < 0x00c00000u || far >= 0x00e00000u)
        return 0;

    /*
     * Preserve real custom/CIA mirrors inside the broad Cx/DxFxxx probe
     * window. Wrapped Kickstart uses addresses like C3F09A/C3F01C as real
     * custom-register mirrors during early bring-up, so only the non-mirror
     * portion of that window should fall back to synthetic RAM under QEMU.
     */
    if (pistorm_is_custom_address(far) ||
        pistorm_is_cia_address(far) ||
        pistorm_decode_custom_target(far, &custom_target))
        return 0;

    *target = (uint32_t)(pistorm_fake_slowram_virt + (far - 0x00c00000u));
    return 1;
}

static inline int pistorm_decode_cia_register(uint32_t far, uint8_t *chip, uint8_t *reg)
{
    /*
     * Real Amiga CIA registers are visible through mirrored byte lanes in the
     * broad BFA/BFD/BFE windows. Under the ARM32 local QEMU path there is no
     * external bus model to normalize those mirrors for us, so accept both the
     * odd-addressed canonical form and the even-addressed mirror form here.
     */
    if (far >= 0x00bfa000 && far < 0x00bfb000)
    {
        *chip = 1;
        *reg = (far >> 8) & 0x0fu;
        return 1;
    }

    if (far >= 0x00bfd000 && far < 0x00bfe000)
    {
        *chip = 1;
        *reg = (far >> 8) & 0x0fu;
        return 1;
    }

    if (far >= 0x00bfe000 && far < 0x00bff000)
    {
        *chip = 0;
        *reg = (far >> 8) & 0x0fu;
        return 1;
    }

    return 0;
}

static inline int pistorm_is_cia_address(uint32_t far)
{
    uint8_t chip;
    uint8_t reg;

    return pistorm_decode_cia_register(far, &chip, &reg);
}

static inline struct ExpansionBoard *pistorm_current_board(void)
{
    if (board == NULL)
        return NULL;

    while (board[board_idx] && !board[board_idx]->enabled)
        board_idx++;

    return board[board_idx];
}

static inline int pistorm_handle_zorro_read(uint32_t far, uint32_t size, uint32_t *value)
{
    struct ExpansionBoard *current;

    if (size != 1 || far < 0x00e80000u || far > 0x00e8ffffu)
        return 0;

    if (pistorm_zorro_disable)
    {
        *value = 0xffu;
        return 1;
    }

    current = pistorm_current_board();
    if (current == NULL)
    {
        *value = 0xffu;
        return 1;
    }

    *value = ((const uint8_t *)current->rom_file)[far - 0x00e80000u];
    return 1;
}

static inline int pistorm_handle_zorro_write(uint32_t far, uint32_t value)
{
    struct ExpansionBoard *current;

    if (far < 0x00e80000u || far > 0x00e8ffffu)
        return 0;

    if (pistorm_zorro_disable)
        return 1;

    current = pistorm_current_board();
    if (current == NULL)
        return 0;

    if (current->is_z3)
    {
        if (far == 0x00e80044u)
        {
            current->map_base = (value & 0xffffu) << 16;
            current->map(current);
            board_idx++;
        }
    }
    else if (far == 0x00e80048u)
    {
        current->map_base = (value & 0xffu) << 16;
        current->map(current);
        board_idx++;
    }

    if (far == 0x00e8004cu || far == 0x00e8004eu)
        board_idx++;

    return 1;
}

static inline uint32_t pistorm_raw_read(uint32_t address, uint32_t size)
{
    switch (size)
    {
        case 1:
            return *(volatile uint8_t *)(uintptr_t)address;
        case 2:
            if (address & 1u)
            {
                volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)address;
                return ((uint32_t)p[0] << 8) | (uint32_t)p[1];
            }
            return *(volatile uint16_t *)(uintptr_t)address;
        case 4:
            if (address & 3u)
            {
                volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)address;
                return ((uint32_t)p[0] << 24) |
                    ((uint32_t)p[1] << 16) |
                    ((uint32_t)p[2] << 8) |
                    (uint32_t)p[3];
            }
            return *(volatile uint32_t *)(uintptr_t)address;
        default:
            return 0;
    }
}

static inline void pistorm_raw_write(uint32_t address, uint32_t value, uint32_t size)
{
    switch (size)
    {
        case 1:
            *(volatile uint8_t *)(uintptr_t)address = (uint8_t)value;
            break;
        case 2:
            if (address & 1u)
            {
                volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)address;
                p[0] = (uint8_t)(value >> 8);
                p[1] = (uint8_t)value;
                break;
            }
            *(volatile uint16_t *)(uintptr_t)address = (uint16_t)value;
            break;
        case 4:
            if (address & 3u)
            {
                volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)address;
                p[0] = (uint8_t)(value >> 24);
                p[1] = (uint8_t)(value >> 16);
                p[2] = (uint8_t)(value >> 8);
                p[3] = (uint8_t)value;
                break;
            }
            *(volatile uint32_t *)(uintptr_t)address = value;
            break;
        default:
            break;
    }
}

static inline int pistorm_is_unmapped_high_address(uint32_t far)
{
    return far >= 0x01000000u;
}

static inline uint32_t pistorm_read_unmapped_high_address(uint32_t far, uint32_t size)
{
    (void)far;

    switch (size)
    {
        case 1:
            return 0xbau;
        case 2:
            return 0xbad0u;
        default:
            return 0xbad00badu;
    }
}

static void pistorm_trace_mmio_access(uint32_t address, uint32_t size, uint32_t value, int is_write)
{
    uint32_t far = address & 0x00ffffff;
    uint8_t *seen = NULL;
    const char *kind;
    int trace_cia = 0;
    uint8_t cia_chip = 0;
    uint8_t cia_reg = 0;

    if (!pistorm_mmio_trace)
        return;

    if (pistorm_is_custom_address(far))
    {
        seen = is_write ? pistorm_mmio_write_seen : pistorm_mmio_read_seen;
        if (seen[far - 0x00dff000])
            return;
        seen[far - 0x00dff000] = 1;
        kind = "CUSTOM";
    }
    else if (pistorm_decode_cia_register(far, &cia_chip, &cia_reg))
    {
        uint32_t index = (cia_chip << 4) | (cia_reg & 0x0fu);

        seen = is_write ? pistorm_cia_write_seen : pistorm_cia_read_seen;
        /*
         * During wrapped-ROM bring-up, repeated CIAA PRA writes are the
         * clearest signal that the self-test fell into the failure blink
         * loop. Keep the normal once-only trace behavior for everything else,
         * but allow a few repeated hits on this one register when tracing is
         * explicitly enabled.
         */
        if (far == 0x00bfe001u)
        {
            if (seen[index] >= 8)
                return;
            seen[index]++;
        }
        else
        {
            if (seen[index])
                return;
            seen[index] = 1;
        }
        kind = "CIA";
        trace_cia = 1;
    }
    else if (far < sizeof(pistorm_low_read_seen))
    {
        seen = is_write ? pistorm_low_write_seen : pistorm_low_read_seen;
        /*
         * Low-memory self-tests repeatedly touch the same locations. Preserve
         * the existing low-noise behavior for most addresses, but log the
         * first few hits in the first 256 bytes so the ARM32 real-ROM path
         * can show whether reads actually match the earlier writes.
         */
        if (far < 0x100u)
        {
            if (seen[far] >= 4)
                return;
            seen[far]++;
        }
        else
        {
            if (seen[far])
                return;
            seen[far] = 1;
        }
        kind = "LOW";
    }
    else if (far < 0x00800000u)
    {
        uint32_t page = far >> 14;

        if (page >= 512)
            return;

        seen = is_write ? pistorm_low_page_write_seen : pistorm_low_page_read_seen;
        if (seen[page])
            return;

        seen[page] = 1;
        kind = "LOWPAGE";
    }
    else
    {
        return;
    }

    pistorm_trace_puts("[PISTORM:");
    pistorm_trace_puts(kind);
    pistorm_trace_puts("] ");
    pistorm_trace_puts(is_write ? "write " : "read ");
    put_char((uint8_t)('0' + size));
    pistorm_trace_puts("b @ ");
    pistorm_trace_put_hex32(far);
    pistorm_trace_puts(" = ");
    pistorm_trace_put_hex32(value);
    if (trace_cia)
    {
        pistorm_trace_puts(" chip=");
        pistorm_trace_put_hex32(cia_chip);
        pistorm_trace_puts(" reg=");
        pistorm_trace_put_hex32(cia_reg);
    }
    put_char('\n');
}

static void pistorm_refresh_video_state(void)
{
    const uint32_t pal_lines = 312;
    const uint32_t pal_hpos = 227;
    uint32_t cntfrq = pistorm_read_cntfrq();
    uint64_t cntpct = pistorm_read_cntpct();
    uint32_t frame_ticks;
    uint32_t line_ticks;
    uint32_t within_frame;
    uint32_t vpos;
    uint32_t hpos;
    uint16_t vposr;
    uint16_t vhposr;

    if (cntfrq == 0)
        cntfrq = 1;

    frame_ticks = uidiv(cntfrq, 50).q;
    if (frame_ticks == 0)
        frame_ticks = 1;

    line_ticks = uidiv(frame_ticks, pal_lines).q;
    if (line_ticks == 0)
        line_ticks = 1;

    within_frame = (uint32_t)uldiv(cntpct, frame_ticks).r;
    vpos = uidiv(within_frame, line_ticks).q;
    if (vpos >= pal_lines)
        vpos = pal_lines - 1;

    within_frame = uidiv(within_frame, line_ticks).r;
    hpos = uidiv(within_frame * pal_hpos, line_ticks).q;
    if (hpos >= pal_hpos)
        hpos = pal_hpos - 1;

    if (vpos < pistorm_last_vpos)
        pistorm_intreq_shadow |= 0x0020u;
    pistorm_last_vpos = vpos;
    pistorm_update_irq_state();

    /*
     * These are approximate PAL beam values. They are sufficient for ROM code
     * that polls for progressing video state under QEMU, even though there is
     * no real Amiga chipset behind this ARM32 runtime.
     */
    vposr = (uint16_t)(0x3000u | ((vpos & 0xffu) << 8) | ((vpos >> 8) & 0x7u));
    vhposr = (uint16_t)((hpos & 0xffu) << 8);

    pistorm_store_special_word(0x00dff004, vposr);
    pistorm_store_special_word(0x00dff006, vhposr);
    pistorm_store_special_word(0x00dff01e, pistorm_intreq_shadow);
}

static void swap_rom_bytes(uint8_t *rom_start, uintptr_t size)
{
    for (uintptr_t i = 0; i < size; i += 2)
    {
        uint8_t tmp = rom_start[i];
        rom_start[i] = rom_start[i + 1];
        rom_start[i + 1] = tmp;
    }
}

void pistorm_set_page0_overlay(int enable)
{
    uint32_t new_desc;

    if (pistorm_page0_saved_desc == 0)
        pistorm_page0_saved_desc = mmu_table[0];

    if (enable)
        new_desc = 0x00f00000 | (pistorm_page0_saved_desc & 0x000fffff);
    else
        new_desc = pistorm_page0_saved_desc;

    if (mmu_table[0] == new_desc)
        return;

    mmu_table[0] = new_desc;
    arm_flush_cache((uintptr_t)&mmu_table[0], sizeof(mmu_table[0]));
    asm volatile("dsb; mcr p15,0,%0,c8,c7,0; dsb; isb"::"r"(0));
}

static void pistorm_trace_overlay_state(void)
{
    uint32_t fake_low0 = 0;

    if (!pistorm_mmio_trace)
        return;

    if (pistorm_fake_lowram_virt)
        fake_low0 = BE32(*(volatile uint32_t *)pistorm_fake_lowram_virt);

    kprintf("[PISTORM:OVL] overlay=%u page0=%08x fake_low0=%08x desc=%08x saved=%08x\n",
        overlay,
        BE32(*(volatile uint32_t *)(uintptr_t)0),
        fake_low0,
        mmu_table[0],
        pistorm_page0_saved_desc);
}

static void pistorm_apply_overlay_state(uint32_t new_overlay, int force)
{
    uint32_t old_overlay = overlay;

    new_overlay &= 1u;

    if (!force && old_overlay == new_overlay)
        return;

    overlay = new_overlay;
    pistorm_set_page0_overlay((int)new_overlay);

    if (force || old_overlay != new_overlay)
        pistorm_trace_overlay_state();
}

void pistorm_update_overlay_state(uint32_t new_overlay)
{
    pistorm_apply_overlay_state(new_overlay, 0);
}

void pistorm_force_overlay_state(uint32_t new_overlay)
{
    pistorm_apply_overlay_state(new_overlay, 1);
}

static void pistorm_init_fake_lowram(void)
{
    uintptr_t size_bytes;
    uintptr_t slowram_size_bytes;
    uintptr_t sink_size_bytes;
    uintptr_t total_bytes;
    uintptr_t virt_base;
    uintptr_t phys_base;
    uintptr_t slowram_virt;
    uintptr_t slowram_phys;
    uintptr_t sink_virt;
    uintptr_t sink_phys;
    uint32_t desc_attrs;
    uint32_t sink_descriptor;
    uint32_t aliased_sections;

    if (pistorm_fake_lowram_mb == 0)
        pistorm_fake_lowram_mb = pistorm_boot_fake_lowram_mb;

    if (pistorm_fake_lowram_mb == 0)
        return;

    if (pistorm_fake_lowram_mb > 8)
        pistorm_fake_lowram_mb = 8;

    size_bytes = (uintptr_t)pistorm_fake_lowram_mb << 20;
    slowram_size_bytes = 0x00200000u;
    sink_size_bytes = 0x00100000u;
    total_bytes = size_bytes + slowram_size_bytes + sink_size_bytes;
    virt_base = ((uintptr_t)top_of_ram - 0x02000000u - total_bytes) & ~0x000fffffu;
    phys_base = mmu_virt2phys(virt_base) & ~0x000fffffu;
    slowram_virt = virt_base + size_bytes;
    slowram_phys = phys_base + size_bytes;
    sink_virt = slowram_virt + slowram_size_bytes;
    sink_phys = slowram_phys + slowram_size_bytes;
    desc_attrs = mmu_table[0] & 0x000fffffu;

    memset((void *)virt_base, 0, total_bytes);
    /*
     * Wrapped ROM images look for a "LOWM" marker in low RAM. The current
     * Kickstart wrapper probes both address 0 and 0x00010000, so seed both
     * views in the synthetic low-RAM window.
     */
    *(volatile uint32_t *)virt_base = BE32(0x4c4f574d);
    if (size_bytes > 0x10000u)
        *(volatile uint32_t *)(virt_base + 0x10000u) = BE32(0x4c4f574d);
    arm_flush_cache(virt_base, total_bytes);

    for (uint32_t i = 0; i < pistorm_fake_lowram_mb; i++)
        mmu_table[i] = (uint32_t)(phys_base + ((uintptr_t)i << 20)) | desc_attrs;

    /*
     * Alias every currently-unmapped 1 MB section onto a single "sink"
     * section so that stray JIT stores to m68k addresses outside valid
     * RAM/ROM/IO land in scratch memory instead of taking a data abort.
     *
     * DiagROM's chip-RAM bit-walk test at pc=0xf80252 walks a moving
     * "1" through every power-of-two m68k address up to 0x80000000,
     * including sections far above the identity-mapped QEMU RAM range
     * (sections 0x400..0xff7 for raspi2b). Without the sink, the ARM32
     * store_reg_to_addr paired-halfword STRH faults, and since the
     * ARMv7 abort vector does not set up an abort-mode stack, the
     * handler itself re-aborts and spins forever. The shadow mapping
     * at sections 0xff8..0xfff must stay intact (it mirrors the kernel
     * image that the runtime executes from), so leave those alone.
     */
    sink_descriptor = (uint32_t)sink_phys | desc_attrs;
    aliased_sections = 0;
    /*
     * Alias every currently device-mapped 1 MB section below the SoC
     * peripheral window onto a single scratch "sink" so that stray JIT
     * stores to m68k addresses outside valid RAM/ROM/IO land in a
     * harmless page instead of taking a data abort.
     *
     * DiagROM's chip-RAM bit-walk at pc=0xf80252 walks a moving "1" through
     * every power-of-two m68k address up to 0x80000000, so the JIT emits
     * direct paired-halfword STRH at ARM virt 0x40000000/0x80000000/etc.
     * Those sections default to strongly-ordered device (attr 0x0c06 from
     * the early identity-map fill at start_rpi.c:2930) pointing at unmapped
     * raspi2b bus space, which faults; the ARMv7 abort vector has no abort
     * stack so the handler itself re-aborts and the CPU spins.
     *
     * Sections already retargeted to cached RAM (attr 0x1c0e) by the
     * system-memory identity-map loop in raspi_startup are left alone.
     * The SoC peripheral window at sections 0xf20..start_map-1 (see the
     * /soc ranges mapping below raspi_startup) and the kernel image mirror
     * at 0xff8..0xfff must also stay intact, so the loop stops at 0xf00.
     */
    for (uint32_t i = pistorm_fake_lowram_mb; i < 0xf00u; i++)
    {
        uint32_t desc = mmu_table[i];

        if ((desc & 0x000fffffu) != desc_attrs)
        {
            mmu_table[i] = sink_descriptor;
            aliased_sections++;
        }
    }

    pistorm_fake_lowram_virt = virt_base;
    pistorm_fake_lowram_phys = phys_base;
    pistorm_fake_slowram_virt = slowram_virt;
    pistorm_fake_slowram_phys = slowram_phys;
    pistorm_page0_saved_desc = mmu_table[0];

    arm_flush_cache((uintptr_t)mmu_table, sizeof(mmu_table));
    asm volatile("dsb; mcr p15,0,%0,c8,c7,0; dsb; isb"::"r"(0));

    kprintf("[BOOT] ARM32 PiStorm fake low RAM: %d MB @ virt=%08x phys=%08x\n",
        pistorm_fake_lowram_mb, (uint32_t)virt_base, (uint32_t)phys_base);
    kprintf("[BOOT] ARM32 PiStorm fake slow RAM: %d MB @ virt=%08x phys=%08x\n",
        (uint32_t)(slowram_size_bytes >> 20), (uint32_t)slowram_virt, (uint32_t)slowram_phys);
    kprintf("[BOOT] ARM32 PiStorm stray-store sink: 1 MB @ virt=%08x phys=%08x (%u sections aliased)\n",
        (uint32_t)sink_virt, (uint32_t)sink_phys, aliased_sections);
    kprintf("[BOOT] ARM32 PiStorm fake low RAM seed low0=%08x\n",
        BE32(*(volatile uint32_t *)virt_base));
}

void pistorm_handle_overlay_byte_store(uint32_t address, uint32_t value)
{
    if ((address & 0x00ffffff) == 0x00bfe001)
        pistorm_update_overlay_state(value);
}

static uint8_t pistorm_swap_ciabprb_read(uint8_t value)
{
    if (pistorm_swap_df0_with_dfx)
    {
        const uint32_t sel0_bit = 3;
        uint32_t swap_bit = sel0_bit + pistorm_swap_df0_with_dfx;

        if (((value >> sel0_bit) & 1u) != ((value >> swap_bit) & 1u))
            value ^= (uint8_t)((1u << sel0_bit) | (1u << swap_bit));
    }

    return value;
}

static uint8_t pistorm_swap_ciabprb_write(uint8_t value)
{
    if (pistorm_swap_df0_with_dfx)
    {
        const uint32_t sel0_bit = 3;
        uint32_t swap_bit = sel0_bit + pistorm_swap_df0_with_dfx;

        if ((value & ((1u << swap_bit) | 0x80u)) == 0x80u)
            pistorm_spoof_df0_id = 1;
        else
            pistorm_spoof_df0_id = 0;

        if (((value >> sel0_bit) & 1u) != ((value >> swap_bit) & 1u))
            value ^= (uint8_t)((1u << sel0_bit) | (1u << swap_bit));
    }

    return value;
}

static int pistorm_sync_special_write_state(uint32_t far, uint32_t *value_io, uint32_t size)
{
    uint8_t cia_chip;
    uint8_t cia_reg;

    if (size == 1 && pistorm_decode_cia_register(far, &cia_chip, &cia_reg))
    {
        uint8_t cia_value = (uint8_t)(*value_io);

        if (cia_chip == 0 && cia_reg == 0)
            pistorm_update_overlay_state(cia_value);
        else if (cia_chip == 1 && cia_reg == 1)
            cia_value = pistorm_swap_ciabprb_write(cia_value);

        if (cia_reg >= 4 && cia_reg <= 7)
        {
            pistorm_cia_write_timer_reg(cia_chip, cia_reg, cia_value);
            pistorm_cia_regs[cia_chip][cia_reg] = cia_value;
        }
        else if (cia_reg == 13)
        {
            if (cia_value & 0x80u)
                pistorm_cia_icr_mask[cia_chip] |= (uint8_t)(cia_value & 0x1fu);
            else
                pistorm_cia_icr_mask[cia_chip] &= (uint8_t)~(cia_value & 0x1fu);
            pistorm_cia_update_irq(cia_chip);
        }
        else if (cia_reg == 14 || cia_reg == 15)
        {
            pistorm_cia_write_control_reg(cia_chip, (uint8_t)(cia_reg - 14), cia_value);
        }
        else
        {
            pistorm_cia_regs[cia_chip][cia_reg] = cia_value;
        }

        *value_io = (*value_io & ~0xffu) | cia_value;
        return 1;
    }

    if (size == 2 && far == 0x00dff09a)
    {
        pistorm_intena_shadow = pistorm_apply_setclr(pistorm_intena_shadow, (uint16_t)(*value_io), 0x7fffu);
        pistorm_store_special_word(0x00dff01c, pistorm_intena_shadow);
        pistorm_update_irq_state();
        return 1;
    }

    if (size == 2 && far == 0x00dff09c)
    {
        pistorm_intreq_shadow = pistorm_apply_setclr(pistorm_intreq_shadow, (uint16_t)(*value_io), 0x3fffu);
        pistorm_store_special_word(0x00dff01e, pistorm_intreq_shadow);
        pistorm_update_irq_state();
        return 1;
    }

    if (size == 2 && far == 0x00dff096)
    {
        pistorm_dmacon_shadow = pistorm_apply_setclr(pistorm_dmacon_shadow, (uint16_t)(*value_io), 0x7fffu);
        pistorm_store_special_word(0x00dff002, pistorm_dmacon_shadow);
        return 1;
    }

    if (size == 2 && far == 0x00dff09e)
    {
        pistorm_adkcon_shadow = pistorm_apply_setclr(pistorm_adkcon_shadow, (uint16_t)(*value_io), 0x7fffu);
        pistorm_store_special_word(0x00dff010, pistorm_adkcon_shadow);
        return 1;
    }

    return 0;
}

uint32_t pistorm_prepare_protocol_write(uint32_t address, uint32_t value, uint32_t size)
{
    uint32_t far = address;
    uint32_t custom_target = 0;

    if (pistorm_is_unmapped_high_address(far))
        return value;

    if (pistorm_decode_custom_target(far, &custom_target))
        far = custom_target;

    pistorm_sync_special_write_state(far, &value, size);
    return value;
}

static uint32_t pistorm_normalize_special_read_value(uint32_t far, uint32_t value, uint32_t size)
{
    uint8_t cia_chip;
    uint8_t cia_reg;

    if (size == 1 && pistorm_decode_cia_register(far, &cia_chip, &cia_reg))
    {
        uint8_t cia_value = (uint8_t)value;

        if (cia_chip == 0 && cia_reg == 0)
        {
            cia_value |= 0xc0u;
            cia_value = (cia_value & ~1u) | (overlay & 1u);

            if (pistorm_swap_df0_with_dfx && pistorm_spoof_df0_id)
                cia_value &= 0xdfu;
        }
        else if (cia_chip == 1 && cia_reg == 1)
        {
            cia_value = pistorm_swap_ciabprb_read(cia_value);
        }

        return cia_value;
    }

    if ((far & ~1u) == 0x00dff002)
        return pistorm_read_special_word(pistorm_dmacon_shadow, far, size);

    if ((far & ~1u) == 0x00dff010)
        return pistorm_read_special_word(pistorm_adkcon_shadow, far, size);

    if ((far & ~1u) == 0x00dff01c)
        return pistorm_read_special_word(pistorm_intena_shadow, far, size);

    if ((far & ~1u) == 0x00dff01e)
        return pistorm_read_special_word(pistorm_intreq_shadow, far, size);

    return value;
}

uint32_t pistorm_finalize_protocol_read(uint32_t address, uint32_t value, uint32_t size)
{
    uint32_t far = address;
    uint32_t custom_target = 0;
    uint32_t slowram_target = 0;

    if (pistorm_is_unmapped_high_address(far))
        return pistorm_read_unmapped_high_address(far, size);

    if (pistorm_qemu_model)
    {
        if ((overlay && far < 0x00080000u) ||
            (pistorm_fake_lowram_virt != 0 &&
             far < ((uint32_t)pistorm_fake_lowram_mb << 20)) ||
            pistorm_decode_qemu_slowram_window(far, &slowram_target) ||
            pistorm_is_qemu_rom_bus_hole(far) ||
            pistorm_is_custom_address(far) ||
            pistorm_is_cia_address(far) ||
            (far >= 0x00e80000u && far <= 0x00e8ffffu))
        {
            return pistorm_handle_special_read(address, size);
        }
    }

    if (pistorm_decode_custom_target(far, &custom_target))
        far = custom_target;

    return pistorm_normalize_special_read_value(far, value, size);
}

uint32_t pistorm_handle_special_read(uint32_t address, uint32_t size)
{
    uint32_t far = address;
    uint32_t custom_target = 0;
    uint32_t slowram_target = 0;
    uint32_t value;
    uint8_t cia_chip;
    uint8_t cia_reg;
    extern uint32_t overlay;
    extern uint32_t last_PC;

    if (pistorm_is_unmapped_high_address(far))
        return pistorm_read_unmapped_high_address(far, size);

    /*
     * Match the AArch64 PiStorm fast-overlay behavior for low-memory reads:
     * while overlay is active, reads from the first 512K still come from the
     * ROM shadow, even though ARM32 stores may target the underlying RAM view.
     */
    if (overlay && far < 0x00080000)
    {
        value = pistorm_raw_read(0x00f00000u + far, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if (pistorm_move_slow_to_chip &&
        far >= 0x00080000u && far <= 0x000fffffu &&
        pistorm_decode_qemu_slowram_window(far + 0x00b80000u, &slowram_target))
    {
        value = pistorm_raw_read(slowram_target, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if (pistorm_fake_lowram_virt != 0 &&
        far < ((uint32_t)pistorm_fake_lowram_mb << 20))
    {
        volatile uint8_t *base = (volatile uint8_t *)(uintptr_t)(pistorm_fake_lowram_virt + far);

        switch (size)
        {
            case 1:
                value = *base;
                break;
            case 2:
                value = BE16(*(volatile uint16_t *)base);
                break;
            default:
                value = BE32(*(volatile uint32_t *)base);
                break;
        }

        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if (pistorm_decode_qemu_slowram_window(far, &slowram_target))
    {
        value = pistorm_raw_read(slowram_target, size);
        if (pistorm_mmio_trace && far == 0x00c3f01cu && size == 2 && pistorm_chipwin_debug_seen < 8)
        {
            pistorm_chipwin_debug_seen++;
            kprintf("[PISTORM:SLOWWIN] last=%08x read @ %08x -> %08x target=%08x\n",
                last_PC,
                far,
                value,
                slowram_target);
        }
        return value;
    }

    if (pistorm_decode_custom_target(far, &custom_target))
        far = custom_target;

    if (pistorm_block_c0 && far >= 0x00c00000u && far <= 0x00c7ffffu)
        return 0;

    if (pistorm_move_slow_to_chip)
    {
        if (far >= 0x00080000u && far <= 0x000fffffu)
            far += 0x00b80000u;
        else if (far >= 0x00c00000u && far <= 0x00c7ffffu)
            return 0;
    }

    if (pistorm_diagrom_boot &&
        pistorm_allow_fast_diagrom_qemu_boot &&
        far >= 0x00200000u && far < 0x00a00000u)
    {
        if (size == 1)
            return 0xffu;
        if (size == 2)
            return 0xffffu;
        return 0xffffffffu;
    }

    if (pistorm_is_qemu_rom_bus_hole(far))
        return 0;

    if ((far & ~1u) == 0x00dff004)
    {
        pistorm_refresh_video_state();
        value = pistorm_read_special_word(*(volatile uint16_t *)(uintptr_t)0x00dff004, far, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if ((far & ~1u) == 0x00dff006)
    {
        pistorm_refresh_video_state();
        value = pistorm_read_special_word(*(volatile uint16_t *)(uintptr_t)0x00dff006, far, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if ((far & ~1u) == 0x00dff016)
    {
        /*
         * With no real Amiga joystick/mouse hardware behind QEMU, keep the
         * button sense lines idle-high so ROM bring-up does not think every
         * mouse button is held down at reset. DiagROM uses bits 8/10/12/14.
         */
        uint16_t potgor = *(volatile uint16_t *)(uintptr_t)0x00dff016;

        potgor |= 0x5500u;
        value = pistorm_read_special_word(potgor, far, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if (pistorm_handle_zorro_read(far, size, &value))
    {
        if (pistorm_mmio_trace)
            kprintf("[PISTORM:ZORRO] read 1b @ %08x = %02x board_idx=%d\n", far, value & 0xffu, board_idx);
        return value;
    }

    if (size == 1 && pistorm_decode_cia_register(far, &cia_chip, &cia_reg))
    {
        if (cia_reg >= 4 && cia_reg <= 7)
        {
            uint8_t timer = (uint8_t)((cia_reg - 4) >> 1);
            pistorm_cia_refresh_timer(cia_chip, timer);
            value = (cia_reg & 1u)
                ? (pistorm_cia_timers[cia_chip][timer].current >> 8)
                : (pistorm_cia_timers[cia_chip][timer].current & 0xffu);
        }
        else if (cia_reg == 13)
        {
            value = (uint8_t)(pistorm_cia_icr_pending[cia_chip] & 0x1fu);
            if (value & pistorm_cia_icr_mask[cia_chip])
                value |= 0x80u;
            pistorm_cia_icr_pending[cia_chip] = 0;
            pistorm_cia_update_irq(cia_chip);
        }
        else
        {
            value = pistorm_cia_regs[cia_chip][cia_reg];
        }

        if (cia_chip == 0 && cia_reg == 0)
        {
            /*
             * CIAA PRA button / drive-ID inputs are pulled high when idle on
             * a real machine. QEMU does not drive them for the ARM32 PiStorm
             * path, so preserve that idle-high default here.
             */
            value = pistorm_normalize_special_read_value(far, value, size);
        }

        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if ((far & ~1u) == 0x00dff002)
    {
        value = pistorm_normalize_special_read_value(far, 0, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if ((far & ~1u) == 0x00dff010)
    {
        value = pistorm_normalize_special_read_value(far, 0, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if ((far & ~1u) == 0x00dff01c)
    {
        value = pistorm_normalize_special_read_value(far, 0, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if ((far & ~1u) == 0x00dff01e)
    {
        pistorm_refresh_video_state();
        value = pistorm_normalize_special_read_value(far, 0, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if ((far & ~1u) == 0x00dff018)
    {
        /*
         * Minimal synthetic Amiga serial status for ARM32 QEMU ROM boots.
         * DiagROM uses both of the usual "ready" bits here:
         *  - bit 13 in its normal transmit path
         *  - bit 14 in an alternate startup wait loop
         * If either never becomes ready, bring-up spends most of its time
         * in timeout polling instead of progressing through the ROM.
         */
        value = pistorm_read_special_word(0x6000u, far, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    if (pistorm_is_custom_address(far))
    {
        value = pistorm_raw_read(far, size);
        pistorm_trace_mmio_access(far, size, value, 0);
        return value;
    }

    value = pistorm_raw_read(far, size);
    if (far < sizeof(pistorm_low_read_seen))
        pistorm_trace_mmio_access(far, size, value, 0);
    return value;
}

void pistorm_handle_special_write(uint32_t address, uint32_t value, uint32_t size)
{
    uint32_t far = address;
    uint32_t custom_target = 0;
    uint32_t slowram_target = 0;

    if (pistorm_is_unmapped_high_address(far))
        return;

    if (pistorm_move_slow_to_chip &&
        far >= 0x00080000u && far <= 0x000fffffu &&
        pistorm_decode_qemu_slowram_window(far + 0x00b80000u, &slowram_target))
    {
        pistorm_raw_write(slowram_target, value, size);
        return;
    }

    if (pistorm_fake_lowram_virt != 0 &&
        far < ((uint32_t)pistorm_fake_lowram_mb << 20))
    {
        volatile uint8_t *base = (volatile uint8_t *)(uintptr_t)(pistorm_fake_lowram_virt + far);

        switch (size)
        {
            case 1:
                *base = (uint8_t)value;
                break;
            case 2:
                *(volatile uint16_t *)base = BE16((uint16_t)value);
                break;
            default:
                *(volatile uint32_t *)base = BE32(value);
                break;
        }

        arm_flush_cache((uintptr_t)base, size);
        pistorm_trace_mmio_access(far, size, value, 1);
        return;
    }

    if (pistorm_decode_qemu_slowram_window(far, &slowram_target))
    {
        pistorm_raw_write(slowram_target, value, size);
        return;
    }

    if (pistorm_decode_custom_target(far, &custom_target))
        far = custom_target;

    if (pistorm_block_c0 && far >= 0x00c00000u && far <= 0x00c7ffffu)
        return;

    if (pistorm_move_slow_to_chip)
    {
        if (far >= 0x00080000u && far <= 0x000fffffu)
            far += 0x00b80000u;
        else if (far >= 0x00c00000u && far <= 0x00c7ffffu)
            return;
    }

    if (pistorm_diagrom_boot &&
        pistorm_allow_fast_diagrom_qemu_boot &&
        far >= 0x00200000u && far < 0x00a00000u)
        return;

    if (pistorm_is_qemu_rom_bus_hole(far))
        return;

    if (pistorm_sync_special_write_state(far, &value, size))
    {
        pistorm_trace_mmio_access(far, size, value, 1);
        return;
    }

    if (pistorm_handle_zorro_write(far, value))
    {
        if (pistorm_mmio_trace)
            kprintf("[PISTORM:ZORRO] write 1b @ %08x = %02x board_idx=%d\n", far, value & 0xffu, board_idx);
        return;
    }

    if (size == 2 && far == 0x00dff030)
    {
        /*
         * Minimal synthetic serial TX path for ARM32 QEMU DiagROM bring-up.
         * Emit the low byte through the existing boot console so DiagROM can
         * make forward progress without waiting on missing UART hardware.
         */
        if (pistorm_diagrom_boot)
            kprintf("%c", value & 0xffu);
        pistorm_trace_mmio_access(far, size, value, 1);
        return;
    }

    if (pistorm_is_custom_address(far))
    {
        pistorm_raw_write(far, value, size);
        pistorm_trace_mmio_access(far, size, value, 1);
        return;
    }

    if (far < sizeof(pistorm_low_write_seen))
        pistorm_trace_mmio_access(far, size, value, 1);
    pistorm_raw_write(far, value, size);
}

static void pistorm_fast_rom_seed_slowram_node(void)
{
    volatile uint8_t *slowram;
    uint32_t a0 = 0x00000000u;
    uint32_t a1 = 0x00f80414u;
    uint32_t d0 = 0x00200000u - a0;
    uint32_t aligned_a1;
    uint32_t high;

    if (pistorm_fake_slowram_virt == 0)
        return;

    slowram = (volatile uint8_t *)(uintptr_t)pistorm_fake_slowram_virt;

    /*
     * Seed the first synthetic slow-RAM MemHeader at 0x00c00000. This is
     * separate from the fake low-RAM backing used for address-0 bootstrap
     * cells, so ARM32 QEMU no longer aliases slow RAM onto low memory.
     */
    *(volatile uint8_t  *)(slowram + a0 + 0x08u) = 0x0au;
    *(volatile uint8_t  *)(slowram + a0 + 0x09u) = 0xf6u;
    *(volatile uint32_t *)(slowram + a0 + 0x0au) = BE32(a1);
    *(volatile uint16_t *)(slowram + a0 + 0x0eu) = BE16(0x0303u);

    aligned_a1 = (a0 + 0x20u + 7u) & ~7u;
    d0 = ((d0 + ((a0 + 0x20u) - aligned_a1)) & ~7u) - 0x20u;
    high = aligned_a1 + d0;

    *(volatile uint32_t *)(slowram + a0 + 0x10u) = BE32(aligned_a1);
    *(volatile uint32_t *)(slowram + a0 + 0x14u) = BE32(aligned_a1);
    *(volatile uint32_t *)(slowram + a0 + 0x18u) = BE32(high);
    *(volatile uint32_t *)(slowram + a0 + 0x1cu) = BE32(d0);
    *(volatile uint32_t *)(slowram + aligned_a1 + 0x00u) = 0;
    *(volatile uint32_t *)(slowram + aligned_a1 + 0x04u) = BE32(d0);
}

static void pistorm_fast_rom_finalize_slowram_node(void)
{
    if (pistorm_fast_rom_lowram_node_finalized || pistorm_fake_slowram_virt == 0)
        return;
    pistorm_fast_rom_lowram_node_finalized = 1;
}

static inline uint8_t pistorm_fast_rom_read8(uint32_t addr)
{
    if (addr > 0x00ffffffu)
        return *(volatile uint8_t *)(uintptr_t)addr;

    return (uint8_t)(pistorm_handle_special_read(addr, 1) & 0xffu);
}

static inline uint16_t pistorm_fast_rom_read16(uint32_t addr)
{
    if (addr > 0x00ffffffu)
        return BE16(*(volatile uint16_t *)(uintptr_t)addr);

    return BE16((uint16_t)pistorm_handle_special_read(addr, 2));
}

static inline uint32_t pistorm_fast_rom_read32(uint32_t addr)
{
    if (addr > 0x00ffffffu)
        return BE32(*(volatile uint32_t *)(uintptr_t)addr);

    return BE32(pistorm_handle_special_read(addr, 4));
}

static inline void pistorm_fast_rom_write8(uint32_t addr, uint8_t value)
{
    if (addr > 0x00ffffffu)
    {
        *(volatile uint8_t *)(uintptr_t)addr = value;
        return;
    }

    pistorm_handle_special_write(addr, value, 1);
}

static inline void pistorm_fast_rom_write16(uint32_t addr, uint16_t value)
{
    if (addr > 0x00ffffffu)
    {
        *(volatile uint16_t *)(uintptr_t)addr = BE16(value);
        return;
    }

    pistorm_handle_special_write(addr, value, 2);
}

static inline void pistorm_fast_rom_write32(uint32_t addr, uint32_t value)
{
    if (addr > 0x00ffffffu)
    {
        *(volatile uint32_t *)(uintptr_t)addr = BE32(value);
        return;
    }

    pistorm_handle_special_write(addr, value, 4);
}

static int pistorm_fast_rom_expand_stream(struct M68KState *ctx)
{
    uint32_t a0 = BE32(ctx->A[0].u32);
    uint32_t a1 = BE32(ctx->A[1].u32);
    uint32_t a2 = BE32(ctx->A[2].u32);
    uint32_t a7 = BE32(ctx->A[7].u32);
    uint32_t d0 = BE32(ctx->D[0].u32);
    uint32_t d1 = BE32(ctx->D[1].u32);
    uint32_t ret;
    uint32_t clear_words;

    if (a2 == 0 || a7 == 0)
        return 0;

    /*
     * Kickstart helper at 0x00f81202:
     * 1. clear the freshly allocated target region as words
     * 2. walk a small bytecode stream at A1
     * 3. expand byte/word/long runs into the destination rooted at A2
     * 4. RTS
     */
    a0 = a2;
    clear_words = ((uint32_t)(d0 & 0xffffu) >> 1) + 1u;
    while (clear_words-- != 0)
    {
        pistorm_fast_rom_write16(a0, 0);
        a0 += 2u;
    }

    a0 = a2;

    for (;;)
    {
        uint32_t op;
        uint32_t mode;
        uint32_t count;

        d0 &= 0xffff0000u;
        op = pistorm_fast_rom_read8(a1);
        a1 += 1u;
        d0 |= op;

        if (op == 0)
            break;

        if ((int8_t)op < 0)
        {
            uint32_t off;

            if (op & 0x40u)
            {
                off = pistorm_fast_rom_read32(a1 - 1u) & 0x00ffffffu;
                a1 += 3u;
            }
            else
            {
                off = pistorm_fast_rom_read8(a1);
                a1 += 1u;
            }

            a0 = a2 + off;
        }

        mode = (op >> 3) & 0x0eu;
        count = op & 0x0fu;

        switch (mode)
        {
            case 0x00:
            case 0x08:
            {
                uint32_t iters = count + 1u;

                a1 = (a1 + 1u) & ~1u;
                while (iters-- != 0)
                {
                    uint32_t v = pistorm_fast_rom_read32(a1);
                    pistorm_fast_rom_write32(a0, v);
                    a0 += 4u;
                    a1 += 4u;
                }
                break;
            }

            case 0x02:
            case 0x0a:
            {
                uint32_t iters = count + 1u;

                a1 = (a1 + 1u) & ~1u;
                while (iters-- != 0)
                {
                    uint16_t v = pistorm_fast_rom_read16(a1);
                    pistorm_fast_rom_write16(a0, v);
                    a0 += 2u;
                    a1 += 2u;
                }
                break;
            }

            case 0x04:
            case 0x0c:
            {
                uint32_t iters = count + 1u;

                while (iters-- != 0)
                {
                    uint8_t v = pistorm_fast_rom_read8(a1);
                    pistorm_fast_rom_write8(a0, v);
                    a0 += 1u;
                    a1 += 1u;
                }
                break;
            }

            case 0x06:
            case 0x0e:
                goto done;

            default:
                return 0;
        }
    }

done:
    ret = pistorm_fast_rom_read32(a7);
    ctx->A[0].u32 = BE32(a0);
    ctx->A[1].u32 = BE32(a1);
    ctx->D[0].u32 = BE32(d0);
    ctx->D[1].u32 = BE32(d1);
    ctx->A[7].u32 = BE32(a7 + 4u);
    ctx->PC = BE32(ret);

    if (pistorm_mmio_trace || pistorm_step_trace || pistorm_tracepc_end > pistorm_tracepc_start)
    {
        kprintf("[BOOT] ARM32 PiStorm fast ROM expander returned pc=%08x a0=%08x a1=%08x a2=%08x d0=%08x\n",
            ret,
            a0,
            a1,
            a2,
            d0);
    }

    return 1;
}

static void pistorm_fast_rom_alloc_bookkeep(uint32_t a6)
{
    uint8_t v127 = pistorm_fast_rom_read8(a6 + 0x127u);

    v127 = (uint8_t)(v127 - 1u);
    pistorm_fast_rom_write8(a6 + 0x127u, v127);
    if ((int8_t)v127 >= 0)
        return;

    if ((int8_t)pistorm_fast_rom_read8(a6 + 0x126u) >= 0)
        return;

    if ((int16_t)pistorm_fast_rom_read16(a6 + 0x124u) >= 0)
        return;
}

static uint32_t pistorm_fast_rom_alloc_from_header(uint32_t header, uint32_t size)
{
    uint32_t entry = header + 0x10u;
    uint32_t alloc_size = (size + 7u) & ~7u;

    for (;;)
    {
        uint32_t chunk = pistorm_fast_rom_read32(entry);
        uint32_t chunk_size;

        if (chunk == 0)
            return 0;

        chunk_size = pistorm_fast_rom_read32(chunk + 4u);
        if (alloc_size <= chunk_size)
        {
            uint32_t remain = chunk_size - alloc_size;

            if (remain == 0)
            {
                pistorm_fast_rom_write32(entry, pistorm_fast_rom_read32(chunk));
            }
            else
            {
                uint32_t split = chunk + alloc_size;

                pistorm_fast_rom_write32(entry, split);
                pistorm_fast_rom_write32(split + 0u, pistorm_fast_rom_read32(chunk + 0u));
                pistorm_fast_rom_write32(split + 4u, remain);
            }

            pistorm_fast_rom_write32(header + 0x1cu, pistorm_fast_rom_read32(header + 0x1cu) - alloc_size);
            return chunk;
        }

        entry = chunk;
    }
}

static int pistorm_fast_rom_finish_allocclear(struct M68KState *m68k)
{
    uint32_t a0 = BE32(m68k->A[0].u32);
    uint32_t a6 = BE32(m68k->A[6].u32);
    uint32_t a7 = BE32(m68k->A[7].u32);
    uint32_t d1 = BE32(m68k->D[1].u32);
    uint32_t d2 = BE32(m68k->D[2].u32);
    uint32_t d3 = BE32(m68k->D[3].u32);
    uint32_t saved_d2;
    uint32_t saved_d3;
    uint32_t ret;
    uint32_t result = 0;
    uint16_t sr;

    if (a0 == 0 || a6 == 0)
        return 0;

    pistorm_fast_rom_finalize_slowram_node();

    while (a0 != 0)
    {
        uint32_t flags;
        uint32_t free_bytes;

        flags = (uint32_t)pistorm_fast_rom_read16(a0 + 0x0eu) & (d2 & 0xffffu);
        free_bytes = pistorm_fast_rom_read32(a0 + 0x1cu);

        if (flags == (d2 & 0xffffu) && d3 <= free_bytes)
        {
            result = pistorm_fast_rom_alloc_from_header(a0, d3);
            break;
        }

        a0 = pistorm_fast_rom_read32(a0);
    }

    pistorm_fast_rom_alloc_bookkeep(a6);

    if (result != 0 && (d2 & (1u << 16)) != 0)
    {
        uint32_t clear_addr = result;
        uint32_t clear_size = (d3 + 7u) & ~7u;

        d1 = 0;
        while (clear_size != 0)
        {
            pistorm_fast_rom_write32(clear_addr, 0);
            clear_addr += 4u;
            clear_size -= 4u;
        }

        m68k->A[0].u32 = BE32(clear_addr);
    }

    saved_d2 = pistorm_fast_rom_read32(a7 + 0u);
    saved_d3 = pistorm_fast_rom_read32(a7 + 4u);
    ret = pistorm_fast_rom_read32(a7 + 8u);
    kprintf("[PISTORM:FINISHALLOC] a7=%08x s0=%08x s4=%08x s8=%08x result=%08x\n",
        a7, saved_d2, saved_d3, ret, result);

    m68k->D[0].u32 = BE32(result);
    m68k->D[1].u32 = BE32(d1);
    m68k->D[2].u32 = BE32(saved_d2);
    m68k->D[3].u32 = BE32(saved_d3);
    sr = BE16(m68k->SR);
    sr &= (uint16_t)~(SR_N | SR_Z | SR_V | SR_C);
    if (result == 0)
        sr |= SR_Z;
    else if (result & 0x80000000u)
        sr |= SR_N;
    m68k->SR = BE16(sr);
    m68k->A[7].u32 = BE32(a7 + 12u);
    m68k->PC = BE32(ret);
    return 1;
}

static int pistorm_try_fast_rom_exec_kick_boot(const uint8_t *rom_start, uintptr_t image_size)
{
    uint32_t lowmem_resident;
    volatile uint32_t *lowmem;

    pistorm_fast_rom_qemu_boot = 0;
    pistorm_fast_rom_preexecbase_boot = 0;
    pistorm_fast_rom_low0 = 0;
    pistorm_fast_rom_a6 = 0;
    pistorm_fast_rom_saved_a1 = 0;
    pistorm_fast_rom_saved_a3 = 0;
    pistorm_fast_rom_residents = 0;
    pistorm_fast_rom_lowram_node_finalized = 0;
    pistorm_diagrom_menu_handoff_ready = 0;
    pistorm_diagrom_menu_mode = 0;

    if (!pistorm_allow_fast_rom_qemu_boot ||
        !pistorm_qemu_model || image_size < 0x250 ||
        pistorm_fake_lowram_mb == 0 || pistorm_fake_lowram_virt == 0)
        return 0;

    /*
     * This matches the ROM exec-entry Kickstart 3.2 bootstrap that:
     * 1. checks the ROM checksum
     * 2. fills low exception vectors with 0x00f80492
     * 3. verifies that fill
     * 4. continues at 0x00f8024e
     *
     * Under QEMU that bootstrap burns a lot of time before reaching the real
     * ROM body, so pre-seed the same state and jump directly to the wrapped
     * ROM continuation.
     */
    if (!(rom_start[0x152] == 0x4f && rom_start[0x153] == 0xf8 &&
          rom_start[0x154] == 0x04 && rom_start[0x155] == 0x00 &&
          rom_start[0x156] == 0x41 && rom_start[0x157] == 0xf9 &&
          rom_start[0x15c] == 0x72 && rom_start[0x15d] == 0xff &&
          rom_start[0x23e] == 0x30 && rom_start[0x23f] == 0x3c &&
          rom_start[0x240] == 0x00 && rom_start[0x241] == 0xf0 &&
          rom_start[0x242] == 0x72 && rom_start[0x243] == 0x2d &&
          rom_start[0x244] == 0xb3 && rom_start[0x245] == 0xe0 &&
          rom_start[0x24e] == 0x74 && rom_start[0x24f] == 0x00))
    {
        return 0;
    }

    lowmem_resident =
        ((uint32_t)rom_start[0x48] << 24) |
        ((uint32_t)rom_start[0x49] << 16) |
        ((uint32_t)rom_start[0x4a] << 8) |
        (uint32_t)rom_start[0x4b];

    pistorm_fast_rom_saved_a1 = 0x00004000u;
    pistorm_fast_rom_saved_a3 = 0x00200000u;

    lowmem = (volatile uint32_t *)(uintptr_t)pistorm_fake_lowram_virt;
    lowmem[0] = BE32(0x4c4f574du);
    lowmem[1] = 0;
    *(volatile uint32_t *)(uintptr_t)(pistorm_fake_lowram_virt + 0x00010000u) = BE32(0x4c4f574du);

    for (uint32_t addr = 0x00000008u; addr <= 0x000000bcu; addr += 4u)
        pistorm_raw_write(addr, 0x00f80492u, 4);

    pistorm_raw_write(0x00da8000u, 1u, 1);
    pistorm_handle_special_write(0x00bfa001u, 0u, 1);
    pistorm_handle_special_write(0x00bfa201u, 0u, 1);
    pistorm_handle_special_write(0x00bfe001u, 0u, 1);
    pistorm_handle_special_write(0x00bfe201u, 3u, 1);
    pistorm_handle_special_write(0x00dff09au, 0x7fffu, 2);
    pistorm_handle_special_write(0x00dff09cu, 0x7fffu, 2);
    pistorm_handle_special_write(0x00dff096u, 0x7fffu, 2);
    pistorm_handle_special_write(0x00dff032u, 0x0174u, 2);
    pistorm_handle_special_write(0x00dff100u, 0x0200u, 2);
    pistorm_handle_special_write(0x00dff110u, 0x0000u, 2);
    pistorm_handle_special_write(0x00dff180u, 0x0111u, 2);

    pistorm_fast_rom_residents = pistorm_fast_rom_build_residents(rom_start, image_size, NULL);
    pistorm_fast_rom_qemu_boot = 1;
    pistorm_fast_rom_preexecbase_boot = 1;
    pistorm_rom_initial_pc = 0x00f8030au;
    kprintf("[BOOT] Fast QEMU ROM bootstrap enabled. Reset PC=%08x A0=%08x A3=%08x resident=%08x list=%08x\n",
        pistorm_rom_initial_pc,
        pistorm_fast_rom_saved_a1,
        pistorm_fast_rom_saved_a3,
        lowmem_resident,
        pistorm_fast_rom_residents);
    return 1;
}

static int pistorm_fast_makefunctions(struct M68KState *ctx)
{
    uint32_t a0 = BE32(ctx->A[0].u32);
    uint32_t a1 = BE32(ctx->A[1].u32);
    uint32_t a2 = BE32(ctx->A[2].u32);
    uint32_t saved_a3 = BE32(ctx->A[3].u32);
    uint32_t d0 = 0;
    uint32_t sp = BE32(ctx->A[7].u32);
    uint32_t ret;

    if (sp == 0)
        return 0;

    if (a2 != 0)
    {
        for (;;)
        {
            int16_t off = BE16(*(volatile uint16_t *)(uintptr_t)a1);
            uint32_t target;

            a1 += 2u;
            if (off == -1)
                break;

            target = a2 + (int32_t)off;
            a0 -= 4u;
            *(volatile uint32_t *)(uintptr_t)a0 = BE32(target);
            a0 -= 2u;
            *(volatile uint16_t *)(uintptr_t)a0 = BE16(0x4ef9u);
            d0 += 6u;
        }
    }
    else
    {
        for (;;)
        {
            uint32_t target = BE32(*(volatile uint32_t *)(uintptr_t)a1);

            a1 += 4u;
            if (target == 0xffffffffu)
                break;

            a0 -= 4u;
            *(volatile uint32_t *)(uintptr_t)a0 = BE32(target);
            a0 -= 2u;
            *(volatile uint16_t *)(uintptr_t)a0 = BE16(0x4ef9u);
            d0 += 6u;
        }
    }

    ret = BE32(*(volatile uint32_t *)(uintptr_t)sp);
    ctx->A[0].u32 = BE32(a0);
    ctx->A[1].u32 = BE32(a1);
    ctx->A[3].u32 = BE32(saved_a3);
    ctx->D[0].u32 = BE32(d0);
    ctx->A[7].u32 = BE32(sp + 4u);
    ctx->PC = BE32(ret);

    if (pistorm_mmio_trace || pistorm_step_trace || pistorm_tracepc_end > pistorm_tracepc_start)
    {
        kprintf("[BOOT] ARM32 PiStorm fast MakeFunctions builder returned pc=%08x a0=%08x d0=%08x a1=%08x a2=%08x\n",
            ret,
            a0,
            d0,
            BE32(ctx->A[1].u32),
            a2);
    }

    return 1;
}

static int pistorm_service_pending_interrupt(struct M68KState *ctx)
{
    uint16_t sr;
    uint16_t sr_copy;
    uint8_t level;
    uint32_t sp;
    uint32_t vector;
    uint32_t current_pc;
    uint32_t new_pc;
    uint32_t vbr;

    pistorm_refresh_video_state();
    level = ctx->INT.IPL;

    if (level == 0)
        return 0;

    if (level > 7)
    {
        if (pistorm_mmio_trace)
        {
            pistorm_trace_puts("[PISTORM:IRQ] invalid level=");
            pistorm_trace_put_hex32(level);
            pistorm_trace_puts(" pc=");
            pistorm_trace_put_hex32(BE32(ctx->PC));
            pistorm_trace_puts(" sp=");
            pistorm_trace_put_hex32(BE32(ctx->A[7].u32));
            put_char('\n');
        }

        ctx->INT.IPL = 0;
        return 0;
    }

    sr = BE16(ctx->SR);
    if (level != 7 && level <= ((sr & SR_IPL) >> SRB_IPL))
        return 0;

    sp = BE32(ctx->A[7].u32);

    if ((sr & SR_S) == 0)
    {
        ctx->USP.u32 = BE32(sp);
        if (sr & SR_M)
            sp = BE32(ctx->MSP.u32);
        else
            sp = BE32(ctx->ISP.u32);
    }

    sr_copy = sr;
    vector = 0x60u + ((uint32_t)level << 2);
    sr |= SR_S;
    sr &= ~(SR_T0 | SR_T1);
    sr &= ~SR_IPL;
    sr |= (uint16_t)((uint16_t)level << SRB_IPL);

    sp -= 8;
    current_pc = BE32(ctx->PC);

    *(volatile uint16_t *)(uintptr_t)sp = BE16(sr_copy);
    *(volatile uint32_t *)(uintptr_t)(sp + 2) = BE32(current_pc);
    *(volatile uint16_t *)(uintptr_t)(sp + 6) = BE16((uint16_t)vector);

    if (sr & SR_M)
        ctx->MSP.u32 = BE32(sp);
    else
        ctx->ISP.u32 = BE32(sp);

    ctx->A[7].u32 = BE32(sp);
    ctx->SR = BE16(sr);

    vbr = BE32(ctx->VBR);
    new_pc = BE32(*(volatile uint32_t *)(uintptr_t)(vbr + vector));
    ctx->PC = BE32(new_pc);

    if (pistorm_mmio_trace)
    {
        pistorm_trace_puts("[PISTORM:IRQ] level=");
        pistorm_trace_put_hex32(level);
        pistorm_trace_puts(" vector=");
        pistorm_trace_put_hex32(vector);
        pistorm_trace_puts(" pc=");
        pistorm_trace_put_hex32(new_pc);
        pistorm_trace_puts(" sp=");
        pistorm_trace_put_hex32(sp);
        put_char('\n');
    }

    return 1;
}

/* Amiga checksum, taken from the AArch64 startup path. */
static int amiga_checksum(uint8_t *mem, uintptr_t size, uintptr_t chkoff, int update)
{
    uint32_t oldcksum = 0, cksum = 0, prevck = 0;
    uintptr_t i;

    for (i = 0; i < size; i += 4)
    {
        uint32_t val = (mem[i + 0] << 24) |
                       (mem[i + 1] << 16) |
                       (mem[i + 2] << 8) |
                       (mem[i + 3] << 0);

        if (update && (i == chkoff))
        {
            oldcksum = val;
            val = 0;
        }

        cksum += val;
        if (cksum < prevck)
            cksum++;
        prevck = cksum;
    }

    cksum = ~cksum;

    if (update && cksum != oldcksum)
    {
        kprintf("[BOOT] Updating checksum from 0x%08x to 0x%08x\n", oldcksum, cksum);

        mem[chkoff + 0] = (cksum >> 24) & 0xff;
        mem[chkoff + 1] = (cksum >> 16) & 0xff;
        mem[chkoff + 2] = (cksum >> 8) & 0xff;
        mem[chkoff + 3] = (cksum >> 0) & 0xff;

        return 1;
    }

    return 0;
}

static int load_pistorm_rom_image(void *image_start, uintptr_t image_size)
{
    const uintptr_t rom_base = 0x00f80000;
    const uintptr_t rom_overlay_base = 0x00f00000;
    uint8_t *rom_start = (uint8_t *)rom_base;
    extern uint32_t overlay;

    kprintf("[BOOT] Loading ROM from %p, size %d\n", image_start, image_size);

    pistorm_qemu_rom_bus_hole = pistorm_qemu_model;
    if (pistorm_qemu_rom_bus_hole_override >= 0)
        pistorm_qemu_rom_bus_hole = (uint32_t)pistorm_qemu_rom_bus_hole_override;
    if (pistorm_qemu_rom_bus_hole)
        kprintf("[BOOT] ARM32 PiStorm QEMU ROM bus-hole mode enabled for 0x00a00000-0x00b7ffff and 0x00c00000-0x00d9ffff\n");
    else if (pistorm_qemu_model)
        kprintf("[BOOT] ARM32 PiStorm QEMU ROM bus-hole mode disabled by bootargs\n");

    pistorm_init_fake_lowram();

    switch (image_size)
    {
        case 262144:
            DuffCopy((uint32_t *)(uintptr_t)0x00f00000, image_start, 262144 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00f40000, image_start, 262144 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00f80000, image_start, 262144 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00fc0000, image_start, 262144 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00e00000, (const uint32_t *)(uintptr_t)0x00f80000, 524288 / 4);
            break;
        case 524288:
            DuffCopy((uint32_t *)(uintptr_t)0x00f00000, image_start, 524288 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00e00000, image_start, 524288 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00f80000, image_start, 524288 / 4);
            break;
        case 1048576:
            DuffCopy((uint32_t *)(uintptr_t)0x00f00000, image_start, 1048576 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00e00000, image_start, 524288 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00f80000, (const uint32_t *)((uintptr_t)image_start + 524288), 524288 / 4);
            break;
        case 2097152:
            DuffCopy((uint32_t *)(uintptr_t)0x00f00000, (const uint32_t *)((uintptr_t)image_start + 1048576), 1048576 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00e00000, image_start, 524288 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00a80000, (const uint32_t *)((uintptr_t)image_start + 524288), 524288 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00b00000, (const uint32_t *)((uintptr_t)image_start + 2 * 524288), 524288 / 4);
            DuffCopy((uint32_t *)(uintptr_t)0x00f80000, (const uint32_t *)((uintptr_t)image_start + 3 * 524288), 524288 / 4);
            break;
        default:
            kprintf("[BOOT] Unsupported ROM size %d\n", image_size);
            return 0;
    }

    if (rom_start[2] == 0xf9 && rom_start[3] == 0x4e)
    {
        kprintf("[BOOT] Byte-swapped ROM detected. Fixing...\n");
        swap_rom_bytes(rom_start, 524288);
        swap_rom_bytes((uint8_t *)(uintptr_t)rom_overlay_base, 1048576);

        if (image_size == 1048576 || image_size == 2097152)
        {
            swap_rom_bytes((uint8_t *)(uintptr_t)0x00e00000, 524288);

            if (image_size == 2097152)
            {
                swap_rom_bytes((uint8_t *)(uintptr_t)0x00a80000, 1048576);
            }
        }
    }

    if (pistorm_recalc_checksum && image_size >= 524288)
        amiga_checksum((uint8_t *)(uintptr_t)rom_base, 524288, 524288 - 24, 1);

    /*
     * Keep the reset vector table visible at address 0 while overlay is active.
     * On fake-lowram QEMU boots, do not prefill the underlying RAM with ROM
     * contents, otherwise once OVL turns off Kickstart still sees stale ROM
     * words at address 0 instead of clean low RAM.
     */
    if (pistorm_fake_lowram_mb == 0)
        DuffCopy((uint32_t *)(uintptr_t)0x00000000, (const uint32_t *)rom_base, 4096 / 4);

    pistorm_rom_initial_sp = BE32(*(uint32_t *)rom_base);
    pistorm_rom_initial_pc = BE32(*((uint32_t *)rom_base + 1));
    pistorm_rom_exec_entry_boot = 0;
    pistorm_diagrom_boot = 0;

    /*
     * Some ROM images enter through an Amiga ROMTag/JMP stub rather than a
     * raw reset vector table. Synthesise reset state so the ARM32 runtime can
     * start them like the normal ROM path.
     */
    if (rom_start[0] == 0x11 && rom_start[1] == 0x14 && rom_start[2] == 0x4e && rom_start[3] == 0xf9 &&
        pistorm_rom_initial_pc >= 0x00e00000 && pistorm_rom_initial_pc < 0x01000000)
    {
        pistorm_rom_exec_entry_boot = 1;
        pistorm_diagrom_boot = pistorm_rom_is_diagrom(rom_start, image_size);
        pistorm_rom_initial_sp = top_of_ram;
        kprintf("[BOOT] ROM exec-entry image detected. Reset SP=%08x PC=%08x\n",
            pistorm_rom_initial_sp, pistorm_rom_initial_pc);
        (void)pistorm_try_fast_rom_exec_kick_boot(rom_start, image_size);
    }

    /*
     * ROM exec-entry images jump straight into the ROM body and probe
     * low RAM very early, so leave page 0 backed by RAM at entry. Raw
     * Kickstart-style images still need the traditional overlay-on reset view.
     */
    pistorm_force_overlay_state(pistorm_rom_exec_entry_boot ? 0u : 1u);

    return 1;
}
#endif

void print_build_id()
{
    const uint8_t *build_id_data = &g_note_build_id.bid_Data[g_note_build_id.bid_NameLen];

    kprintf("[BOOT] Build ID: ");
    for (unsigned i = 0; i < g_note_build_id.bid_DescLen; ++i) {
        kprintf("%02x", build_id_data[i]);
    }
    kprintf("\n");
}

void boot(uintptr_t dummy, uintptr_t arch, uintptr_t atags, uintptr_t dummy2)
{
    (void)dummy; (void)arch; (void)atags; (void)dummy2;
    uint32_t tmp, initcr;
    uint32_t isar;
    of_node_t *e;
    int skip_relocation = 0;

    /*
     * Enable caches and branch prediction. Also enabled unaligned memory
     * access. Exceptions are set to run in big-endian mode and this is the mode
     * in which page tables are written if the system is set to run in BE.
     */
    asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(initcr));
    tmp = initcr;
    tmp |= (1 << 2) | (1 << 12) | (1 << 11);    /* I and D caches, branch prediction */
    tmp = (tmp & ~2) | (1 << 22);               /* Unaligned access enable */
#if EMU68_HOST_BIG_ENDIAN
    tmp |= (1 << 25);                           /* EE bit for exceptions set - big endian */
                                                /* This bit sets also endianess of page tables */
#endif
    asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(tmp));
    asm volatile ("mrc p15, 0, %0, c0, c2, 0" : "=r"(isar));

    uint32_t fpsid, MVFR1, MVFR0;
    asm volatile("VMRS %0, FPSID":"=r"(fpsid));
    asm volatile("VMRS %0, MVFR1":"=r"(MVFR1));
    asm volatile("VMRS %0, MVFR0":"=r"(MVFR0));

#if SET_FEATURES_AT_RUNTIME
    if ((isar & 0x0f000000) == 0x02000000) {
        Features.ARM_SUPPORTS_DIV = 1;
    }
    if ((isar & 0x0000000f) == 0x00000001) {
        Features.ARM_SUPPORTS_SWP = 1;
    }
    if ((isar & 0x000000f0) == 0x00000010) {
        Features.ARM_SUPPORTS_BITCNT = 1;
    }
    if ((isar & 0x00000f00) == 0x00000100) {
        Features.ARM_SUPPORTS_BITFLD = 1;
    }
    if ((MVFR0 & 0x00f00000) == 0x00100000) {
        Features.ARM_SUPPORTS_SQRT = 1;
    }
    if ((MVFR0 & 0x000f0000) == 0x00010000) {
        Features.ARM_SUPPORTS_VDIV = 1;
    }
#endif

    /* Create 1:1 map for whole memory in order to access the device tree, which can reside anywhere in ram */
    for (int i=8; i < 4096-8; i++)
    {
        /* Caches write-through, write allocate, access for all */
        mmu_table[i] = (i << 20) | 0x0c06; //0x1c0e;
    }

    arm_flush_cache((uint32_t)mmu_table, sizeof(mmu_table));

    tlsf = tlsf_init();
    tlsf_add_memory(tlsf, &__bootstrap_end, 0xffff0000 - (uintptr_t)&__bootstrap_end);
    dt_parse((void*)atags);

    e = dt_find_node("/");
    char *compatible = (char*)0;
#ifdef PISTORM
    char *model = (char*)0;
#endif
    int raspi4 = 0;
    if (e)
    {
        of_property_t *p = dt_find_property(e, "compatible");
        if (p)
        {
            compatible = p->op_value;
            if (compatible[12] >= '4')
                raspi4 = 1;
        }

#ifdef PISTORM
        p = dt_find_property(e, "model");
        if (p && p->op_value)
        {
            model = p->op_value;
            pistorm_qemu_model = strstr(model, "QEMU") != NULL;
        }
#endif
    }

    e = dt_find_node("/chosen");
    if (e)
    {
        of_property_t *p = dt_find_property(e, "bootargs");
        if (p && p->op_value)
        {
            skip_relocation = strstr(p->op_value, "skip_reloc") != NULL;
#ifdef PISTORM
            pistorm_recalc_checksum = strstr(p->op_value, "checksum_rom") != NULL;
            pistorm_allow_fast_rom_qemu_boot =
                strstr(p->op_value, "fast_rom") != NULL ||
                strstr(p->op_value, "fast_headerized") != NULL;
            if (strstr(p->op_value, "no_bus_hole"))
                pistorm_qemu_rom_bus_hole_override = 0;
            else if (strstr(p->op_value, "force_bus_hole"))
                pistorm_qemu_rom_bus_hole_override = 1;
            {
                const char *tok = find_token(p->op_value, "fake_lowram=");

                if (tok)
                {
                    uint32_t val = 0;
                    const char *c = &tok[12];

                    for (int i = 0; i < 2; i++)
                    {
                        if (c[i] < '0' || c[i] > '9')
                            break;

                        val = val * 10 + c[i] - '0';
                    }

                    if (val > 8)
                        val = 8;

                    pistorm_boot_fake_lowram_mb = val;
                }
            }
#endif
        }
    }

    /* Prepare mapping for peripherals. Use the data from device tree here */
    e = dt_find_node("/soc");
    if (e)
    {
        of_property_t *p = dt_find_property(e, "ranges");
        uint32_t *ranges = p->op_value;
        int32_t len = p->op_length;
        uint32_t start_map = 0xf20;

        while (len > 0)
        {
            uint32_t addr_bus, addr_cpu;
            uint32_t addr_len;

            addr_bus = BE32(*ranges++);
            if (raspi4)
                ranges++;
            addr_cpu = BE32(*ranges++);
            addr_len = BE32(*ranges++);

            (void)addr_bus;

            if (addr_len < 0x00100000)
                addr_len = 0x00100000;

            /* Prepare mapping - device type */
            for (unsigned i=0; i < (addr_len >> 20); i++)
            {
                /* Strongly-ordered device, uncached, 16MB region */
                mmu_table[start_map + i] = (i << 20) | addr_cpu | 0x0c06;
            }

            ranges[-2] = BE32(start_map << 20);

            start_map += addr_len >> 20;

            if (raspi4)
                len -= 16;
            else
                len -= 12;
        }
    }

    arm_flush_cache((intptr_t)mmu_table, sizeof(mmu_table));

    setup_serial();

    kprintf("[BOOT] Booting %s\n", bootstrapName);
    kprintf("[BOOT] Boot address is %08x\n", _start);

    print_build_id();
    disasm_init();

    kprintf("[BOOT] ARM stack top at %p\n", tmp_stack_ptr);
    kprintf("[BOOT] Bootstrap ends at %08x\n", &__bootstrap_end);
    kprintf("[BOOT] ISAR=%08x, FPSID=%08x, MVFR0=%08x, MVFR1=%08x\n", isar, fpsid, MVFR0, MVFR1);
#if SET_FEATURES_AT_RUNTIME
    kprintf("[BOOT] Detected features:%s%s%s%s%s%s\n",
#else
    kprintf("[BOOT] Selected features:%s%s%s%s%s%s\n",
#endif
        Features.ARM_SUPPORTS_DIV ? " DIV" : "",
        Features.ARM_SUPPORTS_BITFLD ? " BITFLD" : "",
        Features.ARM_SUPPORTS_BITCNT ? " BITCNT" : "",
        Features.ARM_SUPPORTS_SWP ? " SWP":"",
        Features.ARM_SUPPORTS_VDIV ? " VDIV":"",
        Features.ARM_SUPPORTS_SQRT ? " VSQRT":"");
    kprintf("[BOOT] Args=%08x,%08x,%08x,%08x\n", dummy, arch, atags, dummy2);
    kprintf("[BOOT] Local memory pool:\n");
    kprintf("[BOOT]    %08x - %08x (size=%d)\n", &__bootstrap_end, 0xffff0000, 0xffff0000 - (uintptr_t)&__bootstrap_end);

    e = dt_find_node("/memory");
    if (e)
    {
        of_property_t *p = dt_find_property(e, "reg");
        uint32_t *range = p->op_value;

        if (raspi4)
            range++;

        top_of_ram = BE32(range[0]) + BE32(range[1]);
        intptr_t kernel_new_loc = top_of_ram - 0x00800000;
        top_of_ram = kernel_new_loc - 0x1000;

        range[1] = BE32(BE32(range[1])-0x00800000);

        kprintf("[BOOT] System memory: %p-%p\n", BE32(range[0]), BE32(range[0]) + BE32(range[1]) - 1);

        for (uint32_t i=BE32(range[0]) >> 20; i < (BE32(range[0]) + BE32(range[1])) >> 20; i++)
        {
            /* Caches write-through, write allocate, access for all */
            mmu_table[i] = (i << 20) | 0x1c0e;
        }

        if (skip_relocation)
        {
            top_of_ram = BE32(range[0]) + BE32(range[1]) - 0x1000;
            kprintf("[BOOT] Skipping kernel relocation\n");
        }
        else
        {
            kprintf("[BOOT] Adjusting MMU map\n");

            for (int i=0; i < 8; i++)
            {
                /* Caches write-through, write allocate, access for all */
                mmu_table[0xff8 + i] = ((kernel_new_loc & 0xfff00000) + (i << 20)) | 0x1c0e;
            }
            kprintf("[BOOT] Moving kernel to %p\n", (void*)kernel_new_loc);
            DuffCopy((void*)(kernel_new_loc+4), (void*)4, 0x00800000 / 4 - 1);
            arm_flush_cache(kernel_new_loc, 0x00800000);
            arm_flush_cache((intptr_t)mmu_table, sizeof(mmu_table));

            /* Load new pointer to the mmu table */
            asm volatile("dsb; mcr p15,0,%0,c2,c0,0; dsb; isb"::"r"(((uint32_t)mmu_table_ptr & 0x000fffff) | (kernel_new_loc & 0xfff00000)));
            /* Invalidate entire TLB */
            asm volatile("dsb; mcr p15,0,%0,c8,c7,0; dsb; isb"::"r"(0));
        }
    }

    if (get_max_clock_rate(3) != get_clock_rate(3)) {
        kprintf("[BOOT] Changing ARM clock rate from %d MHz to %d MHz\n", get_clock_rate(3)/1000000, get_max_clock_rate(3)/1000000);
        set_clock_rate(3, get_max_clock_rate(3));
    } else {
        kprintf("[BOOT] ARM Clock at %d MHz\n", get_clock_rate(3) / 1000000);
    }

    display_logo();

#ifdef PISTORM
    {
        extern void (*__init_start)();
        void (**InitFunctions)() = &__init_start;

        while (*InitFunctions)
        {
            (*InitFunctions)();
            InitFunctions++;
        }
    }
#endif

    e = dt_find_node("/chosen");
    if (e)
    {
        void *image_start, *image_end;
        void *hunks;
        uintptr_t hunk_base = 0x7f000000;
#ifdef PISTORM
        uintptr_t image_size;
        uint32_t image_magic;
#endif
        of_property_t *p = dt_find_property(e, "linux,initrd-start");
        if (p == NULL || p->op_value == NULL)
        {
            kprintf("[BOOT] No initrd start property in /chosen\n");
            while (1);
        }

        image_start = (void*)(intptr_t)BE32(*(uint32_t*)p->op_value);
        p = dt_find_property(e, "linux,initrd-end");
        if (p == NULL || p->op_value == NULL)
        {
            kprintf("[BOOT] No initrd end property in /chosen\n");
            while (1);
        }

        image_end = (void*)(intptr_t)BE32(*(uint32_t*)p->op_value);
#ifdef PISTORM
        image_size = (uintptr_t)image_end - (uintptr_t)image_start;
        image_magic = BE32(*(uint32_t*)image_start);
#endif

        kprintf("[BOOT] Loading executable from %p-%p\n", image_start, image_end);
#ifdef PISTORM
        if (image_magic != 0x000003f3 && load_pistorm_rom_image(image_start, image_size))
        {
            start_emu(NULL);
            while (1);
        }
#endif
        if (skip_relocation)
        {
            uint32_t hunk_size = GetHunkFileSize(image_start);
            hunk_base = ((uintptr_t)top_of_ram - 0x00100000 - hunk_size) & ~31u;
        }
        hunks = LoadHunkFile(image_start, (void*)hunk_base);
        if (hunks == NULL)
        {
            kprintf("[BOOT] Failed to load initrd image\n");
            while (1);
        }

        start_emu((void *)((intptr_t)hunks + 4));
    }
    else
    {
        kprintf("[BOOT] No /chosen node in device tree\n");
    }

    while(1);
}




uint8_t m68kcode[] = {
/*
    0x7c, 0x20,
    0x7e, 0xff,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x52, 0x80, 0x52, 0x80, 0x52, 0x80,
    0x51, 0xcf, 0xff, 0x9e,
    0x51, 0xce, 0xff, 0x98,
    0x4e, 0x75,
    0xff, 0xff
*/
    /*0x74,0x08,
    0x20,0x3c,0xaa,0x55,0xaa,0x55,
    0x22,0x3c,0x87,0x65,0x43,0x21,
    0xe5,0xb8,
    0xe4,0xb9,
    0x4e,0x75,*/
    /*
    0x20,0x3c,0xde,0xad,0xbe,0xef,
    0x22,0x3c,0x12,0x34,0x56,0x78,
    0x34,0x3c,0xaa,0x55,
    0x16,0x3c,0x00,0xc7,
    0x4e,0x75,
*/
    0x2a,0x49,  // movea.l  a1,a5
    0x2c,0x4a,  // movea.l  a2,a6
    0x22,0x5e,  //           	movea.l (a6)+,a1
    0x24,0x5e,  //           	movea.l (a6)+,a2
    0x26,0x5e,  //           	movea.l (a6)+,a3
    0x28,0x56,  //           	movea.l (a6),a4
    0x2c,0x3c,0x55,0x55 ,0x55,0x55, // 	move.l #1431655765,d6
    0x2e,0x3c ,0x33,0x33,0x33,0x33, // 	move.l #858993459,d7
    0x28,0x3c ,0x0f,0x0f ,0x0f,0x0f,// 	loop: move.l #252645135,d4
    0x2a,0x3c ,0x00,0xff ,0x00,0xff, //	move.l #16711935,d5
    0x20,0x18, // move.l (a0)+, d0
    0xc0,0x84, //           	and.l d4,d0
    0x22,0x18, // 	move.l (a0)+,d1
    0xe9,0x88, //           	lsl.l #4,d0
    0xc2,0x84, //           	and.l d4,d1
    0x80,0x81, //           	or.l d1,d0
    0x22,0x18, // 	move.l (a0)+,d1
    0xc2,0x84, //           	and.l d4,d1
    0x24,0x18, // 	move.l (a0)+,d2
    0xe9,0x89, //           	lsl.l #4,d1
    0xc4,0x84, //           	and.l d4,d2
    0x82,0x82, //           	or.l d2,d1
    0x24,0x00, //           	move.l d0,d2
    0x26,0x01, //           	move.l d1,d3
    0xc0,0x85, //           	and.l d5,d0
    0xc6,0x85, //           	and.l d5,d3
    0xb1,0x82, //           	eor.l d0,d2
    0xb7,0x81, //           	eor.l d3,d1
    0xe1,0x88, //           	lsl.l #8,d0
    0xe0,0x89, //           	lsr.l #8,d1
    0x80,0x83, //           	or.l d3,d0
    0x82,0x82, //           	or.l d2,d1
    0x24,0x00, //           	move.l d0,d2
    0x26,0x01, //           	move.l d1,d3
    0xc0,0x86, //           	and.l d6,d0
    0xc6,0x86, //           	and.l d6,d3
    0xb1,0x82, //           	eor.l d0,d2
    0xb7, 0x81,//           	eor.l d3,d1
    0xd6,0x83, //           	add.l d3,d3
    0xe2,0x8a, //           	lsr.l #1,d2
    0x80,0x83, //           	or.l d3,d0
    0x82,0x82, //           	or.l d2,d1
    0x24,0x18, // 	move.l (a0)+,d2
    0xc4,0x84, //           	and.l d4,d2
    0x26,0x18, // 	move.l (a0)+,d3
    0xe9,0x8a, //           	lsl.l #4,d2
    0xc6,0x84, //           	and.l d4,d3
    0x84,0x83, //           	or.l d3,d2
    0x26,0x18, // 	move.l (a0)+,d3
    0xc6,0x84, //           	and.l d4,d3
    0xc8,0x98, // 	move.l (a0)+,d4
    0xe9,0x8b, //           	lsl.l #4,d3
    0x86,0x84, //           	or.l d4,d3
    0x28,0x02, //           	move.l d2,d4
    0xc4,0x85, //           	and.l d5,d2
    0xca,0x83, //           	and.l d3,d5
    0xb5,0x84, //           	eor.l d2,d4
    0xbb,0x83, //           	eor.l d5,d3
    0xe1,0x8a, //           	lsl.l #8,d2
    0xe0,0x8b, //           	lsr.l #8,d3
    0x84,0x85, //           	or.l d5,d2
    0x86,0x84, //           	or.l d4,d3
    0x28,0x02, //           	move.l d2,d4
    0x2a,0x03, //           	move.l d3,d5
    0xc4,0x86, //           	and.l d6,d2
    0xca,0x86, //           	and.l d6,d5
    0xb5,0x84, //           	eor.l d2,d4
    0xbb,0x83, //           	eor.l d5,d3
    0xda,0x85, //           	add.l d5,d5
    0xe2,0x8c, //           	lsr.l #1,d4
    0x84,0x85, //           	or.l d5,d2
    0x86,0x84, //           	or.l d4,d3
    0x48,0x42, //           	swap d2
    0x48,0x43, //           	swap d3
    0xb1,0x42, //           	eor.w d0,d2
    0xb3,0x43, //           	eor.w d1,d3
    0xb5,0x40, //           	eor.w d2,d0
    0xb7,0x41, //           	eor.w d3,d1
    0xb1,0x42, //           	eor.w d0,d2
    0xb3,0x43, //           	eor.w d1,d3
    0x48,0x42, //           	swap d2
    0x48,0x43, //           	swap d3
    0x28,0x00, //           	move.l d0,d4
    0x2a,0x02, //           	move.l d2,d5
    0xc0,0x87, //           	and.l d7,d0
    0xca,0x87, //           	and.l d7,d5
    0xb1,0x84, //           	eor.l d0,d4
    0xbb,0x82, //           	eor.l d5,d2
    0xe5,0x88, //           	lsl.l #2,d0
    0xe4,0x8a, //           	lsr.l #2,d2
    0x80,0x85, //           	or.l d5,d0
    0x22,0xc0, //           	move.l d0,(a1)+
    0x84,0x84, //           	or.l d4,d2
    0x26,0xc2, //           	move.l d2,(a3)+
    0x28,0x01, //           	move.l d1,d4
    0x2a,0x03, //           	move.l d3,d5
    0xc2,0x87, //           	and.l d7,d1
    0xca,0x87, //           	and.l d7,d5
    0xb3,0x84, //           	eor.l d1,d4
    0xbb,0x83, //           	eor.l d5,d3
    0xe5,0x89, //           	lsl.l #2,d1
    0xe4,0x8b, //           	lsr.l #2,d3
    0x82,0x85, //           	or.l d5,d1
    0x24,0xc1, //           	move.l d1,(a2)+
    0x86,0x84, //           	or.l d4,d3
    0x28,0xc3, //           	move.l d3,(a4)+
    0xb1,0xcd, //       cmpa.l a5,a0
    0x6d,0x00,0xff,0x30, // blt.w loop
    0x4e,0x75,
};

void *m68kcodeptr = m68kcode;

uint32_t boot_data[128];

void print_context(struct M68KState *m68k)
{
    printf("\nM68K Context:\n");

    for (int i=0; i < 8; i++) {
        if (i==4)
            printf("\n");
        printf("    D%d = 0x%08x", i, BE32(m68k->D[i].u32));
    }
    printf("\n");

    for (int i=0; i < 8; i++) {
        if (i==4)
            printf("\n");
        printf("    A%d = 0x%08x", i, BE32(m68k->A[i].u32));
    }
    printf("\n");

    printf("    PC = 0x%08x    SR = ", BE32((int)m68k->PC));
    uint16_t sr = BE16(m68k->SR);
    if (sr & SR_X)
        printf("X");
    else
        printf(".");

    if (sr & SR_N)
        printf("N");
    else
        printf(".");

    if (sr & SR_Z)
        printf("Z");
    else
        printf(".");

    if (sr & SR_V)
        printf("V");
    else
        printf(".");

    if (sr & SR_C)
        printf("C");
    else
        printf(".");

    printf("    CACR=0x%08x    VBR = 0x%08x\n", BE32(m68k->CACR), BE32(m68k->VBR));
    printf("    USP= 0x%08x    MSP= 0x%08x    ISP= 0x%08x\n", BE32(m68k->USP.u32), BE32(m68k->MSP.u32), BE32(m68k->ISP.u32));

    for (int i=0; i < 8; i++) {
        union {
            double d;
            uint32_t u[2];
        } u;
        if (i==4)
            printf("\n");
        u.d = *(double*)&m68k->FP[i];
        printf("    FP%d = %08x%08x", i, u.u[0], u.u[1]);
    }
    printf("\n");

    printf("    FPSR=0x%08x    FPIAR=0x%08x   FPCR=0x%04x\n", BE32(m68k->FPSR), BE32(m68k->FPIAR), BE32(m68k->FPCR));

}

//#define DATA_SIZE 25600*4800

uint8_t *chunky; //[DATA_SIZE];
//uint8_t *plane0; //[DATA_SIZE ];
//uint8_t *plane1; //[DATA_SIZE ];
//uint8_t *plane2; //[DATA_SIZE ];
//uint8_t *plane3; //[DATA_SIZE ];

//uint8_t *bitmap[4];// = {
    //plane0, plane1, plane2, plane3
//};

uint32_t last_PC = 0xffffffff;

#ifndef __aarch64__
#ifdef PISTORM
void pistorm_special_read_trampoline_probe(uint32_t address, uint32_t size, uint32_t value)
{
    if (pistorm_mmio_trace &&
        (((size == 2) && (last_PC == 0x00f80444u || last_PC == 0x00f8044eu)) ||
         ((size == 4) && (address == 0x000000bcu))) &&
        pistorm_special_read_wrapper_probe_seen < 16)
    {
        pistorm_special_read_wrapper_probe_seen++;
        kprintf("[PISTORM:WRAPRET] last=%08x read @ %08x -> %08x\n", last_PC, address, value);
    }
}

void pistorm_special_read_trampoline_entry_probe(uint32_t address, uint32_t size)
{
    if (pistorm_mmio_trace &&
        (((size == 2) && (last_PC == 0x00f80444u || last_PC == 0x00f8044eu)) ||
         ((size == 4) && (address == 0x000000bcu))) &&
        pistorm_special_read_wrapper_entry_seen < 16)
    {
        pistorm_special_read_wrapper_entry_seen++;
        kprintf("[PISTORM:WRAPIN] last=%08x read @ %08x size=%u\n", last_PC, address, size);
    }
}

void pistorm_special_read_callsite_probe(uint32_t address, uint32_t size)
{
    if (pistorm_mmio_trace &&
        (((size == 2) && (last_PC == 0x00f80444u || last_PC == 0x00f8044eu)) ||
         ((size == 4) && (address == 0x000000bcu))) &&
        pistorm_special_read_callsite_seen < 16)
    {
        pistorm_special_read_callsite_seen++;
        kprintf("[PISTORM:CALLSITE] last=%08x read @ %08x size=%u\n", last_PC, address, size);
    }
}

uint32_t __attribute__((naked, noinline)) pistorm_handle_special_read_trampoline(uint32_t address, uint32_t size)
{
    (void)address;
    (void)size;

    asm volatile(
        "push {r4-r7, ip, lr}\n"
        "mov r4, sp\n"
        "mov r5, r0\n"
        "mov r6, r1\n"
        "bic sp, sp, #7\n"
        "mov r0, r5\n"
        "mov r1, r6\n"
        "bl pistorm_special_read_trampoline_entry_probe\n"
        "mov r0, r5\n"
        "mov r1, r6\n"
        "bl pistorm_handle_special_read\n"
        "mov r7, r0\n"
        "mov r0, r5\n"
        "mov r1, r6\n"
        "mov r2, r7\n"
        "bl pistorm_special_read_trampoline_probe\n"
        "mov r0, r7\n"
        "mov sp, r4\n"
        "pop {r4-r7, ip, lr}\n"
        "bx lr\n");
}
#endif /* PISTORM */

static void __attribute__((naked, noinline)) armhf_call_translation_unit(void (*arm_code)(struct M68KState *), struct M68KState *ctx)
{
    (void)arm_code;
    (void)ctx;

    asm volatile(
        /* Keep the translated unit isolated from the C caller's frame state. */
        "sub sp, sp, #4\n"
        "push {r4-r11, lr}\n"
        "mov r11, r1\n"
        "mov r12, r0\n"
        "mov r0, r1\n"
        "blx r12\n"
        "pop {r4-r11, lr}\n"
        "add sp, sp, #4\n"
        "bx lr\n");
}
#endif

void start_emu(void *addr)
{
#ifndef __aarch64__

    struct M68KState *m68k;
    void *fdt = dt_fdt_base();
    uint64_t t1=0, t2=0;

    void (*arm_code)(struct M68KState *);

    struct M68KTranslationUnit * unit = (void*)0;
    struct M68KState __m68k;
    extern int disasm;
    extern int debug;
    uint32_t emu68_icnt = EMU68_M68K_INSN_DEPTH;
    uint32_t emu68_irng = EMU68_BRANCH_INLINE_DISTANCE;
    uint32_t emu68_lcnt = EMU68_MAX_LOOP_COUNT;

    m68k = &__m68k;

    bzero(&__m68k, sizeof(__m68k));
    __m68k_state = &__m68k;
    __m68k.JIT_UNIT_COUNT = 0;
    __m68k.JIT_SOFTFLUSH_THRESH = EMU68_WEAK_CFLUSH_LIMIT;
    __m68k.JIT_CONTROL = EMU68_WEAK_CFLUSH ? JCCF_SOFT : 0;
    __m68k.JIT_CONTROL |= (emu68_icnt & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
    __m68k.JIT_CONTROL |= (emu68_irng & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
    __m68k.JIT_CONTROL |= (emu68_lcnt & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;

    M68K_InitializeCache();
    __m68k.JIT_CACHE_TOTAL = tlsf_get_total_size(jit_tlsf);
    __m68k.JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);

for (int i=1; i < 2; i++)
{

    bzero(&__m68k, sizeof(__m68k));
    __m68k_state = &__m68k;
    __m68k.JIT_CACHE_TOTAL = tlsf_get_total_size(jit_tlsf);
    __m68k.JIT_CACHE_FREE = tlsf_get_free_size(jit_tlsf);
    __m68k.JIT_UNIT_COUNT = 0;
    __m68k.JIT_SOFTFLUSH_THRESH = EMU68_WEAK_CFLUSH_LIMIT;
    __m68k.JIT_CONTROL = EMU68_WEAK_CFLUSH ? JCCF_SOFT : 0;
    __m68k.JIT_CONTROL |= (emu68_icnt & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
    __m68k.JIT_CONTROL |= (emu68_irng & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
    __m68k.JIT_CONTROL |= (emu68_lcnt & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;
#ifdef PISTORM
    if (addr == NULL &&
        (pistorm_fast_rom_qemu_boot ||
         (pistorm_diagrom_boot && pistorm_allow_fast_diagrom_qemu_boot)))
    {
        /*
         * The ARM32 QEMU ROM bring-up paths need conservative unit sizing so
         * the runtime returns to the outer loop frequently enough for the
         * targeted bootstrap hooks to fire. This is already required for the
         * fast ROM bootstrap path, and the fast DiagROM path has the
         * same constraint once it gets past the early serial / memory probes.
         * These settings stay limited to ROM bootstrap mode and do not affect
         * the normal synthetic HUNK smoke paths.
         */
        __m68k.JIT_CONTROL &= ~((JCCB_INSN_DEPTH_MASK << JCCB_INSN_DEPTH) |
                                (JCCB_INLINE_RANGE_MASK << JCCB_INLINE_RANGE) |
                                (JCCB_LOOP_COUNT_MASK << JCCB_LOOP_COUNT));
        __m68k.JIT_CONTROL |= (1u & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
        __m68k.JIT_CONTROL |= (1u & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
        __m68k.JIT_CONTROL |= (1u & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;
    }
#endif
    last_PC = 0xffffffff;
    //memset(&stack, 0xaa, sizeof(stack));
#ifdef PISTORM
    if (addr == NULL)
    {
        extern uint32_t overlay;

        if (pistorm_rom_initial_sp == 0 || pistorm_rom_initial_pc == 0)
        {
            kprintf("[BOOT] No valid PiStorm ROM reset vectors were loaded. SP=%08x PC=%08x\n",
                pistorm_rom_initial_sp, pistorm_rom_initial_pc);
            while (1);
        }

        m68k->A[6].u32 = BE32((uint32_t)(uintptr_t)fdt);
        m68k->ISP.u32 = BE32(pistorm_rom_initial_sp);
        m68k->A[7].u32 = m68k->ISP.u32;
        m68k->PC = BE32(pistorm_rom_initial_pc);
        m68k->SR = BE16(SR_S | SR_IPL);
        m68k->FPCR = 0;

        if (pistorm_fast_rom_qemu_boot)
        {
            if (pistorm_fast_rom_preexecbase_boot)
            {
                uint32_t sp = BE32(m68k->A[7].u32) - 4u;

                m68k->A[0].u32 = BE32(pistorm_fast_rom_saved_a1);
                m68k->A[3].u32 = BE32(pistorm_fast_rom_saved_a3);
                m68k->A[7].u32 = BE32(sp);
                *(volatile uint32_t *)(uintptr_t)sp = 0;
                kprintf("[BOOT] ARM32 fast ROM entry seeds a0=%08x a3=%08x sp=%08x\n",
                    pistorm_fast_rom_saved_a1,
                    pistorm_fast_rom_saved_a3,
                    sp);
            }
            else
            {
                m68k->D[0].u32 = BE32(pistorm_fast_rom_low0);
                m68k->A[1].u32 = BE32(pistorm_fast_rom_saved_a1);
                m68k->A[2].u32 = BE32(0x00000017u);
                m68k->A[3].u32 = BE32(pistorm_fast_rom_saved_a3);
                m68k->A[6].u32 = BE32(pistorm_fast_rom_a6);
                kprintf("[BOOT] ARM32 fast ROM entry seeds d0=%08x a1=%08x a3=%08x a6=%08x\n",
                    pistorm_fast_rom_low0,
                    pistorm_fast_rom_saved_a1,
                    pistorm_fast_rom_saved_a3,
                    pistorm_fast_rom_a6);
            }
        }

        pistorm_intena_shadow = 0;
        pistorm_intreq_shadow = 0;
        pistorm_dmacon_shadow = 0;
        pistorm_adkcon_shadow = 0;
        pistorm_last_vpos = 0;
        memset(pistorm_cia_regs, 0, sizeof(pistorm_cia_regs));
        memset(pistorm_cia_timers, 0, sizeof(pistorm_cia_timers));
        memset(pistorm_cia_icr_mask, 0, sizeof(pistorm_cia_icr_mask));
        memset(pistorm_cia_icr_pending, 0, sizeof(pistorm_cia_icr_pending));
        pistorm_pc_trace_seen = 0;
        pistorm_store_special_word(0x00dff01c, 0);
        pistorm_store_special_word(0x00dff01e, 0);
        pistorm_store_special_word(0x00dff002, 0);
        pistorm_store_special_word(0x00dff010, 0);
        board_idx = 0;
        pistorm_force_overlay_state(pistorm_rom_exec_entry_boot ? 0u : 1u);
        if (pistorm_mmio_trace || pistorm_step_trace)
            kprintf("[BOOT] ARM32 PiStorm forcing OVL %s before ROM JIT entry\n",
                overlay ? "on" : "off");
        if (pistorm_fake_lowram_mb && pistorm_fake_lowram_virt)
        {
            if (pistorm_fast_rom_qemu_boot)
            {
                if (pistorm_fast_rom_preexecbase_boot)
                {
                    *(volatile uint32_t *)pistorm_fake_lowram_virt = BE32(0x4c4f574du);
                    *(volatile uint32_t *)(pistorm_fake_lowram_virt + 4u) = 0;
                }
                else
                {
                    *(volatile uint32_t *)pistorm_fake_lowram_virt = BE32(pistorm_fast_rom_low0);
                    *(volatile uint32_t *)(pistorm_fake_lowram_virt + 4u) = BE32(pistorm_fast_rom_a6);
                    pistorm_fast_rom_seed_slowram_node();
                }
            }
            else
            {
                *(volatile uint32_t *)pistorm_fake_lowram_virt = BE32(0x4c4f574d);
                if (pistorm_rom_exec_entry_boot)
                {
                    /*
                     * Generic ROM exec-entry images still expect the early
                     * low-memory bootstrap cell at 0x00000004 to point at the
                     * current ROM workspace base before they rebuild their
                     * own lists. The fast ROM path already seeded this slot;
                     * do the same for the plain exec-entry path so it does
                     * not immediately zero A6 via "move.l $4.w,d1 / movea.l
                     * d1,a6" when fake low RAM is active.
                     */
                    *(volatile uint32_t *)(pistorm_fake_lowram_virt + 4u) = m68k->A[6].u32;
                }
            }
            *(volatile uint32_t *)(pistorm_fake_lowram_virt + 0x10000u) = BE32(0x4c4f574d);
            if (pistorm_mmio_trace || pistorm_step_trace)
                kprintf("[BOOT] ARM32 PiStorm fake low RAM pre-JIT low0=%08x desc=%08x saved=%08x overlay=%u\n",
                    BE32(*(volatile uint32_t *)pistorm_fake_lowram_virt),
                    mmu_table[0],
                    pistorm_page0_saved_desc,
                    overlay);
        }
        pistorm_update_irq_state();
    }
    else
#endif
    if (i > 0) {
        m68k->D[0].u32 = BE32((uint32_t)pitch);
        m68k->D[1].u32 = BE32((uint32_t)fb_width);
        m68k->D[2].u32 = BE32((uint32_t)fb_height);
        m68k->A[0].u32 = BE32((uint32_t)framebuffer);
        m68k->A[6].u32 = BE32((uint32_t)(uintptr_t)fdt);
        m68k->ISP.u32 = BE32((uint32_t)top_of_ram);
        m68k->A[7].u32 = m68k->ISP.u32;
        m68k->PC = BE32((uint32_t)addr);
        m68k->SR = BE16(SR_S | SR_IPL);
        m68k->FPCR = 0;
    }

    of_node_t *node = dt_find_node("/chosen");
    if (node)
    {
        of_property_t *prop = dt_find_property(node, "bootargs");
        if (prop && prop->op_value)
        {
            const char *tok;
#ifdef PISTORM
            pistorm_swap_df0_with_dfx = 0;
            pistorm_spoof_df0_id = 0;
            pistorm_move_slow_to_chip = 0;
            pistorm_block_c0 = 0;
            pistorm_zorro_disable = 0;
            pistorm_mmio_trace = 0;
            pistorm_step_trace = 0;
            pistorm_tracepc_start = 0;
            pistorm_tracepc_end = 0;
            pistorm_tracepc_last_pc = 0xffffffffu;
            pistorm_tracepc_last_a0 = 0xffffffffu;
            pistorm_tracepc_last_d1 = 0xffffffffu;
            pistorm_tracepc_last_sr = 0xffffu;
            pistorm_recalc_checksum = 0;
            pistorm_allow_fast_rom_qemu_boot = 0;
            pistorm_allow_fast_diagrom_qemu_boot = 0;
            pistorm_diagrom_menu_handoff_ready = 0;
            pistorm_diagrom_menu_mode = 0;
            pistorm_diagrom_menu_reported = 0;
            pistorm_step_trace_last_pc = 0xffffffffu;
            pistorm_step_trace_last_a0_page = 0xffffffffu;
            pistorm_step_trace_last_a3_page = 0xffffffffu;
            pistorm_step_trace_last_low0 = 0xffffffffu;
            pistorm_step_trace_last_intena = 0xffffu;
            pistorm_step_trace_last_intreq = 0xffffu;
            pistorm_step_trace_last_ipl = 0xffu;
            memset(pistorm_mmio_read_seen, 0, sizeof(pistorm_mmio_read_seen));
            memset(pistorm_mmio_write_seen, 0, sizeof(pistorm_mmio_write_seen));
            memset(pistorm_cia_read_seen, 0, sizeof(pistorm_cia_read_seen));
            memset(pistorm_cia_write_seen, 0, sizeof(pistorm_cia_write_seen));
            memset(pistorm_low_read_seen, 0, sizeof(pistorm_low_read_seen));
            memset(pistorm_low_write_seen, 0, sizeof(pistorm_low_write_seen));
            memset(pistorm_low_page_read_seen, 0, sizeof(pistorm_low_page_read_seen));
            memset(pistorm_low_page_write_seen, 0, sizeof(pistorm_low_page_write_seen));
            memset(pistorm_cia_regs, 0, sizeof(pistorm_cia_regs));
            memset(pistorm_cia_icr_mask, 0, sizeof(pistorm_cia_icr_mask));
            memset(pistorm_cia_icr_pending, 0, sizeof(pistorm_cia_icr_pending));
            pistorm_pc_trace_seen = 0;
            pistorm_slot_trace_seen = 0;
            pistorm_fake_lowram_mb = pistorm_boot_fake_lowram_mb;
            if (pistorm_fake_lowram_mb == 0)
            {
                pistorm_fake_lowram_virt = 0;
                pistorm_fake_lowram_phys = 0;
            }

            if (strstr(prop->op_value, "swap_df0_with_df1"))
                pistorm_swap_df0_with_dfx = 1;
            if (strstr(prop->op_value, "swap_df0_with_df2"))
                pistorm_swap_df0_with_dfx = 2;
            if (strstr(prop->op_value, "swap_df0_with_df3"))
                pistorm_swap_df0_with_dfx = 3;
            if (strstr(prop->op_value, "move_slow_to_chip"))
                pistorm_move_slow_to_chip = 1;
            if (strstr(prop->op_value, "block_c0"))
                pistorm_block_c0 = 1;
            if (strstr(prop->op_value, "z3_disable"))
                pistorm_zorro_disable = 1;
            if (strstr(prop->op_value, "mmio_trace"))
                pistorm_mmio_trace = 1;
            if (strstr(prop->op_value, "step_trace"))
                pistorm_step_trace = 1;
            if (strstr(prop->op_value, "fast_rom") ||
                strstr(prop->op_value, "fast_headerized"))
                pistorm_allow_fast_rom_qemu_boot = 1;
            if (strstr(prop->op_value, "fast_diagrom"))
                pistorm_allow_fast_diagrom_qemu_boot = 1;
            if ((tok = find_token(prop->op_value, "tracepc=")))
            {
                uint32_t start = 0;
                uint32_t end = 0;
                const char *c = &tok[8];

                if (pistorm_parse_hex32(c, &start))
                {
                    while ((*c >= '0' && *c <= '9') ||
                           (*c >= 'a' && *c <= 'f') ||
                           (*c >= 'A' && *c <= 'F') ||
                           *c == 'x' || *c == 'X')
                    {
                        c++;
                    }

                    if (*c == '-')
                    {
                        c++;
                        if (!pistorm_parse_hex32(c, &end))
                            end = start + 1;
                    }
                    else
                    {
                        end = start + 1;
                    }

                    if (end <= start)
                        end = start + 1;

                    pistorm_tracepc_start = start;
                    pistorm_tracepc_end = end;
                }
            }
            if (strstr(prop->op_value, "checksum_rom"))
                pistorm_recalc_checksum = 1;
            if (pistorm_mmio_trace)
                kprintf("[BOOT] ARM32 PiStorm MMIO trace enabled\n");
            if (pistorm_step_trace)
                kprintf("[BOOT] ARM32 PiStorm step trace enabled\n");
            if (pistorm_tracepc_end > pistorm_tracepc_start)
                kprintf("[BOOT] ARM32 PiStorm tracepc=%08x-%08x\n", pistorm_tracepc_start, pistorm_tracepc_end);
            if (pistorm_recalc_checksum)
                kprintf("[BOOT] ARM32 PiStorm checksum_rom enabled\n");
            if (pistorm_allow_fast_rom_qemu_boot)
                kprintf("[BOOT] ARM32 PiStorm fast ROM bootstrap enabled\n");
            if (pistorm_allow_fast_diagrom_qemu_boot)
                kprintf("[BOOT] ARM32 PiStorm fast DiagROM bootstrap enabled\n");
            if (pistorm_move_slow_to_chip)
                kprintf("[BOOT] ARM32 PiStorm move_slow_to_chip enabled\n");
            if (pistorm_block_c0)
                kprintf("[BOOT] ARM32 PiStorm block_c0 enabled\n");
            if (pistorm_zorro_disable)
                kprintf("[BOOT] ARM32 PiStorm z3_disable enabled\n");
            if ((tok = find_token(prop->op_value, "fake_lowram=")))
            {
                uint32_t val = 0;
                const char *c = &tok[12];

                for (int i = 0; i < 2; i++)
                {
                    if (c[i] < '0' || c[i] > '9')
                        break;

                    val = val * 10 + c[i] - '0';
                }

                if (val > 8)
                    val = 8;

                pistorm_fake_lowram_mb = val;
            }
            if (pistorm_fake_lowram_mb)
                kprintf("[BOOT] ARM32 PiStorm fake low RAM requested: %d MB\n", pistorm_fake_lowram_mb);
#endif
            if (strstr(prop->op_value, "enable_cache"))
                m68k->CACR = BE32(0x80008000);
            if (strstr(prop->op_value, "debug"))
                debug = 1;
            if (strstr(prop->op_value, "disassemble"))
                disasm = 1;

            if ((tok = find_token(prop->op_value, "ICNT=")))
            {
                uint32_t val = 0;
                const char *c = &tok[5];

                for (int i = 0; i < 4; i++)
                {
                    if (c[i] < '0' || c[i] > '9')
                        break;

                    val = val * 10 + c[i] - '0';
                }

                if (val == 0)
                    val = 1;
                if (val > 256)
                    val = 256;

                emu68_icnt = val;
            }

            if ((tok = find_token(prop->op_value, "IRNG=")))
            {
                uint32_t val = 0;
                const char *c = &tok[5];

                for (int i = 0; i < 7; i++)
                {
                    if (c[i] < '0' || c[i] > '9')
                        break;

                    val = val * 10 + c[i] - '0';
                }

                if (val > 65535)
                    val = 65535;

                emu68_irng = val;
            }

            if ((tok = find_token(prop->op_value, "LCNT=")))
            {
                uint32_t val = 0;
                const char *c = &tok[5];

                for (int i = 0; i < 2; i++)
                {
                    if (c[i] < '0' || c[i] > '9')
                        break;

                    val = val * 10 + c[i] - '0';
                }

                if (val == 0)
                    val = 1;
                if (val > (JCCB_LOOP_COUNT_MASK + 1))
                    val = JCCB_LOOP_COUNT_MASK + 1;

                emu68_lcnt = val;
            }
        }
    }

    __m68k.JIT_CONTROL &= ~((JCCB_INSN_DEPTH_MASK << JCCB_INSN_DEPTH) |
                            (JCCB_INLINE_RANGE_MASK << JCCB_INLINE_RANGE) |
                            (JCCB_LOOP_COUNT_MASK << JCCB_LOOP_COUNT));
    __m68k.JIT_CONTROL |= (emu68_icnt & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
    __m68k.JIT_CONTROL |= (emu68_irng & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
    __m68k.JIT_CONTROL |= (emu68_lcnt & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;
#ifdef PISTORM
    pistorm_saved_jit_control = __m68k.JIT_CONTROL;
    pistorm_bootstrap_jit_clamped = 0;

    if (addr == NULL &&
        (pistorm_fast_rom_qemu_boot ||
         (pistorm_diagrom_boot && pistorm_allow_fast_diagrom_qemu_boot)))
    {
        __m68k.JIT_CONTROL &= ~((JCCB_INSN_DEPTH_MASK << JCCB_INSN_DEPTH) |
                                (JCCB_INLINE_RANGE_MASK << JCCB_INLINE_RANGE) |
                                (JCCB_LOOP_COUNT_MASK << JCCB_LOOP_COUNT));
        __m68k.JIT_CONTROL |= (1u & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
        __m68k.JIT_CONTROL |= (1u & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
        __m68k.JIT_CONTROL |= (1u & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;
        pistorm_bootstrap_jit_clamped = 1;
    }
#endif

#ifdef PISTORM
    /*
     * The ROM loader initializes fake low RAM before entering start_emu(),
     * but synthetic HUNK runs can request the same ARM32 PiStorm bootargs
     * directly from the outer helper. Mirror that setup here so low-memory
     * diagnostics and repro payloads exercise the same page-zero backing as
     * the real ROM path.
     */
    if (addr != NULL && pistorm_fake_lowram_mb && pistorm_fake_lowram_virt == 0)
        pistorm_init_fake_lowram();

    /*
     * HUNK payloads are entered like normal executables, not like Kickstart
     * reset. Keep page 0 backed by RAM for those runs so low-memory probes
     * and synthetic repro payloads read back the words they just wrote,
     * instead of seeing the PiStorm ROM shadow while OVL is still set.
     */
    if (addr != NULL)
    {
        pistorm_force_overlay_state(0);
    }
#endif

    print_context(m68k);

    m68k->A[7].u32 = BE32(BE32(m68k->A[7].u32) - 4);
    m68k->ISP.u32 = m68k->A[7].u32;

    *(uint32_t*)(BE32(m68k->A[7].u32)) = 0;

    printf("[JIT] Let it go...\n");
    uint64_t ctx_count = 0;
#ifdef PISTORM
    uint64_t pistorm_next_progress_trace = 0;
#endif

#ifdef PISTORM
    if (pistorm_step_trace)
        pistorm_trace_puts("[PISTORM:LOOP] before-t1\n");
#endif
    t1 = LE32(*(volatile uint32_t*)0xf2003004) | (uint64_t)LE32(*(volatile uint32_t *)0xf2003008) << 32;
#ifdef PISTORM
    if (pistorm_step_trace)
        pistorm_trace_puts("[PISTORM:LOOP] after-t1\n");
    if (pistorm_mmio_trace)
        pistorm_next_progress_trace = pistorm_read_cntpct() + pistorm_read_cntfrq();
#endif

    do {
#ifdef PISTORM
        if (addr == NULL)
        {
            pistorm_service_pending_interrupt(m68k);

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                pistorm_diagrom_menu_handoff_ready &&
                BE32(m68k->PC) == 0x00f82320u)
            {
                /*
                 * Once the early startup diagnostics have already run once,
                 * DiagROM re-enters the same pre-menu setup block at
                 * 0x00f82320 and spends most of its time replaying that
                 * screen/checksum stage under QEMU. Hand off to DiagROM's own
                 * later path:
                 *
                 *   jsr ClearBuffer
                 *   bsr DefaultVars
                 *   bra MainMenu
                 *
                 * starting at 0x00f82576 so the ROM can continue into its
                 * real menu loop instead of repeating startup.
                 */
                pistorm_diagrom_menu_handoff_ready = 0;
                pistorm_diagrom_menu_mode = 1;
                m68k->PC = BE32(0x00f82576u);
                m68k->JIT_CONTROL &= ~((JCCB_INSN_DEPTH_MASK << JCCB_INSN_DEPTH) |
                                       (JCCB_INLINE_RANGE_MASK << JCCB_INLINE_RANGE) |
                                       (JCCB_LOOP_COUNT_MASK << JCCB_LOOP_COUNT));
                m68k->JIT_CONTROL |= (1u & JCCB_INSN_DEPTH_MASK) << JCCB_INSN_DEPTH;
                m68k->JIT_CONTROL |= (1u & JCCB_INLINE_RANGE_MASK) << JCCB_INLINE_RANGE;
                m68k->JIT_CONTROL |= (1u & JCCB_LOOP_COUNT_MASK) << JCCB_LOOP_COUNT;
                pistorm_bootstrap_jit_clamped = 1;
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f80252u)
            {
                /*
                 * DiagROM spends a long time in a QEMU-only logic-analyzer /
                 * address-line write pattern before it reaches the first
                 * DumpSerial startup text. The earlier setup is already done
                 * by this point, so skip just this synthetic probe block and
                 * resume at the normal pre-DumpSerial entry.
                 */
                m68k->A[4].u32 = 0;
                m68k->D[0].u32 = 0;
                m68k->PC = BE32(0x00f803f2u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f80386u)
            {
                /*
                 * DiagROM's next startup block probes wrapped high addresses
                 * like $40000000/$80000000, samples the mouse-button inputs,
                 * prints a few startup strings via DumpSerial, then continues
                 * at 0x00f80440. On the current ARM32 QEMU path this is where
                 * execution still disappears into one long translated unit.
                 *
                 * Recreate the resulting button bitmap in A4, perform the one
                 * following CIA side effect ($bfe101 := $ff), clear D0 like
                 * the real code does before the first DumpSerial, and resume
                 * at DiagROM's own continuation after those startup prints.
                 */
                uint32_t buttons = 0;

                if ((pistorm_handle_special_read(0x00bfe001u, 1) & (1u << 6)) == 0)
                    buttons |= 1u << 0;
                if ((pistorm_handle_special_read(0x00bfe001u, 1) & (1u << 7)) == 0)
                    buttons |= 1u << 1;
                if ((pistorm_handle_special_read(0x00dff016u, 1) & (1u << 2)) == 0)
                    buttons |= 1u << 2;
                if ((pistorm_handle_special_read(0x00dff016u, 1) & (1u << 6)) == 0)
                    buttons |= 1u << 3;
                if ((pistorm_handle_special_read(0x00dff016u, 1) & (1u << 0)) == 0)
                    buttons |= 1u << 4;
                if ((pistorm_handle_special_read(0x00dff016u, 1) & (1u << 4)) == 0)
                    buttons |= 1u << 5;

                m68k->A[4].u32 = BE32(buttons);
                m68k->D[0].u32 = 0;
                pistorm_handle_special_write(0x00bfe101u, 0xffu, 1);
                m68k->PC = BE32(0x00f80440u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f80b3cu)
            {
                /*
                 * DiagROM's chipmem detector prints one line per 64K block
                 * and spends most of its time in that synthetic scan under
                 * QEMU. Seed the same 2 MB result that the current ARM32
                 * fake-low-RAM setup exposes, then resume at DiagROM's own
                 * ".finished" path so the ROM still prints the summary and
                 * continues into its address-check logic.
                 */
                m68k->D[0].u32 = BE32(0x00000020u);
                m68k->D[3].u32 = BE32(0x00000400u);
                m68k->A[6].u32 = BE32(0x00200400u);
                m68k->PC = BE32(0x00f80eccu);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f80f46u)
            {
                /*
                 * After chipmem sizing, DiagROM spends another long time
                 * filling and rereading the detected range with address
                 * patterns. On the current ARM32 QEMU path this adds a large
                 * delay but no new bring-up signal. Mark the address check as
                 * successful and resume at DiagROM's own CHIPOK / post-check
                 * path.
                 */
                m68k->A[4].u32 = 0;
                m68k->PC = BE32(0x00f8117eu);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81460u)
            {
                /*
                 * Under the current ARM32 QEMU path, DiagROM's startup-action
                 * summary still arrives here with transient mouse-button bits
                 * set, which forces the ROM into the optional fastmem probe
                 * path. Keep the already-validated button-status printout, but
                 * clear the action-selection state so the ROM follows its
                 * normal "NONE" branch and continues bring-up.
                 */
                m68k->D[4].u32 = 0;
                m68k->D[6].u32 = 0;
                m68k->A[4].u32 = 0;
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f83ee2u &&
                BE32(m68k->A[1].u32) == 0x00f818f2u)
            {
                /*
                 * DiagROM hands off from memory detection to the next stage
                 * via DumpSerial at 0x00f83ee2 with A1=0x00f818f2. Under
                 * QEMU this status string dominates runtime, so skip just
                 * this call site and mark NoSerial for the later setup text.
                 */
                m68k->A[4].u32 = BE32(BE32(m68k->A[4].u32) | (1u << 7));
                m68k->PC = BE32(0x00f818f2u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f818f2u)
            {
                /*
                 * The outer ARM32 loop reliably returns at 0x00f818f2 after
                 * the pre-menu memory-detection text. DiagROM still executes
                 * a small register-normalization block here before it prints
                 * one more status line via DumpSerial:
                 *
                 *   movea.l a7,a5
                 *   swap/clr.w/swap d0
                 *   move.l  d3,d1
                 *
                 * Resume directly at the real post-handoff code, but emulate
                 * that register setup first so the later work-area builder
                 * sees the same chip-start backup and chip-block count that
                 * the unskipped path would have produced.
                 */
                m68k->A[5].u32 = m68k->A[7].u32;
                m68k->D[0].u32 = BE32(BE32(m68k->D[0].u32) & 0x0000ffffu);
                m68k->D[1].u32 = m68k->D[3].u32;
                m68k->A[4].u32 = BE32(BE32(m68k->A[4].u32) | (1u << 7));
                m68k->PC = BE32(0x00f8191eu);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                (BE32(m68k->PC) == 0x00f81a46u ||
                 BE32(m68k->PC) == 0x00f81a58u ||
                 BE32(m68k->PC) == 0x00f81b32u ||
                 BE32(m68k->PC) == 0x00f81b68u))
            {
                /*
                 * After memory sizing, DiagROM fills the work area with its
                 * own addresses, rereads the whole range, then clears it
                 * twice before it branches into the real code path. Under the
                 * current ARM32 QEMU setup this verified self-test is pure
                 * startup latency, so seed the successful post-condition and
                 * resume at DiagROM's own branch to "code".
                 */
                uintptr_t work_base = (uintptr_t)BE32(m68k->A[6].u32);

                if (work_base != 0)
                    bzero((void *)work_base, 0x0001138au);

                m68k->D[1].u32 = m68k->D[6].u32;
                m68k->A[0].u32 = m68k->A[5].u32;
                m68k->PC = BE32(0x00f81b70u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81f90u)
            {
                /*
                 * The ARM32 legacy translator still gets stuck on this odd
                 * d16(A6) longword store in DiagROM's post-startup input
                 * path:
                 *
                 *   move.l d7,$22e(a6)
                 *
                 * Emulate just this store and resume at the next instruction
                 * so we can keep validating the later DiagROM/menu flow under
                 * QEMU while the generic translator path is narrowed
                 * separately.
                 */
                uint32_t far = BE32(m68k->A[6].u32) + 0x022eu;
                uint32_t value = BE32(m68k->D[7].u32);

                pistorm_raw_write(far, value, 4);
                m68k->PC = BE32(0x00f81f94u);
                pistorm_restore_runtime_jit_control(m68k, 0x00f81f94u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81f94u)
            {
                /*
                 * Past the startup summary / startup-action selection,
                 * DiagROM is already drawing into the framebuffer and the
                 * remaining latency under ARM32 QEMU is dominated by serial
                 * character output. DiagROM already has an internal NoSerial
                 * flag for this mode, so arm it here instead of trying to
                 * skip every later rs232_out() call site individually.
                 */
                uint32_t a6 = BE32(m68k->A[6].u32);

                pistorm_diagrom_menu_handoff_ready = 0;
                pistorm_diagrom_menu_mode = 1;
                if ((pistorm_handle_special_read(a6 + 0x023au, 1) & 0xffu) == 0)
                {
                    pistorm_raw_write(a6 + 0x023au, 0x01u, 1);
                    if (pistorm_mmio_trace || pistorm_step_trace || pistorm_tracepc_end > pistorm_tracepc_start)
                        kprintf("[BOOT] ARM32 PiStorm fast DiagROM NoSerial enabled at PC=%08x\n", 0x00f81f94u);
                }
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                pistorm_bootstrap_jit_clamped &&
                BE32(m68k->PC) == 0x00f83082u)
            {
                /*
                 * If the late post-startup builder is reached without first
                 * tripping the earlier store workaround above, the fragile
                 * bootstrap phase is still over. Restore the caller-selected
                 * JIT sizing before DiagROM starts its larger menu/setup
                 * builders, otherwise the ARM32 QEMU path degenerates into
                 * endless 1-insn units and cache churn.
                 */
                uint32_t a6 = BE32(m68k->A[6].u32);

                pistorm_diagrom_menu_mode = 1;
                if ((pistorm_handle_special_read(a6 + 0x023au, 1) & 0xffu) == 0)
                {
                    pistorm_raw_write(a6 + 0x023au, 0x01u, 1);
                    if (pistorm_mmio_trace || pistorm_step_trace || pistorm_tracepc_end > pistorm_tracepc_start)
                        kprintf("[BOOT] ARM32 PiStorm fast DiagROM NoSerial enabled at PC=%08x\n", 0x00f83082u);
                }
                pistorm_restore_runtime_jit_control(m68k, 0x00f83082u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                (BE32(m68k->PC) == 0x00f80440u ||
                 BE32(m68k->PC) == 0x00f8044cu ||
                 BE32(m68k->PC) == 0x00f804d2u))
            {
                /*
                 * After the initial startup text, DiagROM performs a large
                 * ROM address-access sweep from 0x00fa7376 to 0x00ffffe6.
                 * On the current ARM32 QEMU path that loop succeeds but
                 * consumes most of the runtime before any later bring-up
                 * signal appears. Resume at DiagROM's own post-scan branch
                 * so the normal success / failure logic still runs.
                 */
                m68k->A[4].u32 = BE32(BE32(m68k->A[4].u32) & ~(1u << 10));
                m68k->PC = BE32(0x00f80502u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                (BE32(m68k->PC) == 0x00f82620u ||
                 BE32(m68k->PC) == 0x00f8262au))
            {
                /*
                 * DiagROM's RomChecksum routine walks eight 64K checksum
                 * windows after the early chipmem/startup summary:
                 *
                 *   move.l  #$3fff, d7
                 * .loop:
                 *   ...
                 *   add.l   (a5)+, d0
                 *   dbra    d7, .loop
                 *
                 * On the current ARM32 QEMU path this dominates the runtime
                 * after bring-up has already proven ROM execution, OVL,
                 * chipmem sizing, and early CIA/custom access. Skip just the
                 * checksum body in fast DiagROM mode and return to the normal
                 * caller so later menu/setup flow can continue.
                 */
                m68k->PC = BE32(0x00f8268cu);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f82728u)
            {
                pistorm_diagrom_menu_handoff_ready = 1;
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                pistorm_diagrom_menu_mode &&
                !pistorm_diagrom_menu_reported)
            {
                uint32_t pc = BE32(m68k->PC);

                if (pc == 0x00f82eceu ||
                    pc == 0x00f834f8u ||
                    pc == 0x00f83b54u)
                {
                    pistorm_diagrom_menu_reported = 1;
                    kprintf("[BOOT] ARM32 PiStorm DiagROM menu active at PC=%08x\n", pc);
                }
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                pistorm_diagrom_menu_mode &&
                BE32(m68k->PC) == 0x00f83214u)
            {
                /*
                 * After DiagROM has handed off into its screen/menu path,
                 * the remaining startup time under ARM32 QEMU is dominated by
                 * rs232_out(), which sends every menu/status character to the
                 * serial port even though the same text is already being drawn
                 * into the framebuffer. Skip only the serial character write
                 * in fast DiagROM menu mode so the screen path can complete.
                 */
                uint32_t sp = BE32(m68k->A[7].u32);
                uint32_t ret = BE32(*(volatile uint32_t *)(uintptr_t)sp);

                m68k->A[7].u32 = BE32(sp + 4u);
                m68k->PC = BE32(ret);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f82eceu)
            {
                /*
                 * Once DiagROM has reached the post-startup menu loop, the
                 * ARM32 QEMU path spends most of its time in GetKey's keyboard
                 * handshake waits even when no key is pressed. Model the
                 * common idle case directly so GetCharKey sees a clean "no
                 * key" result and the menu loop can keep running.
                 */
                uint32_t a6 = BE32(m68k->A[6].u32);
                uint32_t sp = BE32(m68k->A[7].u32);
                uint32_t ret = BE32(*(volatile uint32_t *)(uintptr_t)sp);

                pistorm_raw_write(a6 + 0x0094u, 0x00u, 1); /* scancode */
                pistorm_raw_write(a6 + 0x0095u, 0x00u, 1); /* key */
                pistorm_raw_write(a6 + 0x009au, 0x00u, 1); /* keyup */
                pistorm_raw_write(a6 + 0x009bu, 0x00u, 1); /* keydown */
                pistorm_raw_write(a6 + 0x009du, 0x00u, 1); /* keynew */
                m68k->D[0].u32 = 0;
                m68k->A[7].u32 = BE32(sp + 4u);
                m68k->PC = BE32(ret);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f89e9cu)
            {
                /*
                 * After the menu handoff, DiagROM starts using its WaitShort
                 * helper. Under ARM32 QEMU the timing side of this routine is
                 * pure delay, so skip the helper entirely before it saves any
                 * registers.
                 */
                m68k->PC = BE32(0x00f89ec8u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                (BE32(m68k->PC) == 0x00f89ea0u ||
                 BE32(m68k->PC) == 0x00f89eaau ||
                 BE32(m68k->PC) == 0x00f89eb4u ||
                 BE32(m68k->PC) == 0x00f89ebau ||
                 BE32(m68k->PC) == 0x00f89ec0u ||
                 BE32(m68k->PC) == 0x00f89ecau ||
                 BE32(m68k->PC) == 0x00f89ed8u))
            {
                /*
                 * Same WaitShort helper after its prologue has already run:
                 *
                 *   move.b $dff006,d0
                 *   add.b  #10,d0
                 * .loop:
                 *   cmp.b  $dff006,d0
                 *   bne    .loop
                 *
                 * The ARM32 QEMU custom-beam model is good enough for the
                 * earlier diagnostics, but this tight wait loop still eats
                 * the remaining runtime. Skip just the wait body in fast
                 * DiagROM mode and return through DiagROM's own epilogue.
                 */
                m68k->PC = BE32(0x00f89ee0u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                BE32(m68k->PC) == 0x00f89ee6u)
            {
                /*
                 * Same for WaitLong before it saves regs.
                 */
                m68k->PC = BE32(0x00f89f38u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                (BE32(m68k->PC) == 0x00f89eeau ||
                 BE32(m68k->PC) == 0x00f89ef4u ||
                 BE32(m68k->PC) == 0x00f89ef8u ||
                 BE32(m68k->PC) == 0x00f89efcu ||
                 BE32(m68k->PC) == 0x00f89f02u ||
                 BE32(m68k->PC) == 0x00f89f0eu ||
                 BE32(m68k->PC) == 0x00f89f1cu ||
                 BE32(m68k->PC) == 0x00f89f2au))
            {
                /*
                 * WaitLong after the prologue has already run.
                 */
                m68k->PC = BE32(0x00f89f34u);
            }

            if (pistorm_diagrom_boot &&
                pistorm_allow_fast_diagrom_qemu_boot &&
                (BE32(m68k->PC) == 0x00f82790u ||
                 BE32(m68k->PC) == 0x00f827a2u))
            {
                /*
                 * DiagROM's screen clear path sweeps three bitplanes with:
                 *
                 *   clr.l (a0)+
                 *   clr.l (a1)+
                 *   clr.l (a2)+
                 *   dbra  d0, .loop
                 *
                 * Under the current ARM32 QEMU path this is where startup
                 * still collapses. Resume at DiagROM's own post-clear tail
                 * so the rest of the status/menu setup continues normally.
                 */
                m68k->PC = BE32(0x00f827acu);
            }

            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81eaau)
            {
                uint32_t sp = BE32(m68k->A[7].u32);
                uint32_t ret = pistorm_fast_rom_read32(sp);
                uint32_t header = BE32(m68k->A[0].u32);
                uint32_t size = BE32(m68k->D[0].u32);

                if (header != 0 && size != 0)
                {
                    kprintf("[PISTORM:FASTALLOC] pc=%08x ret=%08x header=%08x size=%08x\n",
                        BE32(m68k->PC), ret, header, size);
                    uint32_t result = pistorm_fast_rom_alloc_from_header(header, size);
                    kprintf("[PISTORM:FASTALLOC] result=%08x\n", result);

                    m68k->D[0].u32 = BE32(result);
                    m68k->A[7].u32 = BE32(sp + 4u);
                    m68k->PC = BE32(ret);
                }
            }

            if (pistorm_fast_rom_qemu_boot &&
                pistorm_fast_rom_a6 == 0 &&
                BE32(m68k->PC) >= 0x00f80352u &&
                BE32(m68k->PC) < 0x00f80384u)
            {
                uint32_t execbase = BE32(m68k->A[6].u32);

                if (execbase != 0)
                {
                    pistorm_fast_rom_a6 = execbase;
                    if (execbase >= 0x38cu)
                        pistorm_fast_rom_low0 = execbase - 0x38cu;
                }
            }

            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81fe0u)
            {
                pistorm_fast_rom_finalize_slowram_node();
                (void)pistorm_fast_rom_finish_allocclear(m68k);
            }

            if (pistorm_fast_rom_qemu_boot &&
                pistorm_fast_rom_residents != 0 &&
                BE32(m68k->PC) == 0x00f803dau)
            {
                uint32_t execbase = BE32(m68k->A[6].u32);

                m68k->A[0].u32 = BE32(0x00f803f8u);
                if (execbase != 0)
                    *(volatile uint32_t *)(uintptr_t)(execbase + 0x12cu) = BE32(pistorm_fast_rom_residents);
                *(volatile uint32_t *)(uintptr_t)(pistorm_fast_rom_a6 + 0x12cu) = BE32(pistorm_fast_rom_residents);
                m68k->PC = BE32(0x00f803e6u);
            }

            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81fe0u)
            {
                pistorm_fast_rom_finalize_slowram_node();
                (void)pistorm_fast_rom_finish_allocclear(m68k);
            }

            if (pistorm_fast_rom_qemu_boot &&
                pistorm_fast_rom_residents != 0 &&
                BE32(m68k->PC) == 0x00f81108u)
            {
                m68k->A[2].u32 = BE32(pistorm_fast_rom_residents);
                m68k->PC = BE32(0x00f8110cu);
            }

            if (pistorm_fast_rom_qemu_boot &&
                pistorm_zorro_disable &&
                BE32(m68k->PC) == 0x00f846bau)
            {
                uint32_t a2 = BE32(m68k->A[2].u32);
                uint32_t a4 = BE32(m68k->A[4].u32);

                /*
                 * In the fast QEMU ROM path with Zorro boards disabled there
                 * is no autoconfig device behind 0x00e80000. The wrapped ROM
                 * enumerator at 0x00f846ba keeps asking Exec for probe
                 * templates and then validating the same empty bus window,
                 * which never advances the boot. Match the "no more configs"
                 * return instead of walking the template loop pointlessly.
                 */
                if (a2 == 0x00e80000u && a4 == 0x00e80000u)
                    m68k->PC = BE32(0x00f8470au);
            }

            /*
             * ARM32 currently misloads RomTag rt_Flags / rt_Init fields in the
             * fast ROM resident walk:
             *
             *   00f8113c  btst.b  #$7, $a(a1)
             *   00f81126  move.b  $a(a1), d0
             *
             * Under QEMU that sends plain residents like 0x00f80042 and
             * 0x00f8413c down the auto-init MakeLibrary path even though their
             * rt_Flags byte is 0x02, then later falls into the red-screen
             * halt. Patch only this fast-QEMU path and only at the specific
             * PCs so the boot can continue while the underlying translator
             * bug is investigated separately.
             */
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f8113cu)
            {
                uint32_t a1 = BE32(m68k->A[1].u32);
                if (a1 == 0x00f80042u || a1 == 0x00f8413cu)
                {
                    uint32_t flags = pistorm_raw_read(a1 + 0x0au, 1) & 0xffu;

                    if (flags & 0x80u)
                    {
                        m68k->PC = BE32(0x00f8114eu);
                    }
                    else
                    {
                        uint32_t init = pistorm_raw_read(a1 + 0x16u, 4);

                        m68k->A[1].u32 = BE32(init);
                        m68k->D[0].u32 = 0;
                        m68k->A[0].u32 = m68k->D[1].u32;
                        m68k->PC = BE32(init);
                    }
                }
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81126u)
            {
                uint32_t a1 = BE32(m68k->A[1].u32);
                if (a1 == 0x00f80042u || a1 == 0x00f8413cu)
                {
                    uint32_t d0 = BE32(m68k->D[0].u32);

                    d0 = (d0 & ~0xffu) | (pistorm_raw_read(a1 + 0x0au, 1) & 0xffu);
                    m68k->D[0].u32 = BE32(d0);
                    m68k->PC = BE32(0x00f8112au);
                }
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f80f22u)
            {
                uint32_t sp = BE32(m68k->A[7].u32);
                uint32_t ret = BE32(*(volatile uint32_t *)(uintptr_t)sp);
                uint32_t table = BE32(m68k->A[0].u32);
                uint32_t list = pistorm_build_resident_list_from_table(table, NULL);

                m68k->D[0].u32 = BE32(list);
                m68k->A[7].u32 = BE32(sp + 4u);
                m68k->PC = BE32(ret);
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81b1eu)
            {
                uint32_t sp = BE32(m68k->A[7].u32);
                uint32_t ret = BE32(*(volatile uint32_t *)(uintptr_t)sp);

                if (ret == 0x00f80fa6u)
                {
                    m68k->D[0].u32 = 0;
                    m68k->A[7].u32 = BE32(sp + 4u);
                    m68k->PC = BE32(ret);
                }
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f8102cu)
            {
                uint32_t execbase = BE32(m68k->A[6].u32);
                uint32_t sp = BE32(m68k->A[7].u32);
                uint32_t ret = BE32(*(volatile uint32_t *)(uintptr_t)sp);

                if (execbase >= 0xff000000u && execbase < 0xffffffffu - 0x22au)
                    *(volatile uint32_t *)(uintptr_t)(execbase + 0x226u) = 0;

                m68k->A[7].u32 = BE32(sp + 4u);
                m68k->PC = BE32(ret);
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81d60u)
            {
                (void)pistorm_fast_makefunctions(m68k);
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81202u)
            {
                (void)pistorm_fast_rom_expand_stream(m68k);
            }
            if (pistorm_fast_rom_qemu_boot &&
                pistorm_bootstrap_jit_clamped &&
                (BE32(m68k->PC) == 0x00f81d4eu ||
                 BE32(m68k->PC) == 0x00f8422cu))
            {
                /*
                 * Past the allocator/expander chain, Kickstart can resume the
                 * caller-selected JIT sizing. The ARM32 translator now forces
                 * tiny units only for the specific fast-ROM helper windows.
                 */
                pistorm_restore_runtime_jit_control(m68k, BE32(m68k->PC));
            }
        }
#endif
        if (last_PC != (uint32_t)m68k->PC)
        {
#ifdef PISTORM
            if (pistorm_fast_rom_qemu_boot &&
                pistorm_fast_rom_residents != 0 &&
                BE32(m68k->PC) == 0x00f81108u)
            {
                m68k->A[2].u32 = BE32(pistorm_fast_rom_residents);
                m68k->PC = BE32(0x00f8110cu);
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f8113cu)
            {
                uint32_t a1 = BE32(m68k->A[1].u32);
                if (a1 == 0x00f80042u || a1 == 0x00f8413cu)
                {
                    uint32_t flags = pistorm_raw_read(a1 + 0x0au, 1) & 0xffu;

                    if (flags & 0x80u)
                    {
                        m68k->PC = BE32(0x00f8114eu);
                    }
                    else
                    {
                        uint32_t init = pistorm_raw_read(a1 + 0x16u, 4);

                        m68k->A[1].u32 = BE32(init);
                        m68k->D[0].u32 = 0;
                        m68k->A[0].u32 = m68k->D[1].u32;
                        m68k->PC = BE32(init);
                    }
                }
            }
            if (pistorm_fast_rom_qemu_boot &&
                BE32(m68k->PC) == 0x00f81126u)
            {
                uint32_t a1 = BE32(m68k->A[1].u32);
                if (a1 == 0x00f80042u || a1 == 0x00f8413cu)
                {
                    uint32_t d0 = BE32(m68k->D[0].u32);

                    d0 = (d0 & ~0xffu) | (pistorm_raw_read(a1 + 0x0au, 1) & 0xffu);
                    m68k->D[0].u32 = BE32(d0);
                    m68k->PC = BE32(0x00f8112au);
                }
            }
            if (pistorm_tracepc_end > pistorm_tracepc_start &&
                !pistorm_slot_trace_seen &&
                BE32(m68k->PC) == 0x00f81108u)
            {
                uint32_t execbase = BE32(m68k->A[6].u32);
                uint32_t slot12c = 0;

                if (execbase >= 0xff000000u && execbase < 0xffffffffu - 0x130u)
                    slot12c = BE32(*(volatile uint32_t *)(uintptr_t)(execbase + 0x12cu));

                pistorm_trace_puts("[PISTORM:SLOT] a6=");
                pistorm_trace_put_hex32(execbase);
                pistorm_trace_puts(" slot12c=");
                pistorm_trace_put_hex32(slot12c);
                pistorm_trace_puts(" fast=");
                pistorm_trace_put_hex32(pistorm_fast_rom_residents);
                put_char('\n');
                pistorm_slot_trace_seen = 1;
            }
            if (pistorm_mmio_trace && !pistorm_pc_trace_seen)
            {
                uint32_t pc = BE32(m68k->PC);

                if (pc == 0x00f81144u || pc == 0x00f81d60u || pc == 0x00f81d68u || pc == 0x0000ffffu || (pc & 1u))
                {
                    pistorm_pc_trace_seen = 1;
                    kprintf("[PISTORM:PC] suspicious pc=%08x a0=%08x a1=%08x d0=%08x d1=%08x sr=%04x\n",
                        pc,
                        BE32(m68k->A[0].u32),
                        BE32(m68k->A[1].u32),
                        BE32(m68k->D[0].u32),
                        BE32(m68k->D[1].u32),
                        BE16(m68k->SR));

                    if (pc == 0x00f81d60u || pc == 0x00f81d68u)
                    {
                        uint32_t a1 = BE32(m68k->A[1].u32);
                        uint32_t a2 = BE32(m68k->A[2].u32);
                        uint32_t a3 = BE32(m68k->A[3].u32);
                        kprintf("[PISTORM:PC] builder a2=%08x a3=%08x table=%08x %04x %04x %04x %04x %04x %04x %04x %04x\n",
                            a2,
                            a3,
                            a1,
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 0)),
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 2)),
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 4)),
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 6)),
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 8)),
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 10)),
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 12)),
                            BE16(*(volatile uint16_t *)(uintptr_t)(a1 + 14)));
                    }
                }
            }
            if (pistorm_step_trace && ctx_count < 32)
            {
                pistorm_trace_puts("[PISTORM:GETTU] enter pc=");
                pistorm_trace_put_hex32(BE32(m68k->PC));
                put_char('\n');
            }
#endif /* PISTORM */
            unit = M68K_GetTranslationUnit((uint16_t *)(BE32((uint32_t)m68k->PC)));
#ifdef PISTORM
            if (pistorm_step_trace && ctx_count < 32)
            {
                pistorm_trace_puts("[PISTORM:GETTU] leave unit=");
                pistorm_trace_put_hex32((uint32_t)(uintptr_t)unit);
                put_char('\n');
            }
#endif /* PISTORM */
            last_PC = (uint32_t)m68k->PC;
        }

#ifdef PISTORM
        if (pistorm_mmio_trace || pistorm_step_trace || pistorm_tracepc_end > pistorm_tracepc_start)
        {
            int emit_step = pistorm_step_trace;
            uint32_t pc = BE32(m68k->PC);
            uint32_t a0 = BE32(m68k->A[0].u32);
            uint32_t a0_page = a0 & ~0xfffu;
            uint32_t a3 = BE32(m68k->A[3].u32);
            uint32_t a3_page = a3 & ~0xfffu;
            uint32_t a6 = BE32(m68k->A[6].u32);
            uint32_t low0 = BE32(pistorm_handle_special_read(0, 4));
            uint16_t intena = pistorm_intena_shadow;
            uint16_t intreq = pistorm_intreq_shadow;
            uint8_t ipl = m68k->INT.IPL;
            int emit_tracepc = 0;

            if (pistorm_tracepc_end > pistorm_tracepc_start &&
                pc >= pistorm_tracepc_start &&
                pc < pistorm_tracepc_end &&
                (pc != pistorm_tracepc_last_pc ||
                 a0 != pistorm_tracepc_last_a0 ||
                 BE32(m68k->D[1].u32) != pistorm_tracepc_last_d1 ||
                 BE16(m68k->SR) != pistorm_tracepc_last_sr))
            {
                emit_tracepc = 1;
            }

            if (emit_step)
            {
                emit_step =
                    pistorm_step_trace_last_pc == 0xffffffffu ||
                    ((pc ^ pistorm_step_trace_last_pc) & ~0x0fu) != 0 ||
                    a0_page != pistorm_step_trace_last_a0_page ||
                    a3_page != pistorm_step_trace_last_a3_page ||
                    low0 != pistorm_step_trace_last_low0 ||
                    intena != pistorm_step_trace_last_intena ||
                    intreq != pistorm_step_trace_last_intreq ||
                    ipl != pistorm_step_trace_last_ipl;
            }
            else
            {
                uint64_t now = pistorm_read_cntpct();

                if (now >= pistorm_next_progress_trace)
                {
                    uint64_t delta = pistorm_read_cntfrq();
                    if (delta == 0)
                        delta = 1;
                    pistorm_next_progress_trace = now + delta;
                    emit_step = 1;
                }
            }

            if (emit_step)
            {
                kprintf("[PISTORM:STEP] pc=%08x a0=%08x a1=%08x a2=%08x a3=%08x a7=%08x d0=%08x d1=%08x ipl=%02x intena=%04x intreq=%04x\n",
                        pc,
                        a0,
                        BE32(m68k->A[1].u32),
                        BE32(m68k->A[2].u32),
                        a3,
                        BE32(m68k->A[7].u32),
                        BE32(m68k->D[0].u32),
                        BE32(m68k->D[1].u32),
                        ipl,
                        intena,
                        intreq);
                kprintf("[PISTORM:STEP] low0=%08x\n", low0);
                pistorm_step_trace_last_pc = pc;
                pistorm_step_trace_last_a0_page = a0_page;
                pistorm_step_trace_last_a3_page = a3_page;
                pistorm_step_trace_last_low0 = low0;
                pistorm_step_trace_last_intena = intena;
                pistorm_step_trace_last_intreq = intreq;
                pistorm_step_trace_last_ipl = ipl;
            }

            if (emit_tracepc)
            {
                kprintf("[PISTORM:TRACEPC] pc=%08x a0=%08x a1=%08x a2=%08x a3=%08x a4=%08x a5=%08x a6=%08x a7=%08x d0=%08x d1=%08x d2=%08x d3=%08x sr=%04x\n",
                        pc,
                        a0,
                        BE32(m68k->A[1].u32),
                        BE32(m68k->A[2].u32),
                        a3,
                        BE32(m68k->A[4].u32),
                        BE32(m68k->A[5].u32),
                        a6,
                        BE32(m68k->A[7].u32),
                        BE32(m68k->D[0].u32),
                        BE32(m68k->D[1].u32),
                        BE32(m68k->D[2].u32),
                        BE32(m68k->D[3].u32),
                        BE16(m68k->SR));
                if (pc == 0x00f80244u)
                {
                    uint32_t lowbc_raw = 0;
                    uint32_t lowbc_special = BE32(pistorm_handle_special_read(0x000000bcu, 4));

                    if (pistorm_fake_lowram_virt != 0)
                        lowbc_raw = BE32(*(volatile uint32_t *)(uintptr_t)(pistorm_fake_lowram_virt + 0xbcu));
                    kprintf("[PISTORM:TRACEPC] cmpa gate lowbc_raw=%08x lowbc_special=%08x a1=%08x\n",
                            lowbc_raw,
                            lowbc_special,
                            BE32(m68k->A[1].u32));
                }
                if (pc == 0x00f83082u)
                {
                    uint32_t a1 = BE32(m68k->A[1].u32);
                    uint32_t bpl1 = BE32(pistorm_handle_special_read(a6 + 0x0cu, 4));
                    uint32_t bpl2 = BE32(pistorm_handle_special_read(a6 + 0x10u, 4));
                    uint32_t bpl3 = BE32(pistorm_handle_special_read(a6 + 0x14u, 4));
                    uint32_t bpl_end = BE32(pistorm_handle_special_read(a6 + 0x18u, 4));
                    uint32_t list0 = BE32(pistorm_handle_special_read(a1 + 0x00u, 4));
                    uint32_t list1 = BE32(pistorm_handle_special_read(a1 + 0x04u, 4));
                    uint32_t list2 = BE32(pistorm_handle_special_read(a1 + 0x08u, 4));
                    uint32_t list3 = BE32(pistorm_handle_special_read(a1 + 0x0cu, 4));

                    kprintf("[PISTORM:FIXBP] a6=%08x a1=%08x bpl=%08x,%08x,%08x,%08x list=%08x,%08x,%08x,%08x\n",
                            a6,
                            a1,
                            bpl1,
                            bpl2,
                            bpl3,
                            bpl_end,
                            list0,
                            list1,
                            list2,
                            list3);
                }
                if (pc >= 0x00f80352u && pc < 0x00f80384u &&
                    a6 >= 0xff000000u && a6 < 0xffffffffu - 0x230u)
                {
                    volatile uint8_t *a6p = (volatile uint8_t *)(uintptr_t)a6;
                    uint32_t s26 = pistorm_debug_read_be32_unaligned(a6p + 0x26u);
                    uint16_t s22 = pistorm_debug_read_be16_unaligned(a6p + 0x22u);
                    uint16_t s24 = pistorm_debug_read_be16_unaligned(a6p + 0x24u);
                    uint32_t s2a = pistorm_debug_read_be32_unaligned(a6p + 0x2au);
                    uint32_t s3e = pistorm_debug_read_be32_unaligned(a6p + 0x3eu);
                    uint32_t s4e = pistorm_debug_read_be32_unaligned(a6p + 0x4eu);
                    uint16_t s128 = pistorm_debug_read_be16_unaligned(a6p + 0x128u);
                    uint32_t s202 = pistorm_debug_read_be32_unaligned(a6p + 0x202u);
                    uint32_t s206 = pistorm_debug_read_be32_unaligned(a6p + 0x206u);
                    uint32_t s20e = pistorm_debug_read_be32_unaligned(a6p + 0x20eu);
                    uint32_t s222 = pistorm_debug_read_be32_unaligned(a6p + 0x222u);
                    uint32_t s226 = pistorm_debug_read_be32_unaligned(a6p + 0x226u);
                    uint32_t s22a = pistorm_debug_read_be32_unaligned(a6p + 0x22au);
                    kprintf("[PISTORM:A6] a6=%08x 22=%04x 24=%04x 26=%08x 2a=%08x 3e=%08x 4e=%08x 128=%04x 202=%08x 206=%08x 20e=%08x 222=%08x 226=%08x 22a=%08x\n",
                            a6, s22, s24, s26, s2a, s3e, s4e, s128, s202, s206, s20e, s222, s226, s22a);
                }
                if (pc >= 0x00f81104u && pc < 0x00f81112u &&
                    a6 >= 0xff000000u && a6 < 0xffffffffu - 0x130u)
                {
                    uint32_t slot12c = BE32(*(volatile uint32_t *)(uintptr_t)(a6 + 0x12cu));
                    kprintf("[PISTORM:A6SLOT] a6=%08x slot12c=%08x fast=%08x\n",
                            a6,
                            slot12c,
                            pistorm_fast_rom_residents);
                }
                if (pc >= 0x00f81f9cu && pc < 0x00f82028u &&
                    a6 >= 0xff000000u && a6 < 0xffffffffu - 0x22eu)
                {
                    volatile uint8_t *a6p = (volatile uint8_t *)(uintptr_t)a6;
                    uint32_t list142 = pistorm_debug_read_be32_unaligned(a6p + 0x142u);
                    uint32_t list146 = pistorm_debug_read_be32_unaligned(a6p + 0x146u);
                    uint16_t s124 = pistorm_debug_read_be16_unaligned(a6p + 0x124u);
                    uint8_t s126 = *(volatile uint8_t *)(a6p + 0x126u);
                    uint8_t s127 = *(volatile uint8_t *)(a6p + 0x127u);
                    uint32_t s222 = pistorm_debug_read_be32_unaligned(a6p + 0x222u);
                    uint32_t s226 = pistorm_debug_read_be32_unaligned(a6p + 0x226u);
                    uint32_t s22a = pistorm_debug_read_be32_unaligned(a6p + 0x22au);
                    uint32_t head142 = 0xffffffffu;
                    uint32_t head146 = 0xffffffffu;
                    uint16_t list142_0e = 0xffffu;
                    uint32_t list142_1c = 0xffffffffu;
                    uint16_t head142_0e = 0xffffu;
                    uint32_t head142_1c = 0xffffffffu;

                    if (list142 < 0x01000000u)
                    {
                        head142 = BE32(pistorm_handle_special_read(list142, 4));
                        list142_0e = (uint16_t)BE16((uint16_t)pistorm_handle_special_read(list142 + 0x0eu, 2));
                        list142_1c = BE32(pistorm_handle_special_read(list142 + 0x1cu, 4));
                    }
                    if (list146 < 0x01000000u)
                        head146 = BE32(pistorm_handle_special_read(list146, 4));
                    if (head142 < 0x01000000u)
                    {
                        head142_0e = (uint16_t)BE16((uint16_t)pistorm_handle_special_read(head142 + 0x0eu, 2));
                        head142_1c = BE32(pistorm_handle_special_read(head142 + 0x1cu, 4));
                    }

                    kprintf("[PISTORM:ALLOC] a6=%08x 124=%04x 126=%02x 127=%02x 142=%08x[%08x|0e=%04x|1c=%08x] 146=%08x[%08x] node142=%08x[0e=%04x|1c=%08x] 222=%08x 226=%08x 22a=%08x\n",
                            a6,
                            s124,
                            s126,
                            s127,
                            list142,
                            head142,
                            list142_0e,
                            list142_1c,
                            list146,
                            head146,
                            head142,
                            head142_0e,
                            head142_1c,
                            s222,
                            s226,
                            s22a);
                }
                if ((pc == 0x00f81e78u || pc == 0x00f81e8au) &&
                    a6 >= 0xff000000u && a6 < 0xffffffffu - 0x146u)
                {
                    uint32_t list142 = BE32(*(volatile uint32_t *)(uintptr_t)(a6 + 0x142u));
                    uint32_t list146 = BE32(*(volatile uint32_t *)(uintptr_t)(a6 + 0x146u));
                    kprintf("[PISTORM:ALLOCWALK] pc=%08x a1=%08x d1=%08x a6=%08x list142=%08x list146=%08x\n",
                            pc,
                            BE32(m68k->A[1].u32),
                            BE32(m68k->D[1].u32),
                            a6,
                            list142,
                            list146);
                    if (list142 != 0)
                        pistorm_debug_dump_memnode_chain("alloc142", list142);
                    if (list146 != 0)
                        pistorm_debug_dump_memnode_chain("alloc146", list146);
                }
                pistorm_tracepc_last_pc = pc;
                pistorm_tracepc_last_a0 = a0;
                pistorm_tracepc_last_d1 = BE32(m68k->D[1].u32);
                pistorm_tracepc_last_sr = BE16(m68k->SR);
            }
        }
#endif

        *(void**)(&arm_code) = unit->mt_ARMEntryPoint;
#ifdef PISTORM
        {
            uint32_t trace_call_last_pc = last_PC;
            int emit_trace_return = 0;

            if (pistorm_step_trace && ctx_count < 32)
            {
                pistorm_trace_puts("[PISTORM:ENTRY] arm=");
                pistorm_trace_put_hex32((uint32_t)(uintptr_t)arm_code);
                pistorm_trace_puts(" unit=");
                pistorm_trace_put_hex32((uint32_t)(uintptr_t)unit);
                put_char('\n');
            }
            if (pistorm_step_trace && ctx_count < 32)
            {
                pistorm_trace_puts("[PISTORM:PRECALL] n=");
                pistorm_trace_put_hex32((uint32_t)ctx_count);
                pistorm_trace_puts(" pc=");
                pistorm_trace_put_hex32(BE32(m68k->PC));
                pistorm_trace_puts(" last=");
                pistorm_trace_put_hex32(last_PC);
                pistorm_trace_puts(" jit=");
                pistorm_trace_put_hex32(__m68k.JIT_CONTROL);
                put_char('\n');
            }
            else if (pistorm_tracepc_end > pistorm_tracepc_start &&
                     last_PC >= pistorm_tracepc_start &&
                     last_PC < pistorm_tracepc_end)
            {
                emit_trace_return = 1;
                pistorm_trace_puts("[PISTORM:TRACECALL] n=");
                pistorm_trace_put_hex32((uint32_t)ctx_count);
                pistorm_trace_puts(" pc=");
                pistorm_trace_put_hex32(BE32(m68k->PC));
                pistorm_trace_puts(" last=");
                pistorm_trace_put_hex32(last_PC);
                put_char('\n');
            }

            armhf_call_translation_unit(arm_code, m68k);

            if (pistorm_step_trace && ctx_count < 32)
            {
                pistorm_trace_puts("[PISTORM:POSTCALL] n=");
                pistorm_trace_put_hex32((uint32_t)ctx_count);
                pistorm_trace_puts(" pc=");
                pistorm_trace_put_hex32(BE32(m68k->PC));
                pistorm_trace_puts(" sr=");
                pistorm_trace_put_hex32(BE16(m68k->SR));
                pistorm_trace_puts(" d0=");
                pistorm_trace_put_hex32(BE32(m68k->D[0].u32));
                pistorm_trace_puts(" d5=");
                pistorm_trace_put_hex32(BE32(m68k->D[5].u32));
                pistorm_trace_puts(" a0=");
                pistorm_trace_put_hex32(BE32(m68k->A[0].u32));
                pistorm_trace_puts(" a6=");
                pistorm_trace_put_hex32(BE32(m68k->A[6].u32));
                pistorm_trace_puts(" a7=");
                pistorm_trace_put_hex32(BE32(m68k->A[7].u32));
                put_char('\n');
            }
            else if (emit_trace_return)
            {
                pistorm_trace_puts("[PISTORM:TRACERET] n=");
                pistorm_trace_put_hex32((uint32_t)ctx_count);
                pistorm_trace_puts(" pc=");
                pistorm_trace_put_hex32(BE32(m68k->PC));
                pistorm_trace_puts(" from=");
                pistorm_trace_put_hex32(trace_call_last_pc);
                pistorm_trace_puts(" last=");
                pistorm_trace_put_hex32(last_PC);
                pistorm_trace_puts(" sr=");
                pistorm_trace_put_hex32(BE16(m68k->SR));
                pistorm_trace_puts(" d0=");
                pistorm_trace_put_hex32(BE32(m68k->D[0].u32));
                pistorm_trace_puts(" d3=");
                pistorm_trace_put_hex32(BE32(m68k->D[3].u32));
                pistorm_trace_puts(" a0=");
                pistorm_trace_put_hex32(BE32(m68k->A[0].u32));
                put_char('\n');
            }
        }
#else
        armhf_call_translation_unit(arm_code, m68k);
#endif
#ifdef PISTORM
        if (addr == NULL && pistorm_fast_rom_qemu_boot && m68k->PC == 0)
        {
            kprintf("[PISTORM:FASTEXIT] last=%08x d0=%08x d1=%08x a0=%08x a1=%08x a6=%08x sp=%08x sr=%04x\n",
                last_PC,
                BE32(m68k->D[0].u32),
                BE32(m68k->D[1].u32),
                BE32(m68k->A[0].u32),
                BE32(m68k->A[1].u32),
                BE32(m68k->A[6].u32),
                BE32(m68k->A[7].u32),
                BE16(m68k->SR));
        }
#endif
        ctx_count++;

    } while(m68k->PC != 0);

    t2 = LE32(*(volatile uint32_t*)0xf2003004) | (uint64_t)LE32(*(volatile uint32_t *)0xf2003008) << 32;

    printf("[JIT] Time spent in m68k mode: %lld us\n", t2-t1);
    printf("[JIT] Number of ARM-M68k switches: %lld\n", ctx_count);

    printf("[JIT] Back from translated code\n");

    print_context(m68k);
}
#if 0
    printf("[JIT] --- Stack dump ---\n");
    for (int i=1024; i > 0; --i)
    {
        printf("[JIT]   sp[%04d] = %08x\n", i, BE32(stack[i]));
        if (stack[i] == 0xaaaaaaaa)
            break;
    }
#endif
    M68K_DumpStats();
#else
(void)addr;
#endif
}
