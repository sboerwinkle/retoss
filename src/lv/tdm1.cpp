#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

#include "../comp/house1.h"
#include "../comp/house2.h"
#include "../comp/trench1.h"

#include "../tasks/tdmScore.h"

void lv_tdm1(gamestate *gs) {
	pushVarIgnore();
	bctx.reset(gs);

	gp("base");
	bctx.push();
	{
		int64_t w = var("width", 80000);
		bctx.pos(0, 0, -w/8);
		bctx.add(1, var("sprite", 7), w);
	}

	gp("house2");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){7100, -3000, 0}));
	house2(gs, var("door", 1600), rvar("rot", (int32_t const[]){8481, 23170, 0}));

	gp("t1");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){6900, -10800, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){32768, 0, 0}));
	trench1();

	gp("1");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){6900, -29800, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){32768, 0, 0}));
	trench1();

	gp("2");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){2100, -7300, 1600}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(var("shape", 1), var("sprite", 5), var("scale", 1800));

	gp("3");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-1500, -7300, 1600}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(var("shape", 1), var("sprite", 5), var("scale", 1800));

	gp("4");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-8700, -12900, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	trench1();

	gp("house1");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-5600, -32200, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){4277, 0, 0}));
	house1(var("wall", 3800), var("door", 3200));

	gp("ramp");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-23100, -14000, 4300}));
	bctx.rot(rvar("rot", (int32_t const[]){32768, 8481, 0}));
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 11000));

	gp("ramp.wall");
	bctx.pos(pvar("pos", (offset const){-2750, 0, 2750}));
	bctx.add(var("shape", 2), var("sprite", 8), var("scale", 11000));

	gp("ramp.2");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){21300, -14000, 4300}));
	bctx.rot(rvar("rot", (int32_t const[]){0, -8481, 0}));
	bctx.add(var("shape", 2), var("sprite", 5), var("scale", 11000));

	gp("ramp.wall");
	bctx.pos(pvar("pos", (offset const){-2750, 0, 2750}));
	bctx.add(var("shape", 2), var("sprite", 8), var("scale", 11000));

	gp("stem.1");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-13500, -24200, 9600}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 0}));
	bctx.add(var("shape", 2), var("sprite", 8), var("scale", 11000));

	gp("stem.2");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){11680, -24200, 9600}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 0}));
	bctx.add(var("shape", 2), var("sprite", 8), var("scale", 11000));

	gp("leaf.1");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-16700, -31070, 10270}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("sprite", 8), var("scale", 5500));

	gp("leaf.2");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){-6800, -17330, 10270}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("sprite", 8), var("scale", 5500));

	gp("leaf.3");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){1700, -31070, 10270}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("sprite", 8), var("scale", 5500));

	gp("leaf.4");
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){10300, -17330, 10270}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("sprite", 8), var("scale", 5500));

	//#add_here

	// Spawner setup.
	tskTdmData *data = NULL;
	// This loop will never find anything when loading the level,
	// but for now it's here to help things run smoothly when
	// pasting this into the dl_tmp folder for "editing".
	rangeconst(i, gs->tasks.num) {
		if (gs->tasks[i].defn->id == TSK_TDM) {
			data = (tskTdmData*)gs->tasks[i].data;
			break;
		}
	}
	if (!data) {
		data = taskTdm_create(gs, 2, 10);
	}

	gp("spawns");
	memcpy(
		data->spawns[0],
		pvar("p1", (offset const){-23177, -16393, 1000}),
		sizeof(offset)
	);
	memcpy(
		data->spawns[1],
		pvar("p2", (offset const){21263, -16393, 1000}),
		sizeof(offset)
	);

	taskTdm_spawnAll(gs, data);

	popVarIgnore();

	/*#1
	gp();
	bctx.peek();
	bctx.pos(pvar("pos", look(5000)));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(var("shape", 1), var("sprite", 5), var("scale", 1600));

	 */
	/*#2
	gp();
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){0,0,1600}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(var("shape", 1), 5, var("scale", 1600));

	 */
	/*#3
	gp();
	bctx.peek();
	bctx.pos(pvar("pos", look(4000)));
	bctx.rot(rvar("rot"));
	trench1();

	 */

}
