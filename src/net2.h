#include "queue.h"
#include "list.h"

// This should match server.py's `MAX_AHEAD`.
#define MAX_AHEAD 15
#define FRAME_ID_MAX (1<<29)

extern pthread_mutex_t netMutex;
extern pthread_cond_t netCond;

// This is the stuff we're going to mutex lock
// ===========================================

extern queue<list<list<char>>> frameData;
extern int finalizedFrames;
extern char asleep;

// ============= End mutex lock ==============

// This may actually block if only part of a frame's data is currently available.
// This is fine for now - it's not common (unless server is malicious?), and even
// if it did happen a lot, there's not a lot else that contends for the attention
// of the "wait for file descriptor to be ready" thread.
extern char net2_read();

extern void net2_init(int _numPlayers, int _frame);
extern void net2_destroy();
