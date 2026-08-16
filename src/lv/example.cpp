#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"

#include "../tasks/killPlane.h"
#include "../tasks/tdmScore.h"

static void addSpawn(tskTdmData *data, offset const o);

//#name lv_example
extern "C" void lv_example(gamestate *gs) {
	// When this is loaded as an actual level,
	// it is always given a clean slate, so
	// clearing it again is redundant. We have
	// to do it here for the edit flow, however,
	// which makes fewer assumptions about what
	// you want cleared. Maybe the convention
	// will change to have `lv`s do their own
	// cleanup?

	bctx.reset(gs);
	prepareGamestateForLoad(gs, 0);
	coreSetup(gs);

	taskKillPlane_create(gs, var("kill_depth", -30000));
	tskTdmData *tdmData = NULL;
	if (var("tdm_active", 1)) {
		tdmData = taskTdm_create(gs, var("tdm_limit", 7));
	}

	bctx.push();

	//#add_here

	/*#1
	gp();
	bctx.pos(pvar("pos", look(3000)));
	bctx.rot(rvar("rot"));
	bctx.add(var("shape"), var("tex", 4), var("scale", 1000));
	bctx.peek();

	*/
	/*#2
	gp();
	addSpawn(tdmData, pvar("pos", look(3000)));

	*/
}

static void addSpawn(tskTdmData *data, offset const o) {
	bctx.push();

	bctx.pos(o);

	if (data) {
		bctx.finalizeTranslate();
		taskTdm_addSpawn(data, bctx.transf.pos);
	} else {
		bctx.add(0, 3, PLAYER_SHAPE_RADIUS);
	}

	bctx.pop();
}
