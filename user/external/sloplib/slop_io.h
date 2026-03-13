#ifndef SLOP_IO_H
#define SLOP_IO_H

#include "../../sys.h"

#define SLOP_O_WRONLY 0x1u
#define SLOP_O_RDWR   0x2u
#define SLOP_O_CREAT  0x40u
#define SLOP_O_TRUNC  0x200u
#define SLOP_O_APPEND 0x400u

static inline int slop_open_read(const char *path) {
    return sys_open(path, 0u);
}

static inline int slop_open_write_trunc(const char *path) {
    return sys_open(path, SLOP_O_WRONLY | SLOP_O_CREAT | SLOP_O_TRUNC);
}

static inline int slop_open_readwrite(const char *path) {
    return sys_open(path, SLOP_O_RDWR | SLOP_O_CREAT);
}

static inline int slop_write_all(int fd, const char *buf, unsigned int len) {
    unsigned int off = 0u;

    while (off < len) {
        int rc = sys_write(fd, buf + off, len - off);
        if (rc <= 0) {
            return -1;
        }
        off += (unsigned int)rc;
    }
    return 0;
}

static inline int slop_write_cstr(const char *s) {
    return slop_write_all(1, s, cstr_len(s));
}

#endif
