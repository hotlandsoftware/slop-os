#include "slop_alloc.h"

#define SLOP_ALLOC_ARENA_SIZE (32u * 1024u)

struct slop_alloc_hdr {
    unsigned int size;
};

static unsigned char slop_alloc_arena[SLOP_ALLOC_ARENA_SIZE];
static unsigned int slop_alloc_used = 0u;

static unsigned int slop_align_up(unsigned int value) {
    return (value + 7u) & ~7u;
}

static void slop_memcpy(unsigned char *dst, const unsigned char *src, unsigned int len) {
    unsigned int i;
    for (i = 0u; i < len; ++i) {
        dst[i] = src[i];
    }
}

void *slop_malloc(unsigned int size) {
    struct slop_alloc_hdr *hdr;
    unsigned int total;
    unsigned int start;

    if (size == 0u) {
        return (void *)0;
    }

    total = slop_align_up((unsigned int)sizeof(struct slop_alloc_hdr) + size);
    start = slop_align_up(slop_alloc_used);
    if (start + total > SLOP_ALLOC_ARENA_SIZE) {
        return (void *)0;
    }

    hdr = (struct slop_alloc_hdr *)(void *)(slop_alloc_arena + start);
    hdr->size = size;
    slop_alloc_used = start + total;
    return (void *)(hdr + 1);
}

void *slop_realloc(void *ptr, unsigned int size) {
    struct slop_alloc_hdr *hdr;
    void *new_ptr;
    unsigned int copy_size;

    if (!ptr) {
        return slop_malloc(size);
    }
    if (size == 0u) {
        slop_free(ptr);
        return (void *)0;
    }

    hdr = ((struct slop_alloc_hdr *)ptr) - 1;
    if (hdr->size >= size) {
        return ptr;
    }

    new_ptr = slop_malloc(size);
    if (!new_ptr) {
        return (void *)0;
    }

    copy_size = hdr->size;
    if (copy_size > size) {
        copy_size = size;
    }
    slop_memcpy((unsigned char *)new_ptr, (const unsigned char *)ptr, copy_size);
    return new_ptr;
}

void slop_free(void *ptr) {
    (void)ptr;
}
