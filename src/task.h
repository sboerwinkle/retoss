#pragma once

struct gamestate;

struct taskDefn {
	int id;
	char (*step)(gamestate *gs, void *data);
	char (*trans)(gamestate *gs, void **data);
	void (*copy)(void **to, void *from);
	void (*destroy)(void *data);
};

struct taskInstance {
	void *data;
	taskDefn *defn;
	taskInstance *prev, *next;
};

enum {
	TSK_TDM,
	TSK_KILL_PLANE,
	TSK_RAILS,
	TSK_DYNAMICS,
	TSK_BLAST,
};

extern taskDefn* taskLookup(int id);

extern void task_init();
extern void task_destroy();
