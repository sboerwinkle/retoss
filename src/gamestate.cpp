#include <arpa/inet.h>

#include "util.h"
#include "gamestate.h"
#include "random.h"

static void decr(mapChunk *mc);
static mapChunk* dup(mapChunk* mc);
static void setSpace(gamestate *gs, int x, int y, char value);
static char getSpace(gamestate *gs, int x, int y);

void resetPlayer(gamestate *gs, int i) {
	int32_t pos = boardSizeSpaces/2;
	gs->players[i] = {.x=pos, .y=pos, .facing=1};
	setSpace(gs, pos, pos, 0);
}

void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	range(i, gs->players.num) resetPlayer(gs, i);
}

void runTick(gamestate *gs) {
	range(i, gs->players.num) {
		player &p = gs->players[i];
		if (p.cooldown) {
			p.cooldown--;
			continue;
		}
		// If there's a free space beneath us,
		if (!getSpace(gs, p.x, p.y - 1)) {
			// If player asked to go down, or doesn't have a ledge to stand on,
			if (p.move == 2 || !getSpace(gs, p.x + p.facing, p.y - 1)) {
				// player goes down.
				p.y--;
				p.cooldown = 4; // cooldown of 4 => 1 move every 5 frames => 3Hz
				continue;
			}
		}
		if (p.move != 1 && p.move != -1) continue;
		p.facing = p.move;
		if (!getSpace(gs, p.x + p.move, p.y)) {
			p.x += p.move;
			p.cooldown = 4;
		} else if (!getSpace(gs, p.x, p.y + 1)) {
			p.y += 1;
			p.cooldown = 4;
		}
	}
}

static char isGrounded(gamestate *gs, player *p) {
	return getSpace(gs, p->x, p->y - 1) || getSpace(gs, p->x + p->facing, p->y - 1);
}

void actionBuild(gamestate *gs, player *p) {
	if (!isGrounded(gs, p)) return;
	// Don't want to mess with tracking a random seed right now,
	// or adding a new tile for player buildings,
	// so they just make the first dirt tile always.
	setSpace(gs, p->x - 1, p->y, 1);
	setSpace(gs, p->x + 1, p->y, 1);
}

void actionDig(gamestate *gs, player *p) {
	if (!isGrounded(gs, p)) return;
	setSpace(gs, p->x + p->facing, p->y, 0);
}

void actionBomb(gamestate *gs, player *p) {
	if (!isGrounded(gs, p)) return;
	range(i, 3) {
		range(j, 3) {
			setSpace(gs, p->x-1+i, p->y-1+j, 0);
		}
	}
}

// I'm thinking `isSync` may be unused forever, but we can leave it for now (forever)
void prepareGamestateForLoad(gamestate *gs, char isSync) {
	int numPlayers = gs->players.num;

	cleanup(gs);
	// Re-initialize with valid (but empty) data
	init(gs);

	// Setup any data that might carry over (right now, just player count)
	setupPlayers(gs, numPlayers);
}

gamestate* dup(gamestate *orig) {
	gamestate *ret = (gamestate*)malloc(sizeof(gamestate));
	range(i, boardAreaChunks) ret->board[i] = dup(orig->board[i]);
	ret->players.init(orig->players);
	return ret;
}

void init(gamestate *gs) {
	mapChunk *emptyChunk = (mapChunk*)malloc(sizeof(mapChunk));
	*emptyChunk = {}; // Zero the chunk out. Empty space, and zero references.
	range(i, boardAreaChunks) {
		gs->board[i] = dup(emptyChunk);
	}
	gs->players.init();
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

void shuffle(gamestate *gs, uint32_t seed) {
	range(i, boardAreaChunks) {
		mkWritable(&gs->board[i]);
		mapChunk *mc = gs->board[i];
		range(j, chunkAreaSpaces) {
			uint32_t rand = splitmix32(&seed);
			// This is kind of goofy, but I'm having a good time okay?
			mc->data[j] = (rand % 4) % 3;
		}
	}
}

static char coords(int x, int y, int *chunk, int *space) {
	if (x < 0 || x >= boardSizeSpaces || y < 0 || y >= boardSizeSpaces) return 1;
	int c_x = x / chunkSizeSpaces;
	int c_y = y / chunkSizeSpaces;
	int s_x = x % chunkSizeSpaces;
	int s_y = y % chunkSizeSpaces;
	*chunk = c_y * boardSizeChunks + c_x;
	*space = s_y * chunkSizeSpaces + s_x;
	return 0;
}

// May not be `static` in the future, idk
static void setSpace(gamestate *gs, int x, int y, char value) {
	int c_coord, s_coord;
	if (coords(x, y, &c_coord, &s_coord)) return;

	if (value) {
		range(i, gs->players.num) {
			player &p = gs->players[i];
			if (p.x == x && p.y == y) return;
		}
	}

	mkWritable(gs->board + c_coord);
	gs->board[c_coord]->data[s_coord] = value;
}

static char getSpace(gamestate *gs, int x, int y) {
	int c_coord, s_coord;
	if (coords(x, y, &c_coord, &s_coord)) return 1;

	return gs->board[c_coord]->data[s_coord];
}

// Seriz / Deser stuff

static void writeBlock(char *mem, int len) {
	// Could probably rewrite this to use `data->addAll`,
	// but right now that uses `setMax` instead of `setMaxUp` internally.
	int n = seriz_data->num;
	seriz_data->setMaxUp(n + len);
	memcpy(seriz_data->items + n, mem, len);
	seriz_data->num = n + len;
}

static void readBlock(char *mem, int len) {
	int i = seriz_index;
	if (i + len > seriz_data->num) {
		memset(mem, 0, len);
		return;
	}
	memcpy(mem, seriz_data->items+i, len);
	seriz_index += len;
}

static void transBlock(char *mem, int len) {
	if (seriz_data) readBlock(mem, len);
	else writeBlock(mem, len);
}

static void transBoard(gamestate *gs) {
	if (seriz_reading) {
		range(i, boardAreaChunks) mkWritable(gs->board + i);
	}
	range(i, boardAreaChunks) {
		// Compression? What's that?????
		transBlock(gs->board[i]->data, chunkAreaSpaces);
	}
}

static void transPlayer(player *p) {
	trans32(&p->x);
	trans32(&p->y);
	trans8(&p->move);
	trans8(&p->facing);
	trans8(&p->cooldown);
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

void serialize(gamestate *gs, list<char> *data) {
	seriz_data = data;
	seriz_reading = 0;
	seriz_version = seriz_latestVersion;

	seriz_writeHeader();
	transBoard(gs);
	write8(gs->players.num);
	range(i, gs->players.num) {
		transPlayer(&gs->players[i]);
	}
}

void deserialize(gamestate *gs, list<char> *data, char fullState) {
	seriz_data = data;
	seriz_reading = 1;
	// This will set seriz_version and seriz_index
	// (if no error)
	if (seriz_verifyHeader(data)) return;

	transBoard(gs);
	int players = read8();
	// If there are fewer players in the game than the file, ignore extras.
	if (gs->players.num < players) players = gs->players.num;
	// If there are more  players in the game than the file, we rely on
	// `prepareGamestateForLoad` to have consistently initialized them.
	
	range(i, players) transPlayer(&gs->players[i]);
}
