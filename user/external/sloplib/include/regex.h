#ifndef SLOP_COMPAT_REGEX_H
#define SLOP_COMPAT_REGEX_H

#include <stddef.h>

typedef struct regex_t {
    unsigned int re_nsub;
} regex_t;

typedef struct regmatch_t {
    int rm_so;
    int rm_eo;
} regmatch_t;

#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NOTBOL 4

int regcomp(regex_t *preg, const char *regex, int cflags);
int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);

#endif
