#pragma once

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>

#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define ALIGN_TO_PAGE_SIZE(x) \
(((x) + HUGE_PAGE_SIZE - 1) / HUGE_PAGE_SIZE * HUGE_PAGE_SIZE)


static void *huge_malloc(size_t size) {

    // return malloc(size);
    size_t real_size = ALIGN_TO_PAGE_SIZE(size);
    void *ptr = mmap(NULL, real_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB, -1, 0);
    
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    return ptr;
}

static void huge_free(void *ptr, size_t size) {
    // free(ptr);
    // return ;
    size_t real_size = ALIGN_TO_PAGE_SIZE(size);
    if (ptr == NULL) {
        return;
    }
    munmap(ptr, real_size);
}
