#include "kernel.h"

#define MAX_MOUNTS 8

static struct mount_entry mounts[MAX_MOUNTS];
static u32 mounts_used = 0;

static int is_known_fs(const char *fs_name) {
    return str_eq(fs_name, "memfs") || str_eq(fs_name, "iso9660");
}

static void mounts_reset(void) {
    u32 i;
    mounts_used = 0;
    for (i = 0; i < MAX_MOUNTS; ++i) {
        mounts[i].used = 0;
        mounts[i].path[0] = '\0';
        mounts[i].fs_name[0] = '\0';
        mounts[i].device_id = -1;
    }
}

int fs_mount(const char *path, const char *fs_name, int device_id) {
    struct mount_entry *entry;

    if (!path || !fs_name || mounts_used >= MAX_MOUNTS || !is_known_fs(fs_name)) {
        return 0;
    }
    if (path[0] != '/') {
        return 0;
    }
    if (!str_eq(fs_name, "memfs") && !storage_get_device(device_id)) {
        return 0;
    }

    entry = &mounts[mounts_used];
    entry->used = 1;
    str_copy(entry->path, path, sizeof(entry->path));
    str_copy(entry->fs_name, fs_name, sizeof(entry->fs_name));
    entry->device_id = device_id;
    ++mounts_used;
    return 1;
}

const struct mount_entry *fs_mounts(void) {
    return mounts;
}

u32 fs_mount_count(void) {
    return mounts_used;
}

void fs_print_mounts(enum console_target target) {
    u32 i;

    if (mounts_used == 0u) {
        console_print(target, "(no mounts)\n");
        return;
    }

    for (i = 0; i < mounts_used; ++i) {
        const struct mount_entry *m = &mounts[i];
        console_printf(target, "%s on %s", m->fs_name, m->path);
        if (m->device_id >= 0) {
            const struct block_device *dev = storage_get_device(m->device_id);
            console_print(target, " dev=");
            if (dev) {
                console_print(target, dev->name);
            } else {
                console_print(target, "unknown");
            }
        }
        console_print(target, "\n");
    }
}

void fs_init(void) {
    const struct block_device *cd0;

    mounts_reset();
    fs_mount("/", "memfs", -1);

    cd0 = storage_find_device("cd0");
    if (cd0) {
        fs_mount("/cdrom", "iso9660", cd0->id);
    }
}
