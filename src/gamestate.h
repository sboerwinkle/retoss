#pragma once

#include "list.h"

#define chunkSizeSpaces 8
#define boardSizeChunks 20
#define boardSizeSpaces (boardSizeChunks*chunkSizeSpaces)
#define boardAreaChunks (boardSizeChunks*boardSizeChunks)
#define chunkAreaSpaces (chunkSizeSpaces*chunkSizeSpaces)

// Intentionally simple model for this gamestate means actually
// no Sharing or Bookkeeping at all (see ../notes.txt).
// We only have a few types, and they each only have 1 Strong reference to them,
// which keeps the data very easy to handle.
// We do have a COW type though, since testing that out is half the point
// of this little project.

// Strong COW
struct mapChunk {
	char data[chunkAreaSpaces];
	int refs;
};

// Strong PIG
struct player {
	int32_t x, y;
	char move;
	unsigned char fallTimer;
	unsigned char moveTimer;
};

struct gamestate {
	list<player> players;
	mapChunk *board[boardAreaChunks];
};

extern void resetPlayer(gamestate *gs, int i);
extern void setupPlayers(gamestate *gs, int numPlayers);

extern void runTick(gamestate *gs);
extern gamestate* dup(gamestate *orig);
extern void prepareGamestateForLoad(gamestate *gs, char isSync);
extern void cleanup(gamestate *gs);

extern void write32(list<char> *data, int32_t v);
extern void serialize(gamestate *gs, list<char> *data);
extern void deserialize(gamestate *gs, list<char> *data, char fullState);
