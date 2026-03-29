#include "util.h"
#include "list.h"
#include "matrix.h"

#include "task.h"

#include "tasks/tdmScore.h"

static list<taskDefn> taskDefns;

// This doesn't need to be fast, I think we only
// need it during serialization (and not a whole
// lot even then). If it starts being used more,
// we can always optimize then.
taskDefn* taskLookup(int id) {
	rangeconst(i, taskDefns.num) {
		if (taskDefns[i].id == id) return &taskDefns[i];
	}
	return NULL;
}

// Small helper for adding task defn's during `task_init`.
// Probably shouldn't call this later on, as list reallocation
// can cause exising `taskDefn*`s to become invalid.
static void add(int id, void (*f)(taskDefn*)) {
	taskDefn &defn = taskDefns.add();
	defn.id = id;
	(*f)(&defn);
}

void task_init() {
	taskDefns.init();
	// Only one for now...
	add(TSK_TDM, &defineTask_tdmScore);
}

void task_destroy() {
	taskDefns.destroy();
}
