#include "kernel.h"

int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

int str_cmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        ++a;
        ++b;
    }
    return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}

int str_startswith(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s != *prefix) {
            return 0;
        }
        ++s;
        ++prefix;
    }
    return 1;
}

u32 str_len(const char *s) {
    u32 len = 0;
    while (*s++) {
        ++len;
    }
    return len;
}

void str_copy(char *dst, const char *src, size_t size) {
    if (size == 0u) {
        return;
    }

    while (*src && size > 1u) {
        *dst++ = *src++;
        --size;
    }
    *dst = '\0';
}

u32 parse_u32_dec(const char *s, int *ok) {
    u32 value = 0;

    if (*s == '\0') {
        *ok = 0;
        return 0;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            *ok = 0;
            return 0;
        }
        value = (value * 10u) + (u32)(*s - '0');
        ++s;
    }

    *ok = 1;
    return value;
}

void mem_zero(void *dst, size_t size) {
    u32 *words = (u32 *)dst;
    u8 *bytes;

    /* Fast path: write 4 bytes at a time. kmalloc aligns to 16 bytes so dst
       is always at least 4-byte aligned when called from kcalloc. */
    while (size >= 4u) {
        *words++ = 0u;
        size -= 4u;
    }

    bytes = (u8 *)words;
    while (size > 0u) {
        *bytes++ = 0;
        --size;
    }
}
