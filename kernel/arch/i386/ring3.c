#include "kernel.h"

static u8 user_stack[4096];
extern void ring3_user_stub(void);

int ring3_test(enum console_target target) {
    u32 user_sp = (u32)&user_stack[sizeof(user_stack) - 16u];

    console_print(target, "ring3: enter test\n");
    (void)enter_user_mode((u32)ring3_user_stub, user_sp);
    console_print(target, "ring3: returned to kernel\n");
    return 1;
}
