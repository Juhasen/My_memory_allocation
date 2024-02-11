//
// Created by krystian on 14.10.23.
//

#include "heap.h"
#include "tested_declarations.h"
#include "rdebug.h"

#define fenceSize 2

#define fence '#'

#define BLOCK_SIZE sizeof(struct block_t)

struct heap_t {
    void *memory;
    size_t size;
    size_t numberOfBlocks;
    struct block_t *head;
} memoryManager;

struct block_t {
    size_t size;
    struct block_t *next;
    struct block_t *prev;
    int valid;
};

int heap_setup(void) {
    void *memory = custom_sbrk(0);
    if (memory == (void *) -1) return -1;
    memoryManager.memory = memory;
    memoryManager.size = 0;
    memoryManager.head = NULL;
    memoryManager.numberOfBlocks = 0;
    return 0;
}

void heap_clean(void) {
    custom_sbrk(-(intptr_t) memoryManager.size);
    memoryManager.size = 0;
    memoryManager.memory = NULL;
    memoryManager.head = NULL;
    memoryManager.numberOfBlocks = 0;
}

size_t blockSizeDifference(struct block_t *block) {
    if (block->next == NULL) return 0;
    size_t difference = ((char *) block->next - (char *) block) - BLOCK_SIZE - block->size - fenceSize * 2;
    return difference;
}

enum pointer_type_t get_pointer_type(const void *const pointer) {
    if (pointer == NULL) return pointer_null;
    if (heap_validate() > 0) return pointer_heap_corrupted;
    if (pointer < memoryManager.memory) return pointer_unallocated;

    struct block_t *memoryBlock = memoryManager.head;

    char *ptr = memoryManager.memory;

    size_t movePtr = (char *) memoryManager.head - (char *) memoryManager.memory;

    size_t moveToNextBlock = 0;
    int found = 0;
    while (memoryBlock != NULL) {
        moveToNextBlock = memoryBlock->size + BLOCK_SIZE + fenceSize * 2 + blockSizeDifference(memoryBlock);
        movePtr += moveToNextBlock;
        if ((void *) (ptr + movePtr) > pointer) {
            found = 1;
            break;
        }
        memoryBlock = memoryBlock->next;
    }

    if (found == 0) {
        return pointer_unallocated;
    } else {
        movePtr -= moveToNextBlock;

        char *currentLocation = ptr + movePtr;

        if (currentLocation + BLOCK_SIZE + fenceSize == pointer) {
            return pointer_valid;
        }
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            if (currentLocation + i == pointer) {
                return pointer_control_block;
            }
        }
        for (size_t i = 0; i < fenceSize; ++i) {
            if (currentLocation + BLOCK_SIZE + i == pointer ||
                currentLocation + BLOCK_SIZE + fenceSize + memoryBlock->size + i == pointer) {
                return pointer_inside_fences;

            }
        }
        for (size_t i = 1; i < memoryBlock->size; ++i) {
            if (currentLocation + BLOCK_SIZE + fenceSize + i == pointer) {
                return pointer_inside_data_block;
            }
        }
    }
    return pointer_unallocated;
}

void createFences(struct block_t *block) {
    char *ptr = (char *) block;
    ptr += BLOCK_SIZE;
    for (int i = 0; i < fenceSize; i++) {
        *(ptr + i) = fence;
    }
    ptr += fenceSize + block->size;
    for (int i = 0; i < fenceSize; i++) {
        *(ptr + i) = fence;
    }
}

struct block_t *findFreeBlock(size_t neededSize) {
    neededSize += fenceSize * 2 + BLOCK_SIZE;
    struct block_t *memoryBlock = memoryManager.head;

    if (memoryManager.numberOfBlocks == 1) {
        size_t freeSpace = (intptr_t) memoryManager.head - (intptr_t) memoryManager.memory;
        if (freeSpace >= neededSize) {
            return memoryBlock;
        }
    }

    while (memoryBlock->next != NULL) {
        size_t freeSpace = ((char *) memoryBlock->next - (char *) memoryBlock) - BLOCK_SIZE - memoryBlock->size - fenceSize * 2;
        if (freeSpace >= neededSize) {
            return memoryBlock;
        }
        memoryBlock = memoryBlock->next;
    }

    return NULL;
}

void addSizeNumBlocks(size_t neededSpaceForBlock) {
    memoryManager.size += neededSpaceForBlock;
    memoryManager.numberOfBlocks++;
}

int calculateValid(struct block_t *memoryBlock) {
    if (memoryBlock == NULL) return -123456789;
    int valid = 0;
    char *ptr = (char *) memoryBlock;
    for (size_t i = 0; i < 24; ++i) {
        valid += *(ptr + i);
    }
    for (int i = 0; i < 4; ++i) {
        valid += *(ptr + 28 + i);
    }
    return valid;
}

void setBlockValid(struct block_t *memoryBlock) {
    if (memoryBlock == NULL) return;
    memoryBlock->valid = calculateValid(memoryBlock);
}

void *heap_malloc(size_t size) {
    if (size == 0 || heap_validate() > 0)return NULL;
    size_t neededSpaceForBlock = BLOCK_SIZE + size + fenceSize * 2;
    void *ptr = custom_sbrk((intptr_t) neededSpaceForBlock);
    if (ptr == (void *) -1) return NULL;

    if (memoryManager.head == NULL) {
        memoryManager.head = (struct block_t *) memoryManager.memory;
        memoryManager.head->prev = NULL;
        memoryManager.head->next = NULL;
        memoryManager.head->size = size;
        createFences(memoryManager.head);
        addSizeNumBlocks(neededSpaceForBlock);
        setBlockValid(memoryManager.head);
        return (char *) memoryManager.head + sizeof(struct block_t) + fenceSize;
    }

    struct block_t *freeBlock = findFreeBlock(size);
    if (freeBlock != NULL) {
        struct block_t *newBlock = (struct block_t *) ((char *) freeBlock + BLOCK_SIZE + freeBlock->size + fenceSize * 2);
        if (memoryManager.numberOfBlocks == 1) {
            newBlock = memoryManager.memory;
            newBlock->prev = NULL;
            newBlock->next = memoryManager.head;
            newBlock->next->prev = newBlock;
            memoryManager.head = newBlock;
            newBlock->size = size;
            createFences(newBlock);
            addSizeNumBlocks(neededSpaceForBlock);
            setBlockValid(newBlock);
            setBlockValid(newBlock->next);
            setBlockValid(newBlock->prev);
            return (char *) newBlock + sizeof(struct block_t) + fenceSize;
        }
        newBlock->prev = freeBlock;
        newBlock->next = freeBlock->next;
        if (freeBlock->next != NULL) {
            freeBlock->next->prev = newBlock;
        }
        freeBlock->next = newBlock;
        newBlock->size = size;
        createFences(newBlock);
        addSizeNumBlocks(neededSpaceForBlock);
        setBlockValid(newBlock);
        setBlockValid(newBlock->next);
        setBlockValid(newBlock->prev);
        return (char *) newBlock + sizeof(struct block_t) + fenceSize;
    }

    struct block_t *currentBlock = memoryManager.head;
    while (currentBlock->next != NULL) {
        currentBlock = currentBlock->next;
    }
    struct block_t *newBlock = (struct block_t *) ((char *) currentBlock + BLOCK_SIZE + currentBlock->size + fenceSize * 2);
    newBlock->prev = currentBlock;
    newBlock->next = NULL;
    newBlock->size = size;
    currentBlock->next = newBlock;
    createFences(newBlock);
    addSizeNumBlocks(neededSpaceForBlock);
    setBlockValid(newBlock);
    setBlockValid(newBlock->next);
    setBlockValid(newBlock->prev);
    return (char *) newBlock + sizeof(struct block_t) + fenceSize;
}

void *heap_calloc(size_t number, size_t size) {
    if (number == 0 || size == 0 || heap_validate() > 0) return NULL;
    size_t sizeMalloc = number * size;
    void *ptr = heap_malloc(sizeMalloc);
    if (ptr == NULL) return NULL;
    memset(ptr, 0, sizeMalloc);
    return ptr;
}

struct block_t *findBlock(void *memblock) {
    struct block_t *memoryBlock = memoryManager.head;
    while (memoryBlock != NULL) {
        if ((char *) memoryBlock + sizeof(struct block_t) + fenceSize == memblock) {
            return memoryBlock;
        }
        memoryBlock = memoryBlock->next;
    }
    return NULL;
}

void *heap_realloc(void *memblock, size_t count) {
    if (heap_validate() > 0) return NULL;
    if (memblock != NULL && count == 0) {
        heap_free(memblock);
        return NULL;
    }
    if (memblock == NULL && count > 0) {
        return heap_malloc(count);
    }
    if (memblock == NULL && count == 0) {
        return NULL;
    }
    struct block_t *memoryBlock = memoryManager.head;
    if (memoryBlock == NULL) {
        return NULL;
    }

    memoryBlock = findBlock(memblock);
    if (memoryBlock == NULL) {
        return NULL;
    }

    if (memoryBlock->next == NULL) {
        void *ptr = custom_sbrk((intptr_t) (count - memoryBlock->size));
        if (ptr == (void *) -1) return NULL;
        memoryManager.size += count - memoryBlock->size;
        memoryBlock->size = count;
        createFences(memoryBlock);
        setBlockValid(memoryBlock);
        return memblock;
    }
    if (memoryBlock->size >= count || blockSizeDifference(memoryBlock) >= count - memoryBlock->size) {
        memoryBlock->size = count;
        createFences(memoryBlock);
        setBlockValid(memoryBlock);
        return memblock;
    }

    void *newPtr = heap_malloc(count);
    if (newPtr == NULL) return NULL;
    memcpy(newPtr, memblock, memoryBlock->size);
    heap_free(memblock);
    return newPtr;
}

void heap_free(void *memblock) {
    if (memblock == NULL || heap_validate() > 0) return;
    struct block_t *memoryBlock = memoryManager.head;
    if (memoryBlock == NULL) {
        return;
    }
    memoryBlock = findBlock(memblock);
    if (memoryBlock != NULL) {
        memoryManager.numberOfBlocks--;
        if (memoryBlock->prev == NULL && memoryBlock->next == NULL) {
            memoryManager.head = NULL;
        } else if (memoryBlock->prev == NULL) {
            memoryManager.head = memoryBlock->next;
            memoryManager.head->prev = NULL;
        } else if (memoryBlock->next == NULL) {
            memoryBlock->prev->next = NULL;
        } else {
            memoryBlock->prev->next = memoryBlock->next;
            memoryBlock->next->prev = memoryBlock->prev;
        }
        setBlockValid(memoryBlock->next);
        setBlockValid(memoryBlock->prev);
    }
}

size_t heap_get_largest_used_block_size(void) {
    if (heap_validate() > 0) return 0;
    struct block_t *memoryBlock = memoryManager.head;
    size_t max = 0;
    while (memoryBlock != NULL) {
        if (memoryBlock->size > max) {
            max = memoryBlock->size;
        }
        memoryBlock = memoryBlock->next;
    }
    return max;
}

int checkFenceForBlock(struct block_t *block) {
    char *ptr = (char *) block;
    ptr += BLOCK_SIZE;
    for (int i = 0; i < fenceSize; i++) {
        if (*(ptr + i) != fence) {
            return 1;
        }
    }
    ptr += fenceSize + block->size;
    for (int i = 0; i < fenceSize; i++) {
        if (*(ptr + i) != fence) {
            return 1;
        }
    }
    return 0;
}

int heap_setup_validate(void) {
    if (memoryManager.memory == NULL) return 1;
    return 0;
}

int heap_fence_validate(void) {
    struct block_t *block = memoryManager.head;
    while (block != NULL) {
        if (checkFenceForBlock(block) > 0) {
            return 1;
        }
        block = block->next;
    }
    return 0;
}

int heap_block_validate(void) {
    struct block_t *memoryBlock = memoryManager.head;
    if (memoryBlock == NULL) return 0;
    for (size_t i = 0; i < memoryManager.numberOfBlocks; ++i) {
        if (memoryBlock == NULL) {
            return 3;
        }
        if (calculateValid(memoryBlock) != memoryBlock->valid) {
            return 3;
        }
        memoryBlock = memoryBlock->next;
    }
    return 0;
}

int heap_validate(void) {
    if (heap_setup_validate() > 0) return 2;
    if (heap_block_validate() > 0) return 3;
    if (heap_fence_validate() > 0) return 1;
    return 0;
}


