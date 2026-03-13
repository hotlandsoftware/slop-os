#ifndef SLOP_COMPAT_SETJMP_H
#define SLOP_COMPAT_SETJMP_H

typedef int jmp_buf[1];

#define setjmp(env) (0)
#define longjmp(env, val) ((void)(env), (void)(val))

#endif
