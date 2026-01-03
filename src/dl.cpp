#include <dlfcn.h>
#include <math.h>
#include <stdio.h>

#include "util.h"
#include "list.h"
#include "mtx.h"

#include "gamestate.h"
#include "bctx.h"
#include "game_graphics.h"

#include "dl.h"
#include "dl_game.h"

// DL (Dynamic Load) stuff

static int lvlWrVersion = -1;
static void *fileHandle = NULL;
static void (*lvlUpdFn)(gamestate*) = NULL;
static FILE* editEventsFifo;

static gamestate *updGamestate = NULL;
static player *updPlayer = NULL;
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
}

static void updReset_post() {
	range(i, dl_updVars.num) {
		if (!dl_updVars[i].seen) {
			dl_updVars.rmAt(i);
			i--;
		}
	}

	if (!dl_updVars.num) {
		dl_updVar *x = &dl_updVars.add();
		strcpy(x->name, "[NONE]");
		x->value.integer = x->incr = x->seen = 1;
		x->type = VAR_T_INT;
	}

	if (dl_updVarSelected >= dl_updVars.num) dl_updVarSelected = 0;

	updResetting = 0;
	mtx_unlock(dl_updVarMtx);
}

static void upd_pre(gamestate *gs, int myPlayer) {
	bctx.reset(gs);
	if (updGamestate) {
		puts("ERROR: dl: `updGamestate` already set, what's up???");
		exit(1);
	}
	updGamestate = gs;
	updPlayer = &gs->players[myPlayer];

	rangeconst(i, dl_updVars.num) {
		dl_updVars[i].seen = 0;
	}
}

static void upd_post() {
	updGamestate = NULL;
}

// Right now this always returns the same pointer,
// which of course could cause problems. Maybe revisit this at some point.
int64_t* look(int64_t dist) {
	static offset result;
	if (!updGamestate) {
		puts("WARN: dl: `look` called outside of `lvlUpd`?");
		range(i, 3) result[i] = 0;
	} else {
		float dirPlayer[3] = {0, 1, 0};
		float dirWorld[3];
		quat_apply(dirWorld, quatCamRotation, dirPlayer);

		// This is the offset from `bctx`'s position, but still with the world rotation (no rotation)
		offset intermediate;
		range(i, 3) {
			intermediate[i] = (int64_t)(dirWorld[i] * dist) + updPlayer->pos[i] - bctx.transf.pos[i];
		}

		// Convert `intermediate` to use `bctx`'s rotation
		iquat invRot;
		iquat_inv(invRot, bctx.transf.rot);
		iquat_applySm(result, invRot, intermediate);
	}
	return result;
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

static dl_updVar *findVarByName(char const *name) {
	rangeconst(i, dl_updVars.num) {
		if (!strncmp(name, dl_updVars[i].name, DL_VARNAME_LEN)) {
			return &dl_updVars[i];
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
		return NULL;
	}
	if (strlen(name) >= DL_VARNAME_LEN) {
		printf("new var name \"%s\" is too long!\n", name);
		return NULL;
	}
	if (strchr(name, ' ')) {
		// There's actually lots of stuff you shouldn't include -
		// anything that requires a backslash escape -
		// but spaces are maybe more common.
		printf("new var name \"%s\" may not contain spaces!\n", name);
		return NULL;
	}

	dl_updVar *x = &dl_updVars.add();
	strcpy(x->name, name);
	x->seen = 0;
	x->type = VAR_T_UNSET;

	return x;
}

// Unlike most non-static methods, we omit the "dl_" prefix.
// This method has a super-short name since it's intended for use in
// the dl'd source files for level editing.
int64_t var(char const *name) { return var(name, 0); }

int64_t var(char const *name, int64_t val) {
	dl_updVar *v = findVarByName(name);

	if (!v) return 0;

	v->seen = 1;

	if (v->type == VAR_T_UNSET) { // New var
		v->type = VAR_T_INT;
		v->incr = 1;
		v->value.integer = val;
	}

	return v->value.integer;
}

int64_t const * pvar(char const *name, offset const val) {
	dl_updVar *v = findVarByName(name);

	if (!v) return (offset const){0,0,0};

	if (!v->seen) {
		v->seen = 1;
		// Store the reverse of the current buildContext's rotation,
		// which we use for translating edit gestures into this coordinate system.
		// This one is a floating-point quaternion, since it's only used for edit input
		// (so we don't require reproducible math)
		v->value.position.rot[0] = (double)bctx.transf.rot[0] / FIXP;
		v->value.position.rot[1] = (double)-bctx.transf.rot[1] / FIXP;
		v->value.position.rot[2] = (double)-bctx.transf.rot[2] / FIXP;
		v->value.position.rot[3] = (double)-bctx.transf.rot[3] / FIXP;
	}

	if (v->type == VAR_T_UNSET) { // New var
		v->type = VAR_T_POS;
		v->incr = 100;
		memcpy(v->value.position.vec, val, sizeof(offset));
	}

	return v->value.position.vec;
}

int32_t const * rvar(char const *name) {
	return rvar(name, (int32_t const[]){0,0,0});
}

int32_t const * rvar(char const *name, int32_t const val[3]) {
	dl_updVar *v = findVarByName(name);

	if (!v) return (int32_t const[]){0,0,0};

	v->seen = 1;

	if (v->type == VAR_T_UNSET) { // New var
		v->type = VAR_T_ROT;
		v->incr = 15;
		memcpy(v->value.rotation.rotParams, val, sizeof(int32_t[3]));

		range(i, 3) {
			// Recall that the quaternion sin/cos are for *half the angle*
			double halfRadians = asin((double)val[i]/FIXP);
			v->value.rotation.angles[i] = round(halfRadians*2*180/M_PI);
		}
	}

	return v->value.rotation.rotParams;
}

void dl_processFile(char const *filename, gamestate *gs, int myPlayer) {
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
		upd_pre(gs, myPlayer);
		(*lvlUpdFn)(gs);
		upd_post();
	}
	updReset_post();
}

void dl_upd(gamestate *gs, int myPlayer) {
	if (lvlUpdFn) {
		upd_pre(gs, myPlayer);
		(*lvlUpdFn)(gs);
		upd_post();
	}
}

void dl_bake(char const *name) {
	if (!editEventsFifo) return;
	fputs("/bake\n", editEventsFifo);
	rangeconst(i, dl_updVars.num) {
		dl_updVar &v = dl_updVars[i];
		if (!name || !strcmp(name, v.name)) {
			if (v.type == VAR_T_INT) {
				fprintf(editEventsFifo, "%s %ld\n", v.name, v.value.integer);
			} else if (v.type == VAR_T_POS) {
				fprintf(
					editEventsFifo, "%s (offset const){%ld, %ld, %ld}\n",
					v.name,
					v.value.position.vec[0],
					v.value.position.vec[1],
					v.value.position.vec[2]
				);
			} else if (v.type == VAR_T_ROT) {
				fprintf(
					editEventsFifo, "%s (int32_t const[]){%d, %d, %d}\n",
					v.name,
					v.value.rotation.rotParams[0],
					v.value.rotation.rotParams[1],
					v.value.rotation.rotParams[2]
				);

			}
		}
	}
	fputs("\n", editEventsFifo);
	fflush(editEventsFifo);
}

void dl_hotbar(char const *name) {
	if (!editEventsFifo) return;
	fprintf(editEventsFifo, "/hotbar %s\n", name);
	fflush(editEventsFifo);
}

void dl_init() {
	dl_updVars.init();
	updReset_pre();
	updReset_post();
	editEventsFifo = fopen("edit_events.fifo", "w");
	if (!editEventsFifo) {
		// TODO shouldn't assume everyone is using an edit setup,
		//      make it more clear that this isn't a big deal.
		perror("Failed to open edit_events.fifo");
	}
}

void dl_destroy() {
	if (editEventsFifo) fclose(editEventsFifo);
	dl_updVars.destroy();
}
