#include <dlfcn.h>

#include "util.h"
#include "list.h"
#include "mtx.h"

#include "gamestate.h"
#include "bctx.h"

#include "dl.h"
#include "dl_game.h"

// DL (Dynamic Load) stuff

static int lvlWrVersion = -1;
static void *fileHandle = NULL;
static void (*lvlUpdFn)(gamestate*) = NULL;

static int updVarsVersion = 0;
list<dl_updVar> dl_updVars;
int dl_updVarSelected = 0;

// Most of the functions here will be called only from the game thread,
// so a lot of multithreading headaches are avoided.
// This mutex is for the graphics thread, which needs to read some of this data for display.
// During this period, resizing `dl_updVars` or updating `dl_updVar->name` is illegal.
mtx_t dl_updVarMtx = MTX_INIT_EXPR;
// Practically, this is the same thing as "is the mutex locked".
// Only the game thread uses the functions that read/write this,
// so no concerns about ensuring atomic writes etc.
static char updResetting = 0;

static void printDlError(char const *prefix) {
	char const *msg = dlerror();
	if (!msg) msg = "[dlerror returned NULL]";
	printf("%s: %s\n", prefix, msg);
}

static void lvlWr_reset(gamestate *gs) {
	// May do more here eventually
	prepareGamestateForLoad(gs, 0);
}

static void updReset_pre() {
	mtx_lock(dl_updVarMtx);
	updResetting = 1;

	rangeconst(i, dl_updVars.num) {
		dl_updVars[i].inUse = 0;
	}
}

static void updReset_post() {
	range(i, dl_updVars.num) {
		if (!dl_updVars[i].inUse) {
			dl_updVars.rmAt(i);
			i--;
		}
	}

	if (!dl_updVars.num) {
		dl_updVar *x = &dl_updVars.add();
		strcpy(x->name, "[NONE]");
		x->value = x->incr = x->inUse = 1;
	}

	if (dl_updVarSelected >= dl_updVars.num) dl_updVarSelected = 0;

	updResetting = 0;
	mtx_unlock(dl_updVarMtx);
}

void dl_resetVars(int version) {
	// Aside from modifications being illegal when the lock isn't held,
	// this is also an operation that semantically only happens during
	// a reset to `lvlUpdFn`.
	if (!updResetting) return;

	if (version != updVarsVersion) {
		dl_updVars.num = 0;
		updVarsVersion = version;
	}
}

// Unlike most non-static methods, we omit the "dl_" prefix.
// This method has a super-short name since it's intended for use in
// the dl'd source files for level editing.
int64_t var(char const *name) { return var(name, 0); }

int64_t var(char const *name, int64_t val) {
	rangeconst(i, dl_updVars.num) {
		if (!strncmp(name, dl_updVars[i].name, DL_VARNAME_LEN)) {
			if (updResetting) dl_updVars[i].inUse = 1;
			return dl_updVars[i].value;
		}
	}

	// Register new var
	if (!updResetting) {
		// We could allow new vars here, but it's bad practice, better to fail fast.
		// Trouble is that any vars that can be found dynamically will also be scrubbed
		// when we reload the dl file, since they won't show up in the pass and will
		// look obsolete.
		// (Also, we later added mutex locking on modifying that list!)
		printf("Unexpected new var \"%s\"!\n", name);
		return 0;
	}
	if (strlen(name) >= DL_VARNAME_LEN) {
		printf("new var name \"%s\" is too long!\n", name);
		return 0;
	}

	dl_updVar *x = &dl_updVars.add();
	strcpy(x->name, name);
	x->value = val;
	x->incr = 1;
	x->inUse = 1;

	return val;
}

void dl_processFile(char const *filename, gamestate *gs) {
	const int bufLen = 100;
	char path[bufLen];
	snprintf(path, bufLen, "./src/dl_tmp/%s", filename);
	printf("dl: %s\n", path);

	if (fileHandle) {
		if (dlclose(fileHandle)) {
			printDlError("`dlclose` failed");
			// Not sure what makes more sense in this case. For now we just try to proceed anyway?
		}
		fileHandle = NULL;
		lvlUpdFn = NULL;
	}

	fileHandle = dlopen(path, RTLD_NOW);
	if (!fileHandle) {
		printDlError("`dlopen` failed");
		return;
	}

	int* version = (int*)dlsym(fileHandle, "lvlWrVersion");
	if (version && *version > lvlWrVersion) {

		void (*lvlWrFn)(gamestate*) = (void (*)(gamestate*)) dlsym(fileHandle, "lvlWr");
		if (lvlWrFn) {
			lvlWr_reset(gs);
			(*lvlWrFn)(gs);
			lvlWrVersion = *version;
		} else {
			printf("Unable to load \"lvlWr\" despite \"lvlWrVersion\" increasing from %d to %d.\n", lvlWrVersion, *version);
			printDlError("`dlSym` says:");
		}
	}

	lvlUpdFn = (void (*)(gamestate*)) dlsym(fileHandle, "lvlUpd");
	updReset_pre();
	if (lvlUpdFn) {
		bctx.reset(gs);
		(*lvlUpdFn)(gs);
	}
	updReset_post();
}

void dl_upd(gamestate *gs) {
	if (lvlUpdFn) {
		bctx.reset(gs);
		(*lvlUpdFn)(gs);
	}
}

void dl_init() {
	dl_updVars.init();
	updReset_pre();
	updReset_post();
}

void dl_destroy() {
	dl_updVars.destroy();
}
