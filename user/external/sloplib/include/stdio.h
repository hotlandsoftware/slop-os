#ifndef SLOP_COMPAT_STDIO_H
#define SLOP_COMPAT_STDIO_H

#include "../slop_stdio.h"

typedef SLOP_FILE FILE;

#define stdin slop_stdin
#define stdout slop_stdout
#define stderr slop_stderr

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IONBF 0

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *fp);
int fflush(FILE *fp);
int setvbuf(FILE *fp, char *buf, int mode, unsigned int size);
int fgetc(FILE *fp);
int getc(FILE *fp);
int getchar(void);
int fputc(int ch, FILE *fp);
int putc(int ch, FILE *fp);
int putchar(int ch);
unsigned int fread(void *ptr, unsigned int size, unsigned int nmemb, FILE *fp);
unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb, FILE *fp);
int fputs(const char *s, FILE *fp);
int puts(const char *s);
char *fgets(char *buf, int size, FILE *fp);
int fseek(FILE *fp, long offset, int whence);
long ftell(FILE *fp);
int feof(FILE *fp);
int ferror(FILE *fp);
void clearerr(FILE *fp);
FILE *tmpfile(void);
FILE *popen(const char *command, const char *mode);
int pclose(FILE *fp);
int printf(const char *fmt, ...);
int fprintf(FILE *fp, const char *fmt, ...);
int snprintf(char *buf, unsigned int size, const char *fmt, ...);
int vsnprintf(char *buf, unsigned int size, const char *fmt, __builtin_va_list ap);

#endif
