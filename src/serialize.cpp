#include <arpa/inet.h>
#include "util.h"
#include "list.h"
#include "game.h"

#include "serialize.h"

static const char* version_string = "rTs0";
#define VERSION 0

static void write32Raw(list<char> *data, int offset, int32_t v) {
	*(int32_t*)(data->items + offset) = htonl(v);
}

void write32(list<char> *data, int32_t v) {
	int n = data->num;
	data->setMaxUp(n + 4);
	write32Raw(data, n, v);
	data->num = n + 4;
}

static void writeStr(list<char> *data, const char *str) {
	int len = strlen(str);
	if (len > 255) len = 255;
	data->add((unsigned char) len);
	int end = data->num + len;
	data->setMaxUp(end);
	memcpy(data->items + data->num, str, len);
	data->num = end;
}

static void writeHeader(list<char> *data) {
	data->setMaxUp(data->num + 4);
	memcpy(&(*data)[data->num], version_string, 4);
	data->num += 4;
}

void serialize(gamestate *gs, list<char> *data) {
	writeHeader(data);

	write32(data, gs->nonsense);

	// I don't have to actually write the player count since I don't have any data!
	// Deserializing doesn't set the player count, it just writes data for whatever
	// players the file and the current state have in common. And we don't have any
	// state, so...
}

static char read8(const list<const char> *data, int *ix) {
	int i = *ix;
	if (i >= data->num) return 0;
	*ix = i+1;
	return (*data)[i];
}

static int32_t read32(const list<const char> *data, int *ix) {
	int i = *ix;
	if (i + 4 > data->num) return 0;
	*ix = i + 4;
	return ntohl(*(int32_t*)(data->items + i));
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

static void checksum(const list<const char> *data, int *ix, int expected) {
	char x = (*data)[(*ix)++];
	if (x != (char) expected) {
		fprintf(stderr, "Expected %d, got %hhu\n", expected, x);
	}
}

static int verifyHeader(const list<const char> *data, int *ix) {
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
	// TODO Put this somewhere that other code can read it to decide what
	//      to expect in the serialized data. Previously this was global,
	//      but I was hoping to do better. Do I need to do better??
	char version = data->items[3] - '0';
	if (version < 0 || version > VERSION) {
		fprintf(
			stderr,
			"Version number should be between 0 and %d, but found %d\n",
			VERSION, version
		);
		return -1;
	}
	*ix = 4;

	return 0;
}

void deserialize(gamestate *gs, const list<const char> *data, char fullState) {
	int _ix;
	int *ix = &_ix;
	int numEnts = verifyHeader(data, ix);
	if (numEnts == -1) return;

	gs->nonsense = read32(data, ix);

	// This is the part where I'd read through serialized player data
	// and write it to the state (until I ran out of players). But we
	// have no per-player state at the moment, since there's no game!
}
