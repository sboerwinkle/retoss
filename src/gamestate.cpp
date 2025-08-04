#include <arpa/inet.h>

#include "util.h"
#include "gamestate.h"

static void decr(mapChunk *mc);
static mapChunk* dup(mapChunk* mc);
static void setSpace(gamestate *gs, int x, int y, char value);

void runTick(gamestate *gs) {
	// TODO all the fun logic actually
}

void resetPlayer(gamestate *gs, int i) {
	int32_t pos = boardSizeSpaces/2;
	gs->players[i] = {.x=pos, .y=pos};
	setSpace(gs, pos, pos, 0);
}

void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	range(i, gs->players.num) resetPlayer(gs, i);
}

// I'm thinking `isSync` may be unused forever, but we can leave it for now (forever)
void prepareGamestateForLoad(gamestate *gs, char isSync) {
	int numPlayers = gs->players.num;

	cleanup(gs);

	// Re-initialize with valid (but empty) data
	mapChunk *emptyChunk = (mapChunk*)malloc(sizeof(mapChunk));
	*emptyChunk = {}; // Zero the chunk out. Empty space, and zero references.
	range(i, boardAreaChunks) {
		gs->board[i] = dup(emptyChunk);
	}
	gs->players.init(numPlayers);
	setupPlayers(gs, numPlayers);
}

gamestate* dup(gamestate *orig) {
	gamestate *ret = (gamestate*)malloc(sizeof(gamestate));
	range(i, boardAreaChunks) ret->board[i] = dup(orig->board[i]);
	ret->players.init(orig->players);
	return ret;
}

void cleanup(gamestate *gs) {
	range(i, boardAreaChunks) {
		decr(gs->board[i]);
	}
	gs->players.destroy();
}

static void decr(mapChunk* mc) {
	mc->refs--;
	// Cleanup for a mapChunk is fortunately very simple
	if (!mc->refs) free(mc);
}

static void mkWritable(mapChunk **mc2) {
	mapChunk *&mc = *mc2;
#ifndef NODEBUG
	if (mc->refs < 1) {
		printf("ERROR: refs is %d\n", mc->refs);
	}
#endif
	if (mc->refs == 1) return;

	mapChunk *ret = (mapChunk*)malloc(sizeof(mapChunk));
	*ret = *mc;
	mc->refs--;
	ret->refs = 1;

	mc = ret;
}

static mapChunk* dup(mapChunk* mc) {
	// Beautiful COW
	mc->refs++;
	return mc;
}

// May not be `static` in the future, idk
static void setSpace(gamestate *gs, int x, int y, char value) {
	if (x < 0 || x >= boardSizeSpaces || y < 0 || y >= boardSizeSpaces) return;
	int c_x = x / chunkSizeSpaces;
	int c_y = y / chunkSizeSpaces;
	int s_x = x % chunkSizeSpaces;
	int s_y = y % chunkSizeSpaces;
	int c_coord = c_y * boardSizeChunks + c_x;
	int s_coord = s_y * chunkSizeSpaces + s_x;

	mkWritable(gs->board + c_coord);
	gs->board[c_coord]->data[s_coord] = value;
}


/////
// seriz / deser (serialization / deserialization)
/////
static const char* version_string = "rTs0";
#define VERSION 0

// Some global state to make stuff easier.
// Could put this in an object and pass it around if I really needed to,
// but seriz/deser will probably only ever be done on one thread.
static char modeDeser = 0;
static int deserIndex = 0;
static int version = 0;

static void write8(list<char> *data, char v) {
	data->add(v);
}

static char read8(list<char> *data) {
	int i = deserIndex;
	if (i >= data->num) return 0;
	deserIndex++;
	return (*data)[i];
}

static void trans8(list<char> *data, char *x) {
	if (modeDeser) *x = read8(data);
	else write8(data, *x);
}

// We don't care about unsigned vs signed char.
// Small method here prevents manually casting each such case.
static void trans8(list<char> *data, unsigned char *x) {
	trans8(data, (char*) x);
}

static void write32Raw(list<char> *data, int offset, int32_t v) {
	*(int32_t*)(data->items + offset) = htonl(v);
}

// TODO put this in some other file?
//      Maybe some of the more generic utility functions here
//      go back to living in `serialize.cpp`?
void write32(list<char> *data, int32_t v) {
	int n = data->num;
	data->setMaxUp(n + 4);
	write32Raw(data, n, v);
	data->num = n + 4;
}

static int32_t read32(list<char> *data) {
	int i = deserIndex;
	if (i + 4 > data->num) return 0;
	deserIndex += 4;
	return ntohl(*(int32_t*)(data->items + i));
}

static void trans32(list<char> *data, int32_t *x) {
	if (modeDeser) *x = read32(data);
	else write32(data, *x);
}

static void writeBlock(list<char> *data, char *mem, int len) {
	// Could probably rewrite this to use `data->addAll`,
	// but right now that uses `setMax` instead of `setMaxUp` internally.
	int n = data->num;
	data->setMaxUp(n + len);
	memcpy(data->items + n, mem, len);
	data->num = n + len;
}

static void readBlock(list<char> *data, char *mem, int len) {
	int i = deserIndex;
	if (i + len > data->num) {
		memset(mem, 0, len);
		return;
	}
	memcpy(mem, data->items+i, len);
	deserIndex += len;
}

static void transBlock(list<char> *data, char *mem, int len) {
	if (modeDeser) readBlock(data, mem, len);
	else writeBlock(data, mem, len);
}

static void transBoard(list<char> *data, gamestate *gs) {
	if (modeDeser) {
		range(i, boardAreaChunks) mkWritable(gs->board + i);
	}
	range(i, boardAreaChunks) {
		// Compression? What's that?????
		transBlock(data, gs->board[i]->data, chunkSizeSpaces);
	}
}

static void transPlayer(list<char> *data, player *p) {
	trans32(data, &p->x);
	trans32(data, &p->y);
	trans8(data, &p->move);
	trans8(data, &p->fallTimer);
	trans8(data, &p->moveTimer);
}

/* Unused
static void writeStr(list<char> *data, const char *str) {
	int len = strlen(str);
	if (len > 255) len = 255;
	data->add((unsigned char) len);
	int end = data->num + len;
	data->setMaxUp(end);
	memcpy(data->items + data->num, str, len);
	data->num = end;
}

static void readStr(const list<const char> *data, int *ix, char *dest, int limit) {
	int reportedLen = (unsigned char) read8(data, ix);
	int i = *ix;
	int avail = data->num - i;
	if (avail < reportedLen || limit - 1 < reportedLen) {
		// Don't mess around even trying to read this.
		// `avail` could be negative, e.g.
		*ix = data->num;
		*dest = '\0';
		return;
	}

	memcpy(dest, data->items + i, reportedLen);
	dest[reportedLen] = '\0';
	*ix = i + reportedLen;
}
*/

static void writeHeader(list<char> *data) {
	data->setMaxUp(data->num + 4);
	memcpy(&(*data)[data->num], version_string, 4);
	data->num += 4;
}

void serialize(gamestate *gs, list<char> *data) {
	modeDeser = 0;
	version = VERSION;
	writeHeader(data);
	transBoard(data, gs);
	write8(data, gs->players.num);
	range(i, gs->players.num) {
		transPlayer(data, &gs->players[i]);
	}
}

static int verifyHeader(list<char> *data) {
	if (data->num < 4) {
		fprintf(stderr, "Only got %d bytes of data, can't deserialize\n", data->num);
		return -1;
	}
	// Only compare 3 bytes, not 4, since the last one we use as a version
	if (strncmp(data->items, version_string, 3)) {
		fprintf(
			stderr,
			"Beginning of serialized data should read \"%s\", actually reads \"%c%c%c%c\"\n",
			version_string,
			(*data)[0], (*data)[1], (*data)[2], (*data)[3]
		);
		return -1;
	}
	version = data->items[3] - '0';
	if (version < 0 || version > VERSION) {
		fprintf(
			stderr,
			"Version number should be between 0 and %d, but found %d\n",
			VERSION, version
		);
		return -1;
	}

	return 0;
}

void deserialize(gamestate *gs, list<char> *data, char fullState) {
	if (verifyHeader(data)) return;
	modeDeser = 1;
	deserIndex = 4; // 4 header bytes

	transBoard(data, gs);
	int players = read8(data);
	// If there are fewer players in the game than the file, ignore extras.
	if (gs->players.num < players) players = gs->players.num;
	// If there are more  players in the game than the file, we rely on
	// `prepareGamestateForLoad` to have consistently initialized them.
	
	range(i, players) transPlayer(data, &gs->players[i]);
}
