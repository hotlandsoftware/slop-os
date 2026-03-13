#include "include/stdio.h"

FILE *fopen(const char *path, const char *mode) { return slop_fopen(path, mode); }
int fclose(FILE *fp) { return slop_fclose(fp); }
int fflush(FILE *fp) { return slop_fflush(fp); }
int setvbuf(FILE *fp, char *buf, int mode, unsigned int size) { return slop_setvbuf(fp, buf, mode, size); }
int fgetc(FILE *fp) { return slop_fgetc(fp); }
int getc(FILE *fp) { return slop_fgetc(fp); }
int getchar(void) { return slop_getchar(); }
int fputc(int ch, FILE *fp) { return slop_fputc(ch, fp); }
int putc(int ch, FILE *fp) { return slop_fputc(ch, fp); }
int putchar(int ch) { return slop_putchar(ch); }
unsigned int fread(void *ptr, unsigned int size, unsigned int nmemb, FILE *fp) { return slop_fread(ptr, size, nmemb, fp); }
unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb, FILE *fp) { return slop_fwrite(ptr, size, nmemb, fp); }
int fputs(const char *s, FILE *fp) { return slop_fputs(s, fp); }
int puts(const char *s) { return slop_puts(s); }
char *fgets(char *buf, int size, FILE *fp) { return slop_fgets(buf, size, fp); }
int fseek(FILE *fp, long offset, int whence) { return slop_fseek(fp, offset, whence); }
long ftell(FILE *fp) { return slop_ftell(fp); }
int feof(FILE *fp) { return slop_feof(fp); }
int ferror(FILE *fp) { return slop_ferror(fp); }
void clearerr(FILE *fp) { slop_clearerr(fp); }
FILE *tmpfile(void) { return slop_tmpfile(); }
FILE *popen(const char *command, const char *mode) {
    (void)command;
    (void)mode;
    return (FILE *)0;
}
int pclose(FILE *fp) {
    (void)fp;
    return -1;
}

int printf(const char *fmt, ...) {
    __builtin_va_list ap;
    int rc;
    __builtin_va_start(ap, fmt);
    rc = slop_vfprintf(slop_stdout, fmt, ap);
    __builtin_va_end(ap);
    return rc;
}

int fprintf(FILE *fp, const char *fmt, ...) {
    __builtin_va_list ap;
    int rc;
    __builtin_va_start(ap, fmt);
    rc = slop_vfprintf(fp, fmt, ap);
    __builtin_va_end(ap);
    return rc;
}

static int append_char(char *buf, unsigned int size, unsigned int *off, char ch) {
    if (*off + 1u < size) {
        buf[*off] = ch;
    }
    ++*off;
    return 0;
}

static void append_u32(char *buf, unsigned int size, unsigned int *off, unsigned int value) {
    char tmp[16];
    unsigned int n = 0u;
    if (value == 0u) {
        append_char(buf, size, off, '0');
        return;
    }
    while (value > 0u && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (n > 0u) {
        append_char(buf, size, off, tmp[--n]);
    }
}

static void append_s32(char *buf, unsigned int size, unsigned int *off, int value) {
    unsigned int mag;
    if (value < 0) {
        append_char(buf, size, off, '-');
        mag = (unsigned int)(-(value + 1)) + 1u;
    } else {
        mag = (unsigned int)value;
    }
    append_u32(buf, size, off, mag);
}

int vsnprintf(char *buf, unsigned int size, const char *fmt, __builtin_va_list ap) {
    unsigned int off = 0u;
    unsigned int i = 0u;
    if (!buf || size == 0u || !fmt) {
        return -1;
    }
    while (fmt[i] != '\0') {
        if (fmt[i] != '%') {
            append_char(buf, size, &off, fmt[i++]);
            continue;
        }
        ++i;
        if (fmt[i] == '%') {
            append_char(buf, size, &off, '%');
            ++i;
            continue;
        }
        if (fmt[i] == 'l' && fmt[i + 1u] == 'u') {
            append_u32(buf, size, &off, (unsigned int)__builtin_va_arg(ap, unsigned long));
            i += 2u;
            continue;
        }
        switch (fmt[i]) {
            case 's': {
                const char *s = __builtin_va_arg(ap, const char *);
                unsigned int j = 0u;
                if (!s) s = "(null)";
                while (s[j] != '\0') append_char(buf, size, &off, s[j++]);
                ++i;
                break;
            }
            case 'd':
                append_s32(buf, size, &off, __builtin_va_arg(ap, int));
                ++i;
                break;
            case 'u':
                append_u32(buf, size, &off, __builtin_va_arg(ap, unsigned int));
                ++i;
                break;
            case 'c':
                append_char(buf, size, &off, (char)__builtin_va_arg(ap, int));
                ++i;
                break;
            default:
                append_char(buf, size, &off, '%');
                if (fmt[i] != '\0') append_char(buf, size, &off, fmt[i++]);
                break;
        }
    }
    if (size > 0u) {
        buf[(off < size) ? off : (size - 1u)] = '\0';
    }
    return (int)off;
}

int snprintf(char *buf, unsigned int size, const char *fmt, ...) {
    __builtin_va_list ap;
    int rc;
    __builtin_va_start(ap, fmt);
    rc = vsnprintf(buf, size, fmt, ap);
    __builtin_va_end(ap);
    return rc;
}
