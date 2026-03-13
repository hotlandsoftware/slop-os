#include "../../sys.h"
#include "include/errno.h"
#include "include/limits.h"
#include "include/stdlib.h"
#include "include/string.h"
#include "slop_alloc.h"

void *malloc(unsigned int size) { return slop_malloc(size); }
void *realloc(void *ptr, unsigned int size) { return slop_realloc(ptr, size); }
void free(void *ptr) { slop_free(ptr); }

static int str_eq_c(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

const char *getenv(const char *name) {
    if (!name) {
        return (const char *)0;
    }
    if (str_eq_c(name, "TMPDIR")) {
        return "/tmp";
    }
    if (str_eq_c(name, "HOME")) {
        return "/";
    }
    return (const char *)0;
}

long strtol(const char *nptr, char **endptr, int base) {
    const char *p = nptr;
    long value = 0;
    int neg = 0;
    int any = 0;

    errno = 0;
    if (!nptr || (base != 0 && base != 10)) {
        errno = EINVAL;
        if (endptr) *endptr = (char *)nptr;
        return 0;
    }

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        ++p;
    }
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        ++p;
    }
    if (base == 0) {
        base = 10;
    }

    while (*p >= '0' && *p <= '9') {
        int digit = *p - '0';
        if (digit >= base) {
            break;
        }
        any = 1;
        if (value > (LONG_MAX - digit) / base) {
            errno = ERANGE;
            value = LONG_MAX;
            while (*p >= '0' && *p <= '9') ++p;
            break;
        }
        value = (value * base) + digit;
        ++p;
    }

    if (!any) {
        if (endptr) *endptr = (char *)nptr;
        return 0;
    }
    if (endptr) *endptr = (char *)p;
    return neg ? -value : value;
}

void exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(60), "b"(code)
        : "memory");
    for (;;) {}
}

void abort(void) {
    exit(1);
}

int system(const char *cmd) {
    (void)cmd;
    errno = EINVAL;
    return -1;
}
