#include "gamestate.h"

struct taskDefn {
	int id;
	void (*step)(gamestate *gs, void *data);
	void (*trans)(void **data);
	void (*destroy)(void *data); // TODO is this always just `free` in practice?
};

struct taskInstance {
	void *data;
	taskDefn *defn;
};
