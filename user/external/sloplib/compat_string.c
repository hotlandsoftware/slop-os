#include "include/string.h"

void *memcpy(void *dst, const void *src, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        ((char *)dst)[i] = ((const char *)src)[i];
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    size_t i;
    if (dst == src) return dst;
    if ((char *)dst < (const char *)src) {
        return memcpy(dst, src, n);
    }
    for (i = n; i > 0; --i) {
        ((char *)dst)[i - 1u] = ((const char *)src)[i - 1u];
    }
    return dst;
}

void *memset(void *dst, int ch, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        ((char *)dst)[i] = (char)ch;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned char ac = ((const unsigned char *)a)[i];
        unsigned char bc = ((const unsigned char *)b)[i];
        if (ac != bc) return (int)ac - (int)bc;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (((const unsigned char *)s)[i] == (unsigned char)c) {
            return (void *)((const unsigned char *)s + i);
        }
    }
    return (void *)0;
}

size_t strlen(const char *s) {
    size_t n = 0u;
    while (s && s[n] != '\0') ++n;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        ++a; ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    size_t i = 0u;
    while (i < n && a[i] && b[i] && a[i] == b[i]) ++i;
    if (i == n) return 0;
    return (i < n) ? ((unsigned char)a[i] - (unsigned char)b[i]) : 0;
}

char *strcpy(char *dst, const char *src) {
    size_t i = 0u;
    while ((dst[i] = src[i]) != '\0') ++i;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0u;
    while (i < n && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    while (i < n) dst[i++] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src) {
    size_t i = strlen(dst);
    size_t j = 0u;
    while ((dst[i++] = src[j++]) != '\0') {}
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        ++s;
    }
    return ((char)c == '\0') ? (char *)s : (char *)0;
}

char *strrchr(const char *s, int c) {
    const char *last = (const char *)0;
    while (*s) {
        if (*s == (char)c) last = s;
        ++s;
    }
    if ((char)c == '\0') return (char *)s;
    return (char *)last;
}

char *strerror(int errnum) {
    switch (errnum) {
        case 2: return "No such file or directory";
        case 5: return "I/O error";
        case 12: return "Out of memory";
        case 22: return "Invalid argument";
        case 34: return "Result out of range";
        default: return "Error";
    }
}
