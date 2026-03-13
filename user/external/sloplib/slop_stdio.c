#include "../../sys.h"
#include "slop_alloc.h"
#include "slop_io.h"
#include "slop_stdio.h"

SLOP_FILE slop_stdin_file = { 0, 0, 0 };
SLOP_FILE slop_stdout_file = { 1, 0, 0 };
SLOP_FILE slop_stderr_file = { 2, 0, 0 };

#define SLOP_FILE_KIND_FD  1
#define SLOP_FILE_KIND_MEM 2

static int slop_streq(const char *a, const char *b) {
    unsigned int i = 0u;
    if (!a || !b) {
        return 0;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }
    return a[i] == b[i];
}

static void slop_write_u32_dec(char *buf, unsigned int *ioff, unsigned int value) {
    char tmp[16];
    unsigned int n = 0u;

    if (value == 0u) {
        buf[(*ioff)++] = '0';
        return;
    }

    while (value > 0u && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (n > 0u) {
        buf[(*ioff)++] = tmp[--n];
    }
}

static void slop_write_s32_dec(char *buf, unsigned int *ioff, int value) {
    unsigned int mag;
    if (value < 0) {
        buf[(*ioff)++] = '-';
        mag = (unsigned int)(-(value + 1)) + 1u;
    } else {
        mag = (unsigned int)value;
    }
    slop_write_u32_dec(buf, ioff, mag);
}

static int slop_ensure_mem_capacity(SLOP_FILE *fp, unsigned int needed) {
    unsigned int capacity;
    char *new_mem;
    unsigned int i;

    if (!fp) {
        return 0;
    }
    if (needed <= fp->capacity) {
        return 1;
    }

    capacity = fp->capacity ? fp->capacity : 256u;
    while (capacity < needed) {
        capacity *= 2u;
    }

    new_mem = (char *)slop_realloc(fp->mem, capacity);
    if (!new_mem) {
        return 0;
    }
    for (i = fp->capacity; i < capacity; ++i) {
        new_mem[i] = '\0';
    }
    fp->mem = new_mem;
    fp->capacity = capacity;
    return 1;
}

SLOP_FILE *slop_fopen(const char *path, const char *mode) {
    SLOP_FILE *fp;
    unsigned int flags = 0u;
    int fd;

    if (!path || !mode) {
        return (SLOP_FILE *)0;
    }

    if (slop_streq(mode, "r")) {
        flags = 0u;
    } else if (slop_streq(mode, "w")) {
        flags = SLOP_O_WRONLY | SLOP_O_CREAT | SLOP_O_TRUNC;
    } else if (slop_streq(mode, "a")) {
        flags = SLOP_O_WRONLY | SLOP_O_CREAT | SLOP_O_APPEND;
    } else if (slop_streq(mode, "r+")) {
        flags = SLOP_O_RDWR;
    } else if (slop_streq(mode, "w+")) {
        flags = SLOP_O_RDWR | SLOP_O_CREAT | SLOP_O_TRUNC;
    } else if (slop_streq(mode, "a+")) {
        flags = SLOP_O_RDWR | SLOP_O_CREAT | SLOP_O_APPEND;
    } else {
        return (SLOP_FILE *)0;
    }

    fd = sys_open(path, flags);
    if (fd < 0) {
        return (SLOP_FILE *)0;
    }

    fp = (SLOP_FILE *)slop_malloc((unsigned int)sizeof(SLOP_FILE));
    if (!fp) {
        (void)sys_close(fd);
        return (SLOP_FILE *)0;
    }

    fp->fd = fd;
    fp->kind = SLOP_FILE_KIND_FD;
    fp->eof = 0;
    fp->err = 0;
    fp->owned = 1;
    fp->pos = 0u;
    fp->size = 0u;
    fp->capacity = 0u;
    fp->mem = (char *)0;
    return fp;
}

int slop_fclose(SLOP_FILE *fp) {
    int rc;
    if (!fp) {
        return -1;
    }
    rc = 0;
    if (fp->kind == SLOP_FILE_KIND_FD && fp->owned) {
        rc = sys_close(fp->fd);
    }
    slop_free(fp);
    return rc < 0 ? -1 : 0;
}

int slop_fflush(SLOP_FILE *fp) {
    (void)fp;
    return 0;
}

int slop_setvbuf(SLOP_FILE *fp, char *buf, int mode, unsigned int size) {
    (void)fp;
    (void)buf;
    (void)mode;
    (void)size;
    return 0;
}

int slop_fgetc(SLOP_FILE *fp) {
    char ch;
    int rc;

    if (!fp) {
        return -1;
    }
    if (fp->kind == SLOP_FILE_KIND_MEM) {
        if (fp->pos >= fp->size) {
            fp->eof = 1;
            return -1;
        }
        return (unsigned char)fp->mem[fp->pos++];
    }
    rc = sys_read(fp->fd, &ch, 1u);
    if (rc <= 0) {
        fp->eof = (rc == 0);
        fp->err = (rc < 0);
        return -1;
    }
    return (unsigned char)ch;
}

int slop_getchar(void) {
    return slop_fgetc(slop_stdin);
}

int slop_fputc(int ch, SLOP_FILE *fp) {
    char c = (char)ch;
    int rc;

    if (!fp) {
        return -1;
    }
    if (fp->kind == SLOP_FILE_KIND_MEM) {
        if (!slop_ensure_mem_capacity(fp, fp->pos + 2u)) {
            fp->err = 1;
            return -1;
        }
        fp->mem[fp->pos++] = c;
        if (fp->pos > fp->size) {
            fp->size = fp->pos;
        }
        fp->mem[fp->size] = '\0';
        return (unsigned char)c;
    }
    rc = sys_write(fp->fd, &c, 1u);
    if (rc != 1) {
        fp->err = 1;
        return -1;
    }
    return (unsigned char)c;
}

int slop_putchar(int ch) {
    return slop_fputc(ch, slop_stdout);
}

unsigned int slop_fread(void *ptr, unsigned int size, unsigned int nmemb, SLOP_FILE *fp) {
    unsigned int total;
    unsigned int got = 0u;
    int rc;

    if (!ptr || !fp || size == 0u || nmemb == 0u) {
        return 0u;
    }

    if (fp->kind == SLOP_FILE_KIND_MEM) {
        unsigned int available = (fp->pos < fp->size) ? (fp->size - fp->pos) : 0u;
        unsigned int want = size * nmemb;
        unsigned int take = (want < available) ? want : available;
        unsigned int i;
        for (i = 0u; i < take; ++i) {
            ((char *)ptr)[i] = fp->mem[fp->pos + i];
        }
        fp->pos += take;
        if (take < want) {
            fp->eof = 1;
        }
        return take / size;
    }

    total = size * nmemb;
    while (got < total) {
        rc = sys_read(fp->fd, ((char *)ptr) + got, total - got);
        if (rc <= 0) {
            fp->eof = (rc == 0);
            fp->err = (rc < 0);
            break;
        }
        got += (unsigned int)rc;
    }
    return got / size;
}

unsigned int slop_fwrite(const void *ptr, unsigned int size, unsigned int nmemb, SLOP_FILE *fp) {
    unsigned int total;
    unsigned int off = 0u;
    int rc;

    if (!ptr || !fp || size == 0u || nmemb == 0u) {
        return 0u;
    }

    if (fp->kind == SLOP_FILE_KIND_MEM) {
        total = size * nmemb;
        if (!slop_ensure_mem_capacity(fp, fp->pos + total + 1u)) {
            fp->err = 1;
            return 0u;
        }
        while (off < total) {
            fp->mem[fp->pos + off] = ((const char *)ptr)[off];
            ++off;
        }
        fp->pos += total;
        if (fp->pos > fp->size) {
            fp->size = fp->pos;
        }
        fp->mem[fp->size] = '\0';
        return nmemb;
    }

    total = size * nmemb;
    while (off < total) {
        rc = sys_write(fp->fd, ((const char *)ptr) + off, total - off);
        if (rc <= 0) {
            fp->err = 1;
            break;
        }
        off += (unsigned int)rc;
    }
    return off / size;
}

int slop_fputs(const char *s, SLOP_FILE *fp) {
    if (!s || !fp) {
        return -1;
    }
    return slop_write_all(fp->fd, s, cstr_len(s)) == 0 ? 0 : -1;
}

int slop_puts(const char *s) {
    if (slop_fputs(s, slop_stdout) < 0) {
        return -1;
    }
    return slop_fputc('\n', slop_stdout);
}

char *slop_fgets(char *buf, int size, SLOP_FILE *fp) {
    int i = 0;
    int ch;

    if (!buf || !fp || size <= 1) {
        return (char *)0;
    }

    while (i < size - 1) {
        ch = slop_fgetc(fp);
        if (ch < 0) {
            break;
        }
        buf[i++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (i == 0) {
        return (char *)0;
    }

    buf[i] = '\0';
    return buf;
}

int slop_fseek(SLOP_FILE *fp, long offset, int whence) {
    long base;
    long pos;

    if (!fp) {
        return -1;
    }
    if (fp->kind != SLOP_FILE_KIND_MEM) {
        fp->err = 1;
        return -1;
    }

    if (whence == 0) {
        base = 0;
    } else if (whence == 1) {
        base = (long)fp->pos;
    } else if (whence == 2) {
        base = (long)fp->size;
    } else {
        fp->err = 1;
        return -1;
    }

    pos = base + offset;
    if (pos < 0) {
        fp->err = 1;
        return -1;
    }
    fp->pos = (unsigned int)pos;
    fp->eof = 0;
    return 0;
}

long slop_ftell(SLOP_FILE *fp) {
    if (!fp) {
        return -1;
    }
    return (long)fp->pos;
}

int slop_feof(SLOP_FILE *fp) {
    return fp ? fp->eof : 1;
}

int slop_ferror(SLOP_FILE *fp) {
    return fp ? fp->err : 1;
}

void slop_clearerr(SLOP_FILE *fp) {
    if (fp) {
        fp->eof = 0;
        fp->err = 0;
    }
}

SLOP_FILE *slop_tmpfile(void) {
    SLOP_FILE *fp = (SLOP_FILE *)slop_malloc((unsigned int)sizeof(SLOP_FILE));
    if (!fp) {
        return (SLOP_FILE *)0;
    }
    fp->fd = -1;
    fp->kind = SLOP_FILE_KIND_MEM;
    fp->eof = 0;
    fp->err = 0;
    fp->owned = 1;
    fp->pos = 0u;
    fp->size = 0u;
    fp->capacity = 0u;
    fp->mem = (char *)0;
    return fp;
}

int slop_vfprintf(SLOP_FILE *fp, const char *fmt, va_list ap) {
    char out[512];
    unsigned int off = 0u;
    unsigned int i = 0u;

    if (!fp || !fmt) {
        return -1;
    }

    while (fmt[i] != '\0' && off + 2u < sizeof(out)) {
        if (fmt[i] != '%') {
            out[off++] = fmt[i++];
            continue;
        }

        ++i;
        if (fmt[i] == '%') {
            out[off++] = '%';
            ++i;
            continue;
        }
        if (fmt[i] == 'l' && fmt[i + 1u] == 'u') {
            unsigned long value = va_arg(ap, unsigned long);
            slop_write_u32_dec(out, &off, (unsigned int)value);
            i += 2u;
            continue;
        }
        switch (fmt[i]) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                unsigned int j = 0u;
                if (!s) {
                    s = "(null)";
                }
                while (s[j] != '\0' && off + 1u < sizeof(out)) {
                    out[off++] = s[j++];
                }
                ++i;
                break;
            }
            case 'd':
                slop_write_s32_dec(out, &off, va_arg(ap, int));
                ++i;
                break;
            case 'u':
                slop_write_u32_dec(out, &off, va_arg(ap, unsigned int));
                ++i;
                break;
            case 'c':
                out[off++] = (char)va_arg(ap, int);
                ++i;
                break;
            default:
                out[off++] = '%';
                if (fmt[i] != '\0' && off + 1u < sizeof(out)) {
                    out[off++] = fmt[i++];
                }
                break;
        }
    }

    out[off] = '\0';
    return slop_write_all(fp->fd, out, off) == 0 ? (int)off : -1;
}

int slop_fprintf(SLOP_FILE *fp, const char *fmt, ...) {
    va_list ap;
    int rc;
    va_start(ap, fmt);
    rc = slop_vfprintf(fp, fmt, ap);
    va_end(ap);
    return rc;
}

int slop_printf(const char *fmt, ...) {
    va_list ap;
    int rc;
    va_start(ap, fmt);
    rc = slop_vfprintf(slop_stdout, fmt, ap);
    va_end(ap);
    return rc;
}
