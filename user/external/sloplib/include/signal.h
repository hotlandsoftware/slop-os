#ifndef SLOP_COMPAT_SIGNAL_H
#define SLOP_COMPAT_SIGNAL_H

typedef unsigned int sigset_t;

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

#define SA_RESTART 0
#define SIG_IGN ((void (*)(int))1)
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGPIPE 13
#define SIGWINCH 28
#define SIG_UNBLOCK 0

static inline int sigemptyset(sigset_t *set) { if (set) *set = 0u; return 0; }
static inline int sigaddset(sigset_t *set, int signum) { (void)set; (void)signum; return 0; }
static inline int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    (void)how; (void)set; (void)oldset; return 0;
}
static inline int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    (void)signum; (void)act; (void)oldact; return 0;
}

#endif
