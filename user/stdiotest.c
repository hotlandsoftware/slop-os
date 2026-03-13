#include "external/sloplib/slop_stdio.h"

int main(int argc, char **argv) {
    SLOP_FILE *fp;
    char buf[64];

    if (argc < 3) {
        slop_puts("stdiotest: usage stdiotest PATH TEXT");
        return 1;
    }

    fp = slop_fopen(argv[1], "w");
    if (!fp) {
        slop_puts("stdiotest: fopen(w) failed");
        return 1;
    }
    if (slop_fprintf(fp, "value=%s\n", argv[2]) < 0) {
        slop_puts("stdiotest: fprintf failed");
        (void)slop_fclose(fp);
        return 1;
    }
    if (slop_fclose(fp) < 0) {
        slop_puts("stdiotest: fclose failed");
        return 1;
    }

    fp = slop_fopen(argv[1], "r");
    if (!fp) {
        slop_puts("stdiotest: fopen(r) failed");
        return 1;
    }
    if (!slop_fgets(buf, (int)sizeof(buf), fp)) {
        slop_puts("stdiotest: fgets failed");
        (void)slop_fclose(fp);
        return 1;
    }
    (void)slop_fclose(fp);

    slop_printf("stdiotest: read back: %s", buf);
    return 0;
}
