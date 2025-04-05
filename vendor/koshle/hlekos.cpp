#include "dc_hle_types.h"
#include "dc/pvr.h"
#include "dc/maple.h"
#include "dc/maple/controller.h"
#include "dc/asic.h"

#include "emu/emu.h"

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdio>

#include "refsw/refsw_tile.h"

#include "pvr_internal.h"
volatile pvr_state_t pvr_state;

static maple_device_t dev;
cont_state_t mapleInput;
void * maple_dev_status(maple_device*) {
    return &mapleInput;
}
maple_device_t * maple_enum_type(int n, uint32 func) {
    return &dev;
}

int sem_wait_timed(semaphore_t *sem, int timeout) {
    auto count = sem->count.load();

    while(count <= 0 || !sem->count.compare_exchange_strong(count, count-1)) {
        count = sem->count.load();
    }

    return 0;
}

int sem_count(semaphore_t *sem) {
    return sem->count.load();
}

int sem_init(semaphore_t *sm, int count) {
    sm->count = count;
    sm->initialized = 1;
    return 0;
}

int sem_destroy(semaphore_t *sm) {
    sm->count = INT32_MAX;
    sm->initialized = 0;
    return 0;
}
int sem_signal(semaphore_t *sem) {
    sem->count++;
    return 0;
}

void sq_lock(void *dest) {

}

void sq_unlock() {
    
}

void pvr_dma_init(void) {
}

void pvr_dma_shutdown(void) {

}