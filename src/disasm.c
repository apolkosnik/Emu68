#include <capstone/capstone.h>
#include <tlsf.h>
#include <support.h>
#include <stdarg.h>

extern void *tlsf;

void *my_malloc(size_t size)
{
    return tlsf_malloc(tlsf, size);
}

void *my_calloc(size_t nmemb, size_t size)
{
    void *ptr = tlsf_malloc(tlsf, size * nmemb);
    bzero(ptr, size * nmemb);
    return ptr;
}

void *my_realloc(void *ptr, size_t size)
{
    return tlsf_realloc(tlsf, ptr, size);
}

void my_free(void *ptr)
{
    tlsf_free(tlsf, ptr);
}

struct putc_data {
    char *buffer;
    size_t written;
    size_t limit;
};

void my_putc(void *data, const char c)
{
    struct putc_data *p = data;

    if (p->limit > 0 && p->written + 1 < p->limit)
    {
        p->buffer[p->written] = c;
        p->buffer[p->written+1] = 0;
    }

    p->written++;
}

int my_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    struct putc_data p;
    p.buffer = str;
    p.limit = size;
    p.written = 0;

    if (size > 0)
        str[0] = 0;

    vkprintf_pc(my_putc, &p, format, ap);

    return p.written;
}

#ifdef __aarch64__
static const char *format_arm_op_str(const cs_insn *insn, char *buffer, size_t buffer_size)
{
    char last_char = 0;
    size_t p = 0;
    static const char *regnames[] = {
        "A0", "A1", "A2", "A3", "A4", "PC", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "A5", "A6", "A7"
    };

    if (buffer_size == 0)
        return "";

    for (int j=0; j < 160 && p + 1 < buffer_size; j++) {
        char size = 0;

        if (insn->op_str[j] == 0)
            break;

        if (insn->op_str[j] == 'w')
            size = 'w';
        else if (insn->op_str[j] == 'x' && last_char != '0')
            size = 'x';

        if (size) {
            char c1 = insn->op_str[j + 1];
            char c2 = insn->op_str[j + 2];

            if (
                (c1 >= '0' && c1 <= '9' && c2 >= '0' && c2 <= '9') &&
                (insn->op_str[j + 3] < '0' || insn->op_str[j + 3] > '9') &&
                (insn->op_str[j + 3] < 'a' || insn->op_str[j + 3] > 'f')
            )
            {
                char num = (c1-'0')*10 + (c2-'0');

                if (num >= 13 && num <= 29)
                {
                    const char *src = regnames[num - 13];

                    while (*src != 0 && p + 1 < buffer_size)
                        buffer[p++] = *src++;

                    if (size == 'x' && p + 3 < buffer_size) {
                        buffer[p++] = ':';
                        buffer[p++] = '6';
                        buffer[p++] = '4';
                    }

                    j += 2;
                    last_char = insn->op_str[j];
                    continue;
                }
            }
        }

        buffer[p++] = insn->op_str[j];
        last_char = insn->op_str[j];
    }

    buffer[p] = 0;
    return buffer;
}
#else
static const char *format_arm_op_str(const cs_insn *insn, char *buffer, size_t buffer_size)
{
    (void)buffer;
    (void)buffer_size;
    return insn->op_str;
}
#endif

void disasm_init()
{
    cs_opt_mem setup;

    setup.malloc = my_malloc;
    setup.calloc = my_calloc;
    setup.realloc = my_realloc;
    setup.free = my_free;
    setup.vsnprintf = my_vsnprintf;

    if (!cs_option(0, CS_OPT_MEM, (uintptr_t)&setup)) {
        kprintf("[BOOT] Disassembler set up\n");
    } else {
        kprintf("[BOOT] Disassembler init error\n");
    }
}

csh h_m68k;
csh h_arm;

void disasm_open()
{
    cs_err err;
    err = cs_open(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040, &h_m68k);
#ifdef __aarch64__
    err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &h_arm);
#else
    err = cs_open(CS_ARCH_ARM, CS_MODE_ARM, &h_arm);
#endif
    (void)err;
}

void disasm_close()
{
    cs_close(&h_m68k);
    cs_close(&h_arm);
}

void disasm_print(uint16_t *m68k_addr, uint16_t m68k_count, uint32_t *arm_addr, size_t arm_size, uint32_t *arm_start)
{
    cs_insn *insn_m68k;
    cs_insn *insn_arm;
    size_t count_m68k = 0;
    size_t count_arm = 0;
    char fixed_op_str[200];

    if (m68k_addr)
        count_m68k = cs_disasm(h_m68k, (const uint8_t *)m68k_addr, 20*m68k_count, (uintptr_t)m68k_addr, m68k_count, &insn_m68k);
    if (arm_addr)
        count_arm = cs_disasm(h_arm, (const uint8_t *)arm_addr, arm_size, (uintptr_t)arm_addr - (uintptr_t)arm_start, 0, &insn_arm);

    for (size_t i=0; i < count_m68k; i++)
    {
        kprintf("[JIT] %08llx: %7s %21s", (unsigned long long)insn_m68k[i].address,
            insn_m68k[i].mnemonic, insn_m68k[i].op_str);
        if (i != count_m68k - 1)
            kprintf("\n");
    }

    if (count_m68k == 0)
        kprintf("[JIT]                                        ");

    for (size_t i=0; i < count_arm; i++)
    {
        if (i > 0)
            kprintf("[JIT]                                        ");
        kprintf("-> %08llx: %7s %s\n", (unsigned long long)insn_arm[i].address, insn_arm[i].mnemonic,
            format_arm_op_str(&insn_arm[i], fixed_op_str, sizeof(fixed_op_str)));
    }

    if (count_m68k)
        cs_free(insn_m68k, count_m68k);
    if (count_arm)
        cs_free(insn_arm, count_arm);
}
