#include "../../sys.h"
#include "include/errno.h"
#include "include/unistd.h"

int isatty(int fd) {
    struct stat st;
    if (fstat(fd, &st) < 0) {
        return 0;
    }
    return S_ISCHR(st.st_mode);
}

int getpid(void) {
    return sys_getpid();
}

long pathconf(const char *path, int name) {
    (void)path;
    if (name == _PC_PATH_MAX) {
        return 256;
    }
    errno = EINVAL;
    return -1;
}

int stat(const char *path, struct stat *st) {
    struct user_stat ust;
    if (!path || !st) {
        errno = EINVAL;
        return -1;
    }
    if (sys_stat(path, &ust) < 0) {
        errno = ENOENT;
        return -1;
    }
    st->st_mode = ust.st_mode;
    st->st_size = ust.st_size;
    return 0;
}

int fstat(int fd, struct stat *st) {
    struct user_stat ust;
    if (!st) {
        errno = EINVAL;
        return -1;
    }
    if (sys_fstat(fd, &ust) < 0) {
        errno = EINVAL;
        return -1;
    }
    st->st_mode = ust.st_mode;
    st->st_size = ust.st_size;
    return 0;
}

int unlink(const char *path) {
    (void)path;
    errno = EINVAL;
    return -1;
}

int remove(const char *path) {
    return unlink(path);
}
