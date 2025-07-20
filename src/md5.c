#include <stdint.h>
#include "md5.h"
#include "support.h"

static const uint8_t s[] = { 7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21 };

static const uint32_t K[] = { 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };

static inline void prefetch(void *from, void *to)
{
#if defined(__aarch64__)
    uint64_t t1, t2;

    asm volatile("ldp %0, %1, [%2, #0]; stp %0, %1, [%3, #0]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
    asm volatile("ldp %0, %1, [%2, #16]; stp %0, %1, [%3, #16]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
    asm volatile("ldp %0, %1, [%2, #32]; stp %0, %1, [%3, #32]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
    asm volatile("ldp %0, %1, [%2, #48]; stp %0, %1, [%3, #48]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
#else
    // Simple memcpy for AArch32
    uint32_t *src = (uint32_t *)from;
    uint32_t *dst = (uint32_t *)to;
    for (int i = 0; i < 16; i++) {
        dst[i] = src[i];
    }
#endif
}

struct MD5 CalcMD5(void *_start, void *_end)
{
    struct MD5 md = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

    uint32_t *M = (uint32_t*)_start;
    uint32_t *E = (uint32_t*)((intptr_t)_start + (((intptr_t)_end - (intptr_t)_start + 63) & ~63));
    uint32_t count = 1 + (E - M) / 16;
    uint32_t pad[16];

    prefetch(M, pad);

    while(count--)
    {
        uint32_t A = md.a;
        uint32_t B = md.b;
        uint32_t C = md.c;
        uint32_t D = md.d;

        for (int i=0; i < 64; i++)
        {
            uint32_t F, g;

            if (i <= 15)
            {
                F = (B & C) | ((~B) & D);
                g = i;
            }
            else if (i <= 31)
            {
                F = (D & B) | ((~D) & C);
                g = (5*i + 1) & 15;
            }
            else if (i <= 47)
            {
                F = B ^ C ^ D;
                g = (3*i + 5) & 15;
            }
            else
            {
                F = C ^ (B | ~D);
                g = (7*i) & 15;
            }

            F = F + A + K[i] + pad[g];
            A = D;
            D = C;
            C = B;
            B = B + ((F << s[i]) | (F >> (32 - s[i])));
        }

        md.a += A;
        md.b += B;
        md.c += C;
        md.d += D;

        M = M + 16;
        if (M == E)
        {
            pad[0] = BE32(0x80000000);
            for (int i=1; i < 14; i++)
                pad[i] = 0;
            pad[14] = (uintptr_t)_start;
            pad[15] = (uintptr_t)_end - (uintptr_t)_start;
        }
        else
        {
            prefetch(M, pad);
        }
    }

    return md;
}

uint32_t CalcCRC32(void *_start, void *_end)
{
    intptr_t s = (intptr_t)_start;
    intptr_t e = (intptr_t)_end;

    uint32_t crc = 0xffffffff;

#if defined(__aarch64__)
    while((e - s) >= 16) {
        uint64_t val1; uint64_t val2;
        asm volatile("ldp %0, %1, [%2]":"=r"(val1), "=r"(val2):"r"(s));
        asm volatile("crc32x %w0, %w0, %2":"=r"(crc):"0"(crc),"r"(val1));
        asm volatile("crc32x %w0, %w0, %2":"=r"(crc):"0"(crc),"r"(val2));
        s += 16;
    }
    if ((e - s) >= 8) {
        uint64_t val = *(uint64_t *)s;
        asm volatile("crc32x %w0, %w0, %2":"=r"(crc):"0"(crc),"r"(val));
        s += 8;
    }
    if ((e - s) >= 4) {
        uint32_t val = *(uint32_t *)s;
        asm volatile("crc32w %w0, %w0, %w2":"=r"(crc):"0"(crc),"r"(val));
        s += 4;
    }
    if ((e - s) >= 2) {
        uint16_t val = *(uint16_t *)s;
        asm volatile("crc32h %w0, %w0, %w2":"=r"(crc):"0"(crc),"r"(val));
        s += 2;
    }
    if (e != s) {
        uint16_t val = *(uint16_t *)s;
        asm volatile("crc32b %w0, %w0, %w2":"=r"(crc):"0"(crc),"r"(val));
    }
#else
    // Software CRC32 implementation for AArch32
    static const uint32_t crc32_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };
    
    const uint8_t *data = (const uint8_t *)s;
    while (s < e) {
        crc = crc32_table[(crc ^ *data++) & 0xff] ^ (crc >> 8);
        s++;
    }
#endif

    return crc;
}