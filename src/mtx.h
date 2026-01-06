#pragma once

#include <pthread.h>

#define mtx_lock(mtx) if (int __ret = pthread_mutex_lock(&mtx)) printf("Mutex lock failed with code %d\n", __ret)
#define mtx_unlock(mtx) if (int __ret = pthread_mutex_unlock(&mtx)) printf("Mutex unlock failed with code %d\n", __ret)
#define mtx_wait(cond, mtx) if (int __ret = pthread_cond_wait(&cond, &mtx)) printf("Mutex cond wait failed with code %d\n", __ret)
#define mtx_signal(cond) if (int __ret = pthread_cond_signal(&cond)) printf("Mutex cond signal failed with code %d\n", __ret)

typedef pthread_mutex_t mtx_t;
#define MTX_INIT_EXPR PTHREAD_MUTEX_INITIALIZER
