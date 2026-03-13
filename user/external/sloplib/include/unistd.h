#ifndef SLOP_COMPAT_UNISTD_H
#define SLOP_COMPAT_UNISTD_H

#include "../../sys.h"
#include <sys/stat.h>

#define _PC_PATH_MAX 1

int isatty(int fd);
int getpid(void);
long pathconf(const char *path, int name);
int unlink(const char *path);
int remove(const char *path);

#endif
