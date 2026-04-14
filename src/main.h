#pragma once

#include <stdint.h>

#include "bloc.h"
#include "queue.h"

// This should be 666.. repeating,
// but we want to step a little slower.
// The interplay of these constants affects
// how patient and aggressive we are when
// it comes to watching for and responding to
// lag spikes. I'm not sure about these values,
// but hopefully they're about right?
#define STEP_NANOS   67666666
#define FASTER_NANOS 66000000
#define PENALTY_FRAMES 90

#define TEXT_BUF_LEN 200
typedef bloc<char, TEXT_BUF_LEN> strbuf;

extern int main_typingLen;
extern char main_textBuffer[TEXT_BUF_LEN];
extern queue<strbuf> outboundTextQueue;
extern char loopbackCommandBuffer[TEXT_BUF_LEN];

// We don't define this, but we claim that it exists!
// Need this for the types on some of the pointers we expose
struct gamestate;

extern char globalRunning;
extern gamestate *rootState;

extern char isCmd(const char* input, const char *cmd);
extern void mouseGrab(char grab);
