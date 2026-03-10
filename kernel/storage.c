#include "kernel.h"

#define MAX_BLOCK_DEVICES 8

static struct block_device devices[MAX_BLOCK_DEVICES];
static u32 device_count = 0;

static void copy_name(char *dst, const char *src, u32 max_chars) {
    u32 i = 0;
    if (max_chars == 0u) {
        return;
    }
    while (src[i] && i + 1u < max_chars) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

int storage_register_device(const char *name, u32 sector_size, block_read_fn read, void *ctx) {
    struct block_device *dev;
    if (!name || !read || sector_size == 0u || device_count >= MAX_BLOCK_DEVICES) {
        return -1;
    }

    dev = &devices[device_count];
    dev->id = (int)device_count;
    copy_name(dev->name, name, sizeof(dev->name));
    dev->sector_size = sector_size;
    dev->read = read;
    dev->ctx = ctx;
    ++device_count;
    return dev->id;
}

const struct block_device *storage_get_device(int id) {
    if (id < 0 || (u32)id >= device_count) {
        return (const struct block_device *)0;
    }
    return &devices[id];
}

const struct block_device *storage_find_device(const char *name) {
    u32 i;
    for (i = 0; i < device_count; ++i) {
        if (str_eq(devices[i].name, name)) {
            return &devices[i];
        }
    }
    return (const struct block_device *)0;
}

int storage_read(int device_id, u32 lba, u32 count, void *out_buf) {
    const struct block_device *dev = storage_get_device(device_id);
    if (!dev || !out_buf || count == 0u) {
        return 0;
    }
    return dev->read(dev->ctx, lba, count, out_buf);
}

u32 storage_device_count(void) {
    return device_count;
}

void storage_init(void) {
    device_count = 0;
    atapi_probe_and_register();
}
