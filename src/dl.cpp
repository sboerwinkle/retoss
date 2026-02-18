#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>

#include "util.h"
#include "list.h"
#include "mtx.h"

#include "gamestate.h"
#include "bctx.h"
#include "game_graphics.h"
#include "collision.h"

#include "dl.h"
#include "dl_game.h"

// DL (Dynamic Load) stuff

static int lvlWrVersion = -1;
static void *fileHandle = NULL;
static void (*lvlUpdFn)(gamestate*) = NULL;
static FILE* editEventsFifo;

static gamestate *updGamestate = NULL;
static player *updPlayer = NULL;
static list<dl_varGroup> varGroups;
dl_varGroup *dl_selectedGroup;
int dl_selectedVar = 0;

static dl_varGroup *currentGroup = NULL;

static char lookAtGp_winner[DL_VARNAME_LEN];
static fraction lookAtGp_best;
static offset lookAtGp_origin;
static unitvec lookAtGp_dir;

// Most of the functions here will be called only from the game thread,
// so a lot of multithreading headaches are avoided.
// This mutex is for the graphics thread, which needs to read some of this data for display.
// During this period, updating anything with variable size (strings, lists) is forbidden.
mtx_t dl_varMtx = MTX_INIT_EXPR;
static char locked = 0;

void dl_varGroup::init() {
	vars.init();
	seen = 1;
	touched = 0;
}
void dl_varGroup::destroy() { vars.destroy(); }

static void printDlError(char const *prefix) {
	char const *msg = dlerror();
	if (!msg) msg = "[dlerror returned NULL]";
	printf("%s: %s\n", prefix, msg);
}

static void addDummyForGroup(int i) {
	dl_var *x = &varGroups[i].vars.add();
	strcpy(x->name, "[NONE]");
	x->value.integer = x->incr = x->seen = 1;
	x->touched = 0;
	x->type = VAR_T_INT;
}

static void lookAtGp_test(solid *s) {
	// Solid is newly created, so won't have `old` fields.
	// We use those in `raycast` right now though,
	// so populate them.
	memcpy(s->m.oldRot, s->m.rot, sizeof(s->m.rot));
	memcpy(s->m.oldPos, s->m.pos, sizeof(s->m.pos));
	if (raycast(&lookAtGp_best, &s->m, lookAtGp_origin, lookAtGp_dir)) {
		strcpy(lookAtGp_winner, currentGroup->name);
	}
}

static void lvlWr_reset(gamestate *gs) {
	// May do more here eventually
	prepareGamestateForLoad(gs, 0);
}

// This should only run in code where the mutex is locked.
// I don't check `locked` because that means I can be lazy in a couple places and not set it :)
// I crave a language that lets me describe these constraints and enforce them statically.
static void selectedVarFix() {
	if (dl_selectedVar >= dl_selectedGroup->vars.num) dl_selectedVar = 0;
}

static void processUpd(gamestate *gs, int myPlayer, char isFirstLoad) {
	// setup...
	mtx_lock(dl_varMtx);
	locked = 1;

	int selectedGroupIndex = dl_selectedGroup - varGroups.items;
	int origNumGroups = varGroups.num;

	bctx.reset(gs);
	if (updGamestate) {
		puts("ERROR: dl: `updGamestate` already set, what's up???");
		exit(1);
	}
	updGamestate = gs;
	updPlayer = &gs->players[myPlayer];

	// We do this even when not `isFirstLoad` because some things
	// (like positions) like to know when they're encountered the first time,
	// and `seen` is a good way to know that.
	rangeconst(i, varGroups.num) {
		dl_varGroup &g = varGroups[i];
		g.seen = 0;
		rangeconst(j, g.vars.num) g.vars[j].seen = 0;
	}

	gp("");

	// Run the fn...
	if (lvlUpdFn) {
		(*lvlUpdFn)(gs);
	}

	// If the file was modified, then anything that we didn't see in this pass
	// we will assume no longer exists.
	if (isFirstLoad) {
		// And if we added something, select that group!
		if (varGroups.num > origNumGroups) {
			selectedGroupIndex = origNumGroups;
			dl_selectedVar = 0;
		}
		range(i, varGroups.num) {
			if (!varGroups[i].seen) {
				if (selectedGroupIndex > i) selectedGroupIndex--;
				else if (selectedGroupIndex == i) selectedGroupIndex = 0;

				varGroups[i].destroy();
				varGroups.stableRmAt(i);
				i--;
				continue;
			}
			list<dl_var> &vars = varGroups[i].vars;
			range(j, vars.num) {
				if (!vars[j].seen) {
					vars.stableRmAt(j);
					j--;
				}
			}
		}
		if (!varGroups.num) {
			puts("ERROR: dl: Somehow, no groups seen (should always see "" group)");
			exit(1);
		}
	}

	currentGroup = NULL;
	updPlayer = NULL;
	updGamestate = NULL;

	dl_selectedGroup = &varGroups[selectedGroupIndex];

	// We might have empty groups not because variables are removed,
	// but because a new group can be added (with no variables)
	rangeconst(i, varGroups.num) {
		if (!varGroups[i].vars.num) addDummyForGroup(i);
	}

	selectedVarFix();

	locked = 0;
	mtx_unlock(dl_varMtx);
}

static dl_varGroup *findGroup(char const *name) {
	rangeconst(i, varGroups.num) {
		if (!strcmp(varGroups[i].name, name)) return &varGroups[i];
	}
	return NULL;
}

void gp(char const* groupName) {
	if (!locked) {
		// Used to have an error here, but now this is just
		// the case where a level is being loaded outside of editing.
		//puts("ERROR: dl: gp: must be locked");
		return;
	}

	dl_varGroup *existing = findGroup(groupName);
	if (existing) {
		currentGroup = existing;
		currentGroup->seen = 1;
		return;
	}

	if (strlen(groupName) >= DL_VARNAME_LEN) {
		printf("WARN: dl: gp: Group name '%s' is too long\n", groupName);
		return;
	}
	if (strchr(groupName, ' ')) {
		// Same rationale as "no spaces in var names"
		printf("WARN: dl: gp: New group name \"%s\" may not contain spaces!\n", groupName);
		return;
	}

	currentGroup = &varGroups.add();
	currentGroup->init();
	strcpy(currentGroup->name, groupName);
}

void dl_selectGp(char const* groupName) {
	if (locked) {
		puts("ERROR: dl: dl_selectGp: must not be locked");
		exit(1);
	}

	dl_varGroup *existing = findGroup(groupName);
	if (!existing) {
		printf("WARN: dl: dl_selectGp: No group with name '%s'\n", groupName);
		return;
	}

	// No function calls in here that care about `locked`, so no need to set it.
	mtx_lock(dl_varMtx);
	dl_selectedGroup = existing;
	selectedVarFix();
	mtx_unlock(dl_varMtx);
}

static void getLookUnitvec(unitvec dest) {
	float dirPlayer[3] = {0, 1, 0};
	float dirWorld[3];
	quat_apply(dirWorld, quatCamRotation, dirPlayer);
	range(i, 3) dest[i] = FIXP*dirWorld[i];
}

// Right now this always returns the same pointer,
// which of course could cause problems. Maybe revisit this at some point.
int64_t* look(int64_t dist) {
	static offset result;
	if (!updGamestate) {
		puts("WARN: dl: `look` called outside of `lvlUpd`?");
		range(i, 3) result[i] = 0;
	} else {
		unitvec dirWorld;
		getLookUnitvec(dirWorld);

		// This is the offset from `bctx`'s position, but still with the world rotation (no rotation)
		offset intermediate;
		range(i, 3) {
			intermediate[i] = dirWorld[i]*dist/FIXP + updPlayer->m.pos[i] - bctx.transf.pos[i];
		}

		// Convert `intermediate` to use `bctx`'s rotation
		iquat invRot;
		iquat_inv(invRot, bctx.transf.rot);
		iquat_applySm(result, invRot, intermediate);
	}
	return result;
}

static dl_var *findVarByName(char const *name) {
	if (!currentGroup) return NULL;

	list<dl_var> &vars = currentGroup->vars;

	rangeconst(i, vars.num) {
		if (!strcmp(name, vars[i].name)) {
			return &vars[i];
		}
	}

	// Register new var
	if (strlen(name) >= DL_VARNAME_LEN) {
		printf("new var name \"%s\" is too long!\n", name);
		return NULL;
	}
	if (strchr(name, ' ')) {
		// There's actually lots of stuff you shouldn't include -
		// anything that requires a backslash escape -
		// but spaces are probably the most likely troublemaker.
		printf("new var name \"%s\" may not contain spaces!\n", name);
		return NULL;
	}

	// `currentGroup != NULL` implies that we are locked,
	// so we can go resizing lists if we need to.
	dl_var *x = &vars.add();
	strcpy(x->name, name);
	x->seen = 1;
	// Vars default to touched so that any weird expressions (esp. `look`) get rewritten as constants.
	// May change this later.
	x->touched = 1;
	currentGroup->touched = 1;
	x->type = VAR_T_UNSET;

	return x;
}

// Unlike most non-static methods, we omit the "dl_" prefix.
// This method has a super-short name since it's intended for use in
// the dl'd source files for level editing.
int64_t var(char const *name) { return var(name, 0); }

int64_t var(char const *name, int64_t val) {
	dl_var *v = findVarByName(name);

	if (!v) return val;

	v->seen = 1;

	if (v->type == VAR_T_UNSET) { // New var
		v->type = VAR_T_INT;
		v->incr = 1;
		v->value.integer = val;
	}

	return v->value.integer;
}

int64_t const * pvar(char const *name) {
	return pvar(name, (int64_t const[]){0,0,0});
}

int64_t const * pvar(char const *name, offset const val) {
	dl_var *v = findVarByName(name);

	if (!v) return val;

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
	dl_var *v = findVarByName(name);

	if (!v) return val;

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
	processUpd(gs, myPlayer, 1);
}

void dl_upd(gamestate *gs, int myPlayer) {
	processUpd(gs, myPlayer, 0);
}

void dl_lookAtGp(gamestate *gs, int myPlayer) {
	lookAtGp_winner[0] = '\0';
	// I think I could probably get this to be +Inf instead,
	// but since my bootleg `Inf`s compare weirdly with each
	// other, I'd have to adjust some of the raycasting code
	// to avoid that case. This very large number works just
	// as well for our purposes.
	lookAtGp_best = (fraction){.numer = INT64_MAX/FIXP, .denom = 1};
	memcpy(lookAtGp_origin, gs->players[myPlayer].m.pos, sizeof(lookAtGp_origin));
	getLookUnitvec(lookAtGp_dir);

	bctx.solidCallback = lookAtGp_test;
	processUpd(gs, myPlayer, 0);
	bctx.solidCallback = NULL;

	// Re-locking this right after `processUpd` is a bit silly (we *just* had this lock),
	// but this part doesn't really need to be performant.
	mtx_lock(dl_varMtx);
	dl_selectedGroup = findGroup(lookAtGp_winner);
	if (!dl_selectedGroup) {
		puts("ERROR: That's really not supposed to happen");
		dl_selectedGroup = &varGroups[0];
	}
	selectedVarFix();
	mtx_unlock(dl_varMtx);
}

void dl_bake() {
	if (!editEventsFifo) return;
	fputs("/bake\n", editEventsFifo);
	rangeconst(i, varGroups.num) {
		dl_varGroup &g = varGroups[i];
		if (!g.touched) continue;
		g.touched = 0;
		fprintf(editEventsFifo, "gp %s\n", g.name);
		rangeconst(j, g.vars.num) {
			dl_var &v = g.vars[j];
			if (!v.touched) continue;
			v.touched = 0;
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
		fputs("\n", editEventsFifo);
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
	varGroups.init();

	locked = 1; // This is a lie, but it is true that we're not going to have mutex contention at this time!
	gp("");
	if (varGroups.num != 1) {
		puts("ERROR: dl: startup assertion failed");
		exit(1);
	}
	addDummyForGroup(0);
	dl_selectedGroup = &varGroups[0];
	currentGroup = NULL; // Have to reset this after our call to `gp` earlier
	locked = 0;

	editEventsFifo = fopen("edit_events.fifo", "w");
	if (!editEventsFifo) {
		if (errno == ENOENT) {
			puts("Couldn't find edit_events.fifo, assuming no edit mode");
		} else {
			perror("Failed to open edit_events.fifo");
		}
	}
}

void dl_destroy() {
	if (editEventsFifo) fclose(editEventsFifo);

	rangeconst(i, varGroups.num) varGroups[i].destroy();
	varGroups.destroy();
}
