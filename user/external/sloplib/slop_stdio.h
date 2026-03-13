#ifndef SLOP_STDIO_H
#define SLOP_STDIO_H

#include <stdarg.h>

typedef struct slop_file {
    int fd;
    int kind;
    int eof;
    int err;
    int owned;
    unsigned int pos;
    unsigned int size;
    unsigned int capacity;
    char *mem;
} SLOP_FILE;

extern SLOP_FILE slop_stdin_file;
extern SLOP_FILE slop_stdout_file;
extern SLOP_FILE slop_stderr_file;

#define slop_stdin  (&slop_stdin_file)
#define slop_stdout (&slop_stdout_file)
#define slop_stderr (&slop_stderr_file)

SLOP_FILE *slop_fopen(const char *path, const char *mode);
int slop_fclose(SLOP_FILE *fp);
int slop_fflush(SLOP_FILE *fp);
int slop_setvbuf(SLOP_FILE *fp, char *buf, int mode, unsigned int size);
int slop_fgetc(SLOP_FILE *fp);
int slop_getchar(void);
int slop_fputc(int ch, SLOP_FILE *fp);
int slop_putchar(int ch);
unsigned int slop_fread(void *ptr, unsigned int size, unsigned int nmemb, SLOP_FILE *fp);
unsigned int slop_fwrite(const void *ptr, unsigned int size, unsigned int nmemb, SLOP_FILE *fp);
int slop_fputs(const char *s, SLOP_FILE *fp);
int slop_puts(const char *s);
char *slop_fgets(char *buf, int size, SLOP_FILE *fp);
int slop_fseek(SLOP_FILE *fp, long offset, int whence);
long slop_ftell(SLOP_FILE *fp);
int slop_feof(SLOP_FILE *fp);
int slop_ferror(SLOP_FILE *fp);
void slop_clearerr(SLOP_FILE *fp);
SLOP_FILE *slop_tmpfile(void);
int slop_vfprintf(SLOP_FILE *fp, const char *fmt, va_list ap);
int slop_fprintf(SLOP_FILE *fp, const char *fmt, ...);
int slop_printf(const char *fmt, ...);

#endif
