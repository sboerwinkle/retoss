#include "util.h"
#include "cloneable.h"

#include "random.h"
#include "serialize.h"

#include "gamestate.h"
#include "gamestate_box.h"

list<cloneable*> dummyVelboxSerizList;
cloneable dummyDataItem;

void resetPlayer(gamestate *gs, int i) {
	gs->players[i] = {.pos={0,0,0}};
}

void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	range(i, gs->players.num) resetPlayer(gs, i);
}

void runTick(gamestate *gs) {
	velbox_refresh(gs->vb_root);
	range(i, gs->players.num) {
		player &p = gs->players[i];
		// We, uhh, tore everything out again lol
	}
	velbox_completeTick(gs->vb_root);
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
	ret->players.init(orig->players);
	ret->vb_root = velbox_dup(orig->vb_root);
	return ret;
}

void init(gamestate *gs) {
	gs->players.init();
	gs->vb_root = velbox_getRoot();

	// This will move obviously
	box *tmp = velbox_alloc();
	tmp->pos[0] = 0;
	tmp->pos[1] = 3000;
	tmp->pos[2] = 0;
	tmp->vel[0] = 0;
	tmp->vel[1] = 0;
	tmp->vel[2] = 0;
	tmp->r = 1000;
	tmp->end = tmp->start + 90; // This is either 3 sec or 6 sec, I forget
	tmp->data = &dummyDataItem;
	velbox_insert(gs->vb_root, tmp);
}

void cleanup(gamestate *gs) {
	velbox_freeRoot(gs->vb_root);
	gs->players.destroy();
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

// Not sure if we'll ever need this again lol
static void transBlock(char *mem, int len) {
	if (seriz_data) readBlock(mem, len);
	else writeBlock(mem, len);
}

static void transPlayer(player *p) {
	range(i, 3) trans64(&p->pos[i]);
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
	if (seriz_verifyHeader()) return;

	int players = read8();
	// If there are fewer players in the game than the file, ignore extras.
	if (gs->players.num < players) players = gs->players.num;
	// If there are more  players in the game than the file, we rely on
	// `prepareGamestateForLoad` to have consistently initialized them.
	
	range(i, players) transPlayer(&gs->players[i]);
	// Maybe extra data here corresponding to extra players we don't have seated currently
}

void gamestate_init() {
	dummyVelboxSerizList.init();
	dummyVelboxSerizList.add(&dummyDataItem);
	dummyDataItem.clone.ptr = &dummyDataItem;
}

void gamestate_destroy() {
	dummyVelboxSerizList.destroy();
}
