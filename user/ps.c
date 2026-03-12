#include "sys.h"

int main(void) {
    return sys_ps() < 0 ? 1 : 0;
}
