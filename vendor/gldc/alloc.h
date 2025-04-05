#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


int alloc_init(void* pool, size_t size);
void alloc_shutdown();

void *alloc_malloc(void* ctx, size_t size, bool for_defrag = false);
void alloc_free(void* p);

typedef void (defrag_address_move)(void* src, void* dst, void* ctx, void* user_data);
void alloc_run_defrag(defrag_address_move callback, int max_iterations, void* user_data);

size_t alloc_count_free();
size_t alloc_count_continuous();

void* alloc_next_available(size_t required_size);
void* alloc_base_address();
size_t alloc_block_count();