#include "sys.h"

int main(void) {
    return sys_systeminfo() < 0 ? 1 : 0;
}
