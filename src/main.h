#pragma once

#include <stdint.h>

#include "bloc.h"
#include "queue.h"

#define FASTER_NANOS 66000000

#define TEXT_BUF_LEN 200
typedef bloc<char, TEXT_BUF_LEN> strbuf;

extern queue<strbuf> outboundTextQueue;
extern char loopbackCommandBuffer[TEXT_BUF_LEN];

// We don't define this, but we claim that it exists!
// Need this for the types on some of the pointers we expose
struct gamestate;

extern void showMessage(gamestate const * const gs, char const * const msg);
extern char isCmd(const char* input, const char *cmd);

extern char globalRunning;
extern gamestate *rootState;
