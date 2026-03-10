#include "kernel.h"

struct heap_state {
    u32 start;
    u32 current;
    u32 limit;
    int ready;
};

static struct heap_state heap;

static u32 align_up(u32 value, u32 alignment) {
    u32 mask = alignment - 1u;
    return (value + mask) & ~mask;
}

void heap_init(const struct multiboot_info *mbi, u32 magic) {
    u32 heap_start = align_up((u32)&_kernel_end, 16u);
    u32 heap_limit = heap_start;

    heap.start = heap_start;
    heap.current = heap_start;
    heap.limit = heap_start;
    heap.ready = 0;

    if (magic == MULTIBOOT_MAGIC && mbi && (mbi->flags & 0x1u)) {
        heap_limit = 0x00100000u + (mbi->mem_upper * 1024u);
        heap_limit = align_up(heap_limit, 16u);
        if (heap_limit > heap_start) {
            heap.limit = heap_limit;
            heap.ready = 1;
        }
    }
}

void *kmalloc(size_t size) {
    u32 start;
    u32 end;

    if (!heap.ready || size == 0u) {
        return (void *)0;
    }

    start = align_up(heap.current, 16u);
    end = start + (u32)size;
    if (end < start || end > heap.limit) {
        return (void *)0;
    }

    heap.current = align_up(end, 16u);
    return (void *)start;
}

void *kcalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        mem_zero(ptr, size);
    }
    return ptr;
}

u32 heap_bytes_total(void) {
    return heap.limit - heap.start;
}

u32 heap_bytes_used(void) {
    return heap.current - heap.start;
}

u32 heap_bytes_free(void) {
    return heap.limit - heap.current;
}

int heap_is_ready(void) {
    return heap.ready;
}
