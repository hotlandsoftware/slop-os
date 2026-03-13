#ifndef SLOP_ALLOC_H
#define SLOP_ALLOC_H

void *slop_malloc(unsigned int size);
void *slop_realloc(void *ptr, unsigned int size);
void slop_free(void *ptr);

#endif
