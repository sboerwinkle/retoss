#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"

#include "../tasks/killPlane.h"
#include "../tasks/tdmScore.h"

static void addSpawn(tskTdmData *data, offset const o);

void lv_heightmap(gamestate *gs, char const *data, int len) {
	bctx.reset(gs);
	prepareGamestateForLoad(gs, 0);
	coreSetup(gs);

	taskKillPlane_create(gs, -20000);
	tskTdmData *tdmData = taskTdm_create(gs, 5);

	bctx.push();

	bctx.pos((offset const){0, 0, -2000});
	//bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	//bctx.add(var("shape", 0), var("tex", 4), var("scale", 1000));
	bctx.add(0, 4, 1000);
	bctx.peek();

	addSpawn(tdmData, (offset const){0,0,0});
	addSpawn(tdmData, (offset const){0,0,0});

	taskTdm_spawnAll(gs, tdmData);
}

static void addSpawn(tskTdmData *data, offset const o) {
	bctx.push();

	bctx.pos(o);

	bctx.finalizeTranslate();
	taskTdm_addSpawn(data, bctx.transf.pos);

	bctx.pop();
}
