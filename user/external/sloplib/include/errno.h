#ifndef SLOP_COMPAT_ERRNO_H
#define SLOP_COMPAT_ERRNO_H

extern int errno;

#define EPERM    1
#define ENOENT   2
#define EIO      5
#define ENOMEM   12
#define EACCES   13
#define EEXIST   17
#define EINVAL   22
#define ENFILE   23
#define EMFILE   24
#define ERANGE   34

#endif
