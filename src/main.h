#pragma once

#include <stdint.h>

#include "bloc.h"
#include "queue.h"

#define TEXT_BUF_LEN 200
typedef bloc<char, TEXT_BUF_LEN> strbuf;

extern queue<strbuf> outboundTextQueue;

#define mtx_lock(mtx) if (int __ret = pthread_mutex_lock(&mtx)) printf("Mutex lock failed with code %d\n", __ret)
#define mtx_unlock(mtx) if (int __ret = pthread_mutex_unlock(&mtx)) printf("Mutex unlock failed with code %d\n", __ret)
#define mtx_wait(cond, mtx) if (int __ret = pthread_cond_wait(&cond, &mtx)) printf("Mutex cond wait failed with code %d\n", __ret)
#define mtx_signal(cond) if (int __ret = pthread_cond_signal(&cond)) printf("Mutex cond signal failed with code %d\n", __ret)

// We don't define this, but we claim that it exists!
// Need this for the types on some of the pointers we expose
struct gamestate;

extern void showMessage(gamestate const * const gs, char const * const msg);
extern char isCmd(const char* input, const char *cmd);

extern char globalRunning;
extern gamestate *rootState;
