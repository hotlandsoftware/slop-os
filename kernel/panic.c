#include "kernel.h"

static void panic_header(void) {
    term_set_color(15, 4);
    term_clear();
    term_print("A problem has been detected and SLOP OS has been shut down\n");
    term_print("to prevent damage to your computer.\n\n");
}

void kernel_panic_exception(const struct interrupt_frame *frame) {
    panic_header();
    term_print("EXCEPTION DETAILS\n");
    term_print("  vector:  ");
    term_print_u32_dec(frame->vector);
    term_print("\n  error:   ");
    term_print_hex_u32(frame->error_code);
    term_print("\n  eip:     ");
    term_print_hex_u32(frame->eip);
    term_print("\n  cs:      ");
    term_print_hex_u32(frame->cs);
    term_print("\n  eflags:  ");
    term_print_hex_u32(frame->eflags);
    term_print("\n");
    halt_forever();
}

void kernel_panic_message(const char *message) {
    panic_header();
    term_print("EXCEPTION DETAILS\n");
    term_print("  manual panic requested");
    if (message && message[0] != '\0') {
        term_print(": ");
        term_print(message);
    }
    term_print("\n");
    halt_forever();
}
