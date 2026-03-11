#include "kernel.h"

static u8 user_stack[4096];
extern void ring3_user_stub(void);

int ring3_test(enum console_target target) {
    u32 user_sp = (u32)&user_stack[sizeof(user_stack) - 16u];
    u16 gate_sel = 0;
    u8 gate_attr = 0;
    u32 gate_off = 0;

    interrupts_get_gate80(&gate_sel, &gate_attr, &gate_off);

    console_print(target, "ring3: enter test\n");
    console_print(target, "ring3: user_stub=");
    console_print_hex_u32(target, (u32)ring3_user_stub);
    console_print(target, " gate80.off=");
    console_print_hex_u32(target, gate_off);
    console_print(target, " sel=");
    console_print_hex_u32(target, (u32)gate_sel);
    console_print(target, " attr=");
    console_print_hex_u32(target, (u32)gate_attr);
    console_putchar(target, '\n');
    (void)enter_user_mode((u32)ring3_user_stub, user_sp);
    console_print(target, "ring3: returned to kernel\n");
    return 1;
}
