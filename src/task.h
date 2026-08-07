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

#define TSK_BLAST 1
#define TSK_TDM 100
#define TSK_KILL_PLANE 120
#define TSK_RAILS 140
#define TSK_DYNAMICS 200
#define TSK_ROCKET 300
#define TSK_RIFLE_SHOT 400

extern taskDefn* taskLookup(int id);

extern void task_init();
extern void task_destroy();
