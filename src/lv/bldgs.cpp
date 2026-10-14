#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../constel.h"
#include "../dl_helpers.h"
#include "../random.h"

#include "../tasks/killPlane.h"
#include "../tasks/rails.h"
#include "../tasks/tdmScore.h"

static void bldg(gamestate *gs);

static void addSpawn(tskTdmData *data, offset const o);
static void addLaunch(gamestate *gs, int32_t strike, int64_t dist, int32_t skip);
static void elevator1f(gamestate *gs, char addPad);
static void elevator2f(gamestate *gs, char addPad);

static void room_conference(gamestate *gs);
static void room_cubicle(gamestate *gs);
static void room_vault(gamestate *gs);
static void room_omen(gamestate *gs);

static void (*room_fns[4])(gamestate *gs) = {
	&room_conference,
	&room_cubicle,
	&room_vault,
	&room_omen,
};

static offset const perFloorSpawns[8] = {
	{-1000, -8900, 900},
	{-8900, -8900, 900},
	{-8900, 0, 900},
	{-8900, 8900, 900},
	{-1000, 8900, 900},
	{6900, 8900, 900},
	{6900, 0, 900},
	{6900, -8900, 900},
};

static tskTdmData *tdmData;
static constel *launch;

//#name lv_bldgs
extern "C" void lv_bldgs(gamestate *gs) {

	bctx.reset(gs);
	char strictness = var("respawn", 1) ? 0 : -1;
	prepareGamestateForLoad(gs, strictness);
	coreSetup(gs);

	uint32_t init_seed = var("seed", 0);
	if (init_seed) gs->seed = init_seed;

	taskKillPlane_create(gs, var("kill_depth", -30000));
	tdmData = NULL;
	if (var("tdm_active", 1)) {
		tdmData = taskTdm_create(gs, var("tdm_limit", 5));
	}

	gp("launch");
	launch = mkConstel();
	// Kind of silly, but for now the only thing that can be animated is constels,
	// so we have a constel with a single solid.
	bctx.addPt(launch, svar("size", (offset const){2000, 2000, 200}), var("tex", 8));
	launch->estimateRadius();

	bctx.push();

	gp("gnd.0");
	bctx.pos(pvar("pos", (offset const){-6000, -2900, -3400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 1429, -1429}));
	bctx.add(svar("size", (offset const){50000, 50000, 1000}), var("tex", 7));
	bctx.peek();

	gp("gnd.1");
	bctx.pos(pvar("pos", (offset const){-6000, 46800, -1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, -1429}));
	bctx.add(svar("size", (offset const){50000, 50000, 1000}), var("tex", 7));
	bctx.peek();

	gp("gnd.2");
	bctx.pos(pvar("pos", (offset const){43860, -3080, -1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 1429, 0}));
	bctx.add(svar("size", (offset const){50000, 50000, 1000}), var("tex", 7));
	bctx.peek();

	gp("gnd.3");
	bctx.pos(pvar("pos", (offset const){43860, 46780, 1000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){50000, 50000, 1000}), var("tex", 7));
	bctx.peek();

	gp("bldg.0");
	bctx.pos(pvar("pos", (offset const){6000, 3000, 700}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bldg(gs);
	bctx.peek();

	gp("bldg.1");
	bctx.pos(pvar("pos", (offset const){0, 42000, 1300}));
	bctx.rot(rvar("rot", (int32_t const[]){28378, 0, 0}));
	bldg(gs);
	bctx.peek();

	gp("bldg.2");
	bctx.pos(pvar("pos", (offset const){36000, 16000, 1600}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bldg(gs);
	bctx.peek();

	//#add_here

	/*#1
	//#gp
	bctx.pos(pvar("pos", look(3000)));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size"), var("tex", 4));
	bctx.peek();

	*/
	/*#2
	//#gp s
	addSpawn(tdmData, pvar("pos", look(3000)));

	*/
	/*#3
	//#gp launch
	bctx.pos(pvar("pos", look(3000)));
	bctx.rot(rvar("rot"));
	addLaunch(gs, var("strike"), var("dist"), var("skip"));
	bctx.peek();

	*/

	taskTdm_spawnAll(gs, tdmData);

	launch->decr();
	launch = NULL;
	tdmData = NULL;
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

static void addLaunch(gamestate *gs, int32_t strike, int64_t dist, int32_t skip) {
	bctx.push();

	// This one will still have the old group active from the
	// game's perspective, so the result can be "clicked on" in the editor.
	// The python sidecar will disagree about the active group here,
	// but we don't have any variables here, so it doesn't matter.
	constelInst *ci = bctx.add(launch, 1);

	gp("launch");
	tskRailsData *railsData = tskRails_create(gs, ci);

	bctx.pos(0, 0, -var("windup_dist", 190));
	bctx.addPt(railsData, var("windup", 30));
	bctx.peek();

	bctx.pos(0, 0, dist);
	bctx.addPt(railsData, strike);
	bctx.peek();

	bctx.addPt(railsData, var("s+r", 15)-strike);
	bctx.addPt(railsData, var("wait", 30));

	if (skip) {
		railsData->time += skip;
		tskRails_timeHelper(railsData);
	}

	bctx.pop();
}

static void elevator1f(gamestate *gs, char addPad) {
	pushVarIgnore();
	bctx.push();

	if (addPad) {
		gp("elev.f1.pad");
		bctx.pos(pvar("pos", (offset const){0, 0, -1000}));
		bctx.add(svar("size", (offset const){2200, 2200, 2000}), var("tex", 5));
		bctx.peek();
	}

	gp("elev.f1.launch");
	bctx.pos(pvar("pos", (offset const){0, 0, 100}));
	addLaunch(gs, var("strike", 1), var("dist", 660), var("skip", 0));
	bctx.peek();

	gp("elev.f1.cap");
	bctx.pos(pvar("pos", (offset const){700, 0, 8700}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 12540}));
	bctx.add(svar("size", (offset const){2000, 2000, 200}), var("tex", 8));

	bctx.pop();
	popVarIgnore();
}

static void elevator2f(gamestate *gs, char addPad) {
	pushVarIgnore();
	bctx.push();

	if (addPad) {
		gp("elev.f2.pad");
		bctx.pos(pvar("pos", (offset const){0, 0, -1000}));
		bctx.add(svar("size", (offset const){2200, 2200, 2000}), var("tex", 5));
		bctx.peek();
	}

	gp("elev.f2.launch");
	bctx.pos(pvar("pos", (offset const){0, 0, 100}));
	addLaunch(gs, var("strike", 1), var("dist", 900), var("skip", 0));
	bctx.peek();

	gp("elev.f2.cap");
	bctx.pos(pvar("pos", (offset const){700, 0, 15700}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 12540}));
	bctx.add(svar("size", (offset const){2000, 2000, 200}), var("tex", 8));
	bctx.peek();

	bctx.pop();
	popVarIgnore();
}

static void room_conference(gamestate *gs) {
	bctx.push();

	gp("conftab");
	bctx.pos(pvar("pos", (offset const){200, 0, 500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){4100, 13200, 400}), var("tex", 8));
	bctx.peek();

	gp("seat.0");
	bctx.pos(pvar("pos", (offset const){-3500, 5000, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.0");
	bctx.pos(pvar("pos", (offset const){-4400, 5000, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.1");
	bctx.pos(pvar("pos", (offset const){-3500, 1500, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.1");
	bctx.pos(pvar("pos", (offset const){-4400, 1500, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.2");
	bctx.pos(pvar("pos", (offset const){-3500, -2000, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.2");
	bctx.pos(pvar("pos", (offset const){-4400, -2000, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.3");
	bctx.pos(pvar("pos", (offset const){-3500, -5500, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.3");
	bctx.pos(pvar("pos", (offset const){-4400, -5500, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.4");
	bctx.pos(pvar("pos", (offset const){3900, 5000, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.4");
	bctx.pos(pvar("pos", (offset const){4800, 5000, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.5");
	bctx.pos(pvar("pos", (offset const){3900, 1500, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.5");
	bctx.pos(pvar("pos", (offset const){4800, 1500, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.6");
	bctx.pos(pvar("pos", (offset const){3900, -2000, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.6");
	bctx.pos(pvar("pos", (offset const){4800, -2000, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.7");
	bctx.pos(pvar("pos", (offset const){3900, -5500, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.7");
	bctx.pos(pvar("pos", (offset const){4800, -5500, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	gp("seat.8");
	bctx.pos(pvar("pos", (offset const){200, 7900, 50}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){100, 1800, 1800}), var("tex", 2));
	bctx.peek();

	gp("sbak.8");
	bctx.pos(pvar("pos", (offset const){200, 8800, 1400}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 23170, 0}));
	bctx.add(svar("size", (offset const){100, 2600, 1800}), var("tex", 2));
	bctx.peek();

	bctx.pop();
}

static void room_cubicle(gamestate *gs) {
	bctx.push();

	gp("cubicle.0");
	bctx.pos(pvar("pos", (offset const){-1000, 0, 1150}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 14600, 2300}), var("tex", 9));
	bctx.peek();

	gp("cubicle.1");
	bctx.pos(pvar("pos", (offset const){-1000, -7500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){400, 10000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.2");
	bctx.pos(pvar("pos", (offset const){-1000, -2500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){400, 10000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.3");
	bctx.pos(pvar("pos", (offset const){-1000, 2500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){400, 10000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.4");
	bctx.pos(pvar("pos", (offset const){-1000, 7500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){400, 10000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.5");
	bctx.pos(pvar("pos", (offset const){-6200, -7200, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.6");
	bctx.pos(pvar("pos", (offset const){-6200, -2500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1600, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.7");
	bctx.pos(pvar("pos", (offset const){-6200, 2500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1600, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.8");
	bctx.pos(pvar("pos", (offset const){-6200, 7200, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.9");
	bctx.pos(pvar("pos", (offset const){4100, -7200, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.10");
	bctx.pos(pvar("pos", (offset const){4200, -2500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1600, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.11");
	bctx.pos(pvar("pos", (offset const){4200, 2500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1600, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.12");
	bctx.pos(pvar("pos", (offset const){4200, 7200, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){400, 1000, 2400}), var("tex", 9));
	bctx.peek();

	gp("cubicle.13");
	bctx.pos(pvar("pos", (offset const){-1000, 0, 600}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){3900, 14600, 300}), var("tex", 8));
	bctx.peek();

	gp("mon.0");
	bctx.pos(pvar("pos", (offset const){-1951, 3858, 1250}));
	bctx.rot(rvar("rot", (int32_t const[]){19948, 0, 0}));
	bctx.add(svar("size", (offset const){1400, 200, 1000}), var("tex", 8));
	bctx.peek();

	gp("mon.1");
	bctx.pos(pvar("pos", (offset const){-1251, 358, 1545}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 0}));
	bctx.add(svar("size", (offset const){1400, 200, 1000}), var("tex", 8));
	bctx.peek();

	gp("mon.2");
	bctx.pos(pvar("pos", (offset const){-1751, -4142, 1250}));
	bctx.rot(rvar("rot", (int32_t const[]){25997, 0, 0}));
	bctx.add(svar("size", (offset const){1400, 200, 1000}), var("tex", 8));
	bctx.peek();

	gp("mon.3");
	bctx.pos(pvar("pos", (offset const){-1751, -5642, 1250}));
	bctx.rot(rvar("rot", (int32_t const[]){19948, 0, 0}));
	bctx.add(svar("size", (offset const){1400, 200, 1000}), var("tex", 8));
	bctx.peek();

	gp("mon.4");
	bctx.pos(pvar("pos", (offset const){-351, -5642, 1250}));
	bctx.rot(rvar("rot", (int32_t const[]){25997, 0, 0}));
	bctx.add(svar("size", (offset const){1400, 200, 1000}), var("tex", 8));
	bctx.peek();

	gp("mon.5");
	bctx.pos(pvar("pos", (offset const){-751, 0, 1330}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, -286}));
	bctx.add(svar("size", (offset const){1400, 200, 1000}), var("tex", 8));
	bctx.peek();

	bctx.pop();
}

static void room_vault(gamestate *gs) {
	bctx.push();

	gp("vault.0");
	bctx.pos(pvar("pos", (offset const){-3100, -3000, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){10000, 1000, 5000}), var("tex", 8));
	bctx.peek();

	gp("vault.1");
	bctx.pos(pvar("pos", (offset const){4800, -7500, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){10000, 1000, 5000}), var("tex", 8));
	bctx.peek();

	gp("vault.2");
	bctx.pos(pvar("pos", (offset const){-3100, -8000, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){9000, 1000, 5000}), var("tex", 8));
	bctx.peek();

	gp("vault.3");
	bctx.pos(pvar("pos", (offset const){-3100, 3000, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){10000, 1000, 5000}), var("tex", 8));
	bctx.peek();

	gp("vault.4");
	bctx.pos(pvar("pos", (offset const){4800, 7500, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){10000, 1000, 5000}), var("tex", 8));
	bctx.peek();

	gp("vault.5");
	bctx.pos(pvar("pos", (offset const){-3100, 8000, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, 0}));
	bctx.add(svar("size", (offset const){9000, 1000, 5000}), var("tex", 8));
	bctx.peek();

	bctx.pop();
}

static void room_omen(gamestate *gs) {
	bctx.push();

	gp("omen");
	bctx.pos(pvar("pos", (offset const){0, 0, 3500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){3000, 3000, 3000}), var("tex", 0));
	bctx.peek();

	gp("shutter.0");
	bctx.pos(pvar("pos", (offset const){-12400, 6400, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 0}));
	bctx.add(svar("size", (offset const){4800, 1000, 5000}), var("tex", 5));
	bctx.peek();

	gp("shutter.1");
	bctx.pos(pvar("pos", (offset const){-12400, -6400, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 0}));
	bctx.add(svar("size", (offset const){4800, 1000, 5000}), var("tex", 5));
	bctx.peek();

	gp("shutter.2");
	bctx.pos(pvar("pos", (offset const){-6400, -12400, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){4800, 1000, 5000}), var("tex", 5));
	bctx.peek();

	gp("shutter.3");
	bctx.pos(pvar("pos", (offset const){6400, -12400, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){4800, 1000, 5000}), var("tex", 5));
	bctx.peek();

	gp("shutter.4");
	bctx.pos(pvar("pos", (offset const){6400, 12400, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){4800, 1000, 5000}), var("tex", 5));
	bctx.peek();

	gp("shutter.5");
	bctx.pos(pvar("pos", (offset const){-6400, 12400, 2500}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){4800, 1000, 5000}), var("tex", 5));
	bctx.peek();

	bctx.pop();
}

static void bldg(gamestate *gs) {
	bctx.push();

	gp("floor.0");
	bctx.pos(pvar("pos", (offset const){0, 0, -2250}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){25000, 25000, 4500}), var("tex", 5));
	bctx.peek();

	gp("floor.1");
	bctx.pos(pvar("pos", (offset const){-1100, 0, 6000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){22800, 25000, 2000}), var("tex", 5));
	bctx.peek();

	gp("floor.2");
	bctx.pos(pvar("pos", (offset const){-1100, 0, 13000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){22800, 25000, 2000}), var("tex", 5));
	bctx.peek();

	gp("floor.3");
	bctx.pos(pvar("pos", (offset const){-1100, 0, 20000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){22800, 25000, 2000}), var("tex", 5));
	bctx.peek();

	gp("floor.4");
	bctx.pos(pvar("pos", (offset const){-1100, 0, 27000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(svar("size", (offset const){22800, 25000, 2000}), var("tex", 5));
	bctx.peek();

	gp("pillar.0");
	bctx.pos(pvar("pos", (offset const){-11600, 11600, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 2600}), var("tex", 5));
	bctx.peek();

	gp("pillar.1");
	bctx.pos(pvar("pos", (offset const){-11600, 0, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 5000}), var("tex", 5));
	bctx.peek();

	gp("pillar.2");
	bctx.pos(pvar("pos", (offset const){-11600, -11600, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 2600}), var("tex", 5));
	bctx.peek();

	gp("pillar.3");
	bctx.pos(pvar("pos", (offset const){0, -11600, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 5000}), var("tex", 5));
	bctx.peek();

	gp("pillar.4");
	bctx.pos(pvar("pos", (offset const){11600, -11600, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 2600}), var("tex", 5));
	bctx.peek();

	gp("pillar.5");
	bctx.pos(pvar("pos", (offset const){11600, 0, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 5000}), var("tex", 5));
	bctx.peek();

	gp("pillar.6");
	bctx.pos(pvar("pos", (offset const){11600, 11600, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 2600}), var("tex", 5));
	bctx.peek();

	gp("pillar.7");
	bctx.pos(pvar("pos", (offset const){0, 11600, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 2600, 5000}), var("tex", 5));
	bctx.peek();

	gp("elev.2f.0");
	bctx.pos(pvar("pos", (offset const){11400, -9200, 0}));
	elevator2f(gs, 0);
	bctx.peek();

	gp("elev.2f.1");
	bctx.pos(pvar("pos", (offset const){11400, -3600, 14000}));
	elevator2f(gs, 1);
	bctx.peek();

	gp("elev.1f.0");
	bctx.pos(pvar("pos", (offset const){11400, 3600, 0}));
	elevator1f(gs, 0);
	bctx.peek();

	gp("elev.1f.1");
	bctx.pos(pvar("pos", (offset const){11400, 9200, 7000}));
	elevator1f(gs, 1);
	bctx.peek();

	gp("elev.1f.2");
	bctx.pos(pvar("pos", (offset const){11400, 3600, 14000}));
	elevator1f(gs, 1);
	bctx.peek();

	gp("elev.1f.3");
	bctx.pos(pvar("pos", (offset const){11400, 9200, 21000}));
	elevator1f(gs, 1);
	bctx.peek();

	gp("rail.0");
	bctx.pos(pvar("pos", (offset const){12700, -9000, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 400, 400}), var("tex", 8));
	bctx.peek();

	gp("rail.1");
	bctx.pos(pvar("pos", (offset const){12700, -3800, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 400, 400}), var("tex", 8));
	bctx.peek();

	gp("rail.2");
	bctx.pos(pvar("pos", (offset const){12700, 3800, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 400, 400}), var("tex", 8));
	bctx.peek();

	gp("rail.3");
	bctx.pos(pvar("pos", (offset const){12700, 9000, 15000}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 23170}));
	bctx.add(svar("size", (offset const){30000, 400, 400}), var("tex", 8));
	bctx.peek();

	range(i, 4) {
		(*room_fns[splitmix32(&gs->seed)%4])(gs);
		addSpawn(tdmData, perFloorSpawns[splitmix32(&gs->seed)%8]);
		bctx.pos(0, 0, 7000);
	}

	bctx.pop();
}
