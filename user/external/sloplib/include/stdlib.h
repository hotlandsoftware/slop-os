#ifndef SLOP_COMPAT_STDLIB_H
#define SLOP_COMPAT_STDLIB_H

void *malloc(unsigned int size);
void *realloc(void *ptr, unsigned int size);
void free(void *ptr);
long strtol(const char *nptr, char **endptr, int base);
const char *getenv(const char *name);
void exit(int code);
void abort(void);
int system(const char *cmd);

#endif
