#ifndef SLOP_COMPAT_SYS_STAT_H
#define SLOP_COMPAT_SYS_STAT_H

struct stat {
    unsigned int st_mode;
    unsigned int st_size;
};

#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFREG 0100000

#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)

int stat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);

#endif
