#pragma once
#include <pthread.h>
#include <stdlib.h>
typedef pthread_mutex_t *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    SemaphoreHandle_t m = malloc(sizeof(*m));
    if (m) pthread_mutex_init(m, NULL);
    return m;
}
static inline int xSemaphoreTake(SemaphoreHandle_t m, unsigned t) { (void)t; return m ? pthread_mutex_lock(m) == 0 : 1; }
static inline int xSemaphoreGive(SemaphoreHandle_t m) { return m ? pthread_mutex_unlock(m) == 0 : 1; }
