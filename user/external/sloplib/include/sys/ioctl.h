#ifndef SLOP_COMPAT_SYS_IOCTL_H
#define SLOP_COMPAT_SYS_IOCTL_H

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TIOCGWINSZ 0x5413

static inline int ioctl(int fd, unsigned long request, char *argp) {
    (void)fd;
    (void)request;
    (void)argp;
    return -1;
}

#endif
