#include "kernel.h"

#define MAX_SERVICES 16
#define SERVICE_NAME_MAX 15

struct service_entry {
    int used;
    int pid;
    char name[SERVICE_NAME_MAX + 1];
};

static struct service_entry services[MAX_SERVICES];

void service_init(void) {
    u32 i;
    for (i = 0; i < MAX_SERVICES; ++i) {
        services[i].used = 0;
        services[i].pid = -1;
        services[i].name[0] = '\0';
    }
}

int service_register(const char *name, int pid) {
    u32 i;
    int free_slot = -1;

    if (!name || name[0] == '\0' || pid < 0 || proc_state_of(pid) == PROC_UNUSED) {
        return 0;
    }

    for (i = 0; name[i] != '\0'; ++i) {
        char c = name[i];
        if (i >= SERVICE_NAME_MAX) {
            return 0;
        }
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '-' || c == '_')) {
            return 0;
        }
    }

    for (i = 0; i < MAX_SERVICES; ++i) {
        if (services[i].used) {
            if (str_eq(services[i].name, name)) {
                services[i].pid = pid;
                return 1;
            }
        } else if (free_slot < 0) {
            free_slot = (int)i;
        }
    }

    if (free_slot < 0) {
        return 0;
    }

    services[free_slot].used = 1;
    services[free_slot].pid = pid;
    str_copy(services[free_slot].name, name, sizeof(services[free_slot].name));
    return 1;
}

int service_lookup(const char *name) {
    u32 i;

    if (!name || name[0] == '\0') {
        return -1;
    }

    for (i = 0; i < MAX_SERVICES; ++i) {
        if (!services[i].used) {
            continue;
        }
        if (proc_state_of(services[i].pid) == PROC_UNUSED) {
            services[i].used = 0;
            services[i].pid = -1;
            services[i].name[0] = '\0';
            continue;
        }
        if (str_eq(services[i].name, name)) {
            return services[i].pid;
        }
    }

    return -1;
}
