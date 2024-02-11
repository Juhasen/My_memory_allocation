//
// Created by krystian on 14.10.23.
//

#ifndef PROJEKT_ALOKATOR_PAMICI_HEAP_H
#define PROJEKT_ALOKATOR_PAMICI_HEAP_H

#include<stdio.h>
#include<string.h>

enum pointer_type_t {
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

enum pointer_type_t get_pointer_type(const void *const pointer);

int heap_setup(void);

void heap_clean(void);

void *heap_malloc(size_t size);

void *heap_calloc(size_t number, size_t size);

void *heap_realloc(void *memblock, size_t count);

void heap_free(void *memblock);

size_t heap_get_largest_used_block_size(void);

int heap_validate(void);

struct block_t *findFreeBlock(size_t size);

int heap_setup_validate(void);

int heap_fence_validate(void);

int heap_block_validate(void);

#endif //PROJEKT_ALOKATOR_PAMICI_HEAP_H
