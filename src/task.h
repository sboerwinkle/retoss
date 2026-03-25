#pragma once

struct gamestate;

struct taskDefn {
	int id;
	void (*step)(gamestate *gs, void *data);
	void (*trans)(void **data);
	void (*copy)(void **to, void *from);
	void (*destroy)(void *data);
};

struct taskInstance {
	void *data;
	taskDefn *defn;
};

enum {
	TSK_TDM,
};

extern taskDefn* taskLookup(int id);

extern void task_init();
extern void task_destroy();
