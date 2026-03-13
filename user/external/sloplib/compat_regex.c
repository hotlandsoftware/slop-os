#include "include/regex.h"
#include "include/string.h"

int regcomp(regex_t *preg, const char *regex, int cflags) {
    (void)regex;
    (void)cflags;
    if (preg) {
        preg->re_nsub = 0u;
    }
    return 1;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags) {
    (void)preg;
    (void)string;
    (void)nmatch;
    (void)pmatch;
    (void)eflags;
    return 1;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    const char *msg = "regex unsupported";
    size_t len = strlen(msg);
    (void)errcode;
    (void)preg;
    if (errbuf && errbuf_size > 0u) {
        size_t i;
        for (i = 0u; i + 1u < errbuf_size && msg[i] != '\0'; ++i) {
            errbuf[i] = msg[i];
        }
        errbuf[i] = '\0';
    }
    return len;
}

void regfree(regex_t *preg) {
    (void)preg;
}
