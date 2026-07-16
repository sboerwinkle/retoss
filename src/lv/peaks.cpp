#include <stdio.h>

#include "../gamestate.h"
#include "../constel.h"
#include "../bctx.h"
#include "../dl_helpers.h"

#include "../tasks/killPlane.h"
#include "../tasks/rails.h"
#include "../tasks/tdmScore.h"
#include "../comp/plank.h"

// Could I make this struct anon? Too scared to try rn
struct blah { offset pos; };
static list<blah> spawns;

static void addSpawn(offset const o) {
	bctx.pos(o);
	bctx.finalizeTranslate();
	int64_t *dest = spawns.add().pos;
	memcpy(dest, bctx.transf.pos, sizeof(offset));

	// Technically this makes some assumptions about how the method is called,
	// but it's a static function so it's not as big a deal
	bctx.peek();
}

static void trolleyHalf(constel *c) {
	bctx.push();
	gp("b");
	bctx.pos(pvar("pos", (offset const){2400, 0, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.addPt(c, var("shape", 1), var("tex", 8), var("scale", 1200));
	bctx.peek();
	gp("c");
	bctx.pos(pvar("pos", (offset const){2400, 1350, 1350}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.addPt(c, var("shape", 1), var("tex", 8), var("scale", 1200));
	bctx.peek();
	gp("d");
	bctx.pos(pvar("pos", (offset const){2400, -1350, 1350}));
	bctx.rot(rvar("rot", (int32_t const[]){32768, 23170, 0}));
	bctx.addPt(c, var("shape", 1), var("tex", 8), var("scale", 1200));
	bctx.peek();
	gp("e");
	bctx.pos(pvar("pos", (offset const){3750, 0, 1350}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.addPt(c, var("shape", 1), var("tex", 8), var("scale", 1200));
	bctx.peek();

	gp("wing");
	bctx.pos(pvar("pos", (offset const){0, 2700, 200}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 0, 0}));
	bctx.addPt(c, var("shape", 2), var("tex", 8), var("scale", 1200));
	bctx.peek();

	bctx.pop();
}

static void addHangarDoorRoute(tskRailsData *data) {
	tskRailsInstructions *instr = data->instr;
	gp("rte_door");
	int32_t dwellTime = var("dwell_t", 135);
	int32_t travelTime = var("travel_t", 10);
	bctx.addPt(instr, travelTime);
	bctx.addPt(instr, dwellTime);
	bctx.pos(0, -5286*2, 0);
	bctx.addPt(instr, travelTime);
	bctx.addPt(instr, dwellTime);
	data->time = var("time", 0);
}

static void addTrolleyRoute(tskRailsData *data) {
	tskRailsInstructions *instr = data->instr;
	gp("rte_trolley");
	int32_t boardTime = var("board_t", 90);
	int32_t turnTime = var("turn_t", 22);
	int32_t dwellTime = var("dwell_t", 11);
	int32_t travelTime = var("travel_t", 290);
	int32_t const *turn90 = (int32_t const[]){23170, 0, 0};
	bctx.push();
	iquat baseRot;
	memcpy(baseRot, bctx.transf.rot, sizeof(iquat));

	bctx.addPt(instr, travelTime);
	bctx.addPt(instr, boardTime);
	bctx.rot(turn90);
	bctx.addPt(instr, turnTime);
	bctx.rot(turn90);
	bctx.addPt(instr, turnTime);
	bctx.addPt(instr, dwellTime);
	bctx.pos(pvar("pos", (offset const){68000, 2000, 0}));

	bctx.addPt(instr, travelTime);
	bctx.addPt(instr, boardTime);
	bctx.rot(turn90);
	bctx.addPt(instr, turnTime);
	// Restore previous rotation exactly
	memcpy(bctx.transf.rot, baseRot, sizeof(iquat));
	bctx.addPt(instr, turnTime);
	bctx.addPt(instr, dwellTime);
	bctx.pop();

	data->time = var("time", 5);
}

static void mountain(gamestate *gs, constel *trolley, constel *bigPlate, int32_t trolleyStagger) {
	bctx.push();
	gp("2");
	bctx.pos(pvar("pos", (offset const){39326, -44, 575}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 1429, -286}));
	bctx.add(var("shape", 1), var("tex", 9), var("scale", 30000));
	bctx.peek();
	gp("3");
	bctx.pos(pvar("pos", (offset const){39280, 224, 8847}));
	bctx.rot(rvar("rot", (int32_t const[]){6252, 0, 858}));
	bctx.add(var("shape", 0), var("tex", 7), var("scale", 15650));
	bctx.peek();
	gp("4");
	bctx.pos(pvar("pos", (offset const){24457, 15635, 11075}));
	bctx.rot(rvar("rot", (int32_t const[]){-28660, 11743, 0}));
	bctx.add(var("shape", 1), var("tex", 9), var("scale", 10900));
	bctx.peek();
	gp("5");
	bctx.pos(pvar("pos", (offset const){42709, 18982, 15585}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 0), var("tex", 7), var("scale", 5800));
	bctx.peek();
	gp("6");
	bctx.pos(pvar("pos", (offset const){20518, 14016, 10377}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 8481, 4277}));
	bctx.add(var("shape", 0), var("tex", 4), var("scale", 1680));
	bctx.peek();
	gp("7");
	bctx.pos(pvar("pos", (offset const){26571, 23025, 10290}));
	bctx.rot(rvar("rot", (int32_t const[]){19948, -4277, 0}));
	bctx.add(var("shape", 0), var("tex", 4), var("scale", 1400));
	bctx.peek();
	gp("8");
	bctx.pos(pvar("pos", (offset const){36180, 16735, 19750}));
	bctx.rot(rvar("rot", (int32_t const[]){31499, 5408, 3709}));
	bctx.add(var("shape", 1), var("tex", 4), var("scale", 2860));
	bctx.peek();
	gp("9");
	bctx.pos(pvar("pos", (offset const){47149, 20624, 21800}));
	bctx.rot(rvar("rot", (int32_t const[]){-18795, 858, 1715}));
	bctx.add(var("shape", 2), var("tex", 8), var("scale", 5800));
	bctx.peek();
	gp("10");
	bctx.pos(pvar("pos", (offset const){48988, 21580, 11511}));
	bctx.rot(rvar("rot", (int32_t const[]){31651, 19948, 0}));
	bctx.add(var("shape", 2), var("tex", 8), var("scale", 5700));
	bctx.peek();
	gp("11");
	bctx.pos(pvar("pos", (offset const){53376, 9482, 16134}));
	bctx.rot(rvar("rot", (int32_t const[]){6813, -13328, 23965}));
	bctx.add(var("shape", 1), var("tex", 9), var("scale", 6345));
	bctx.peek();
	gp("12");
	bctx.pos(pvar("pos", (offset const){58104, 3443, 7818}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 0), var("tex", 9), var("scale", 3940));
	bctx.peek();
	gp("13");
	bctx.pos(pvar("pos", (offset const){54805, 9764, 7309}));
	bctx.rot(rvar("rot", (int32_t const[]){-4560, 0, 0}));
	bctx.add(var("shape", 0), var("tex", 9), var("scale", 3000));
	bctx.peek();
	gp("14");
	bctx.pos(pvar("pos", (offset const){58969, -3712, 11980}));
	bctx.rot(rvar("rot", (int32_t const[]){8481, -9580, 0}));
	bctx.add(var("shape", 0), var("tex", 9), var("scale", 2130));
	bctx.peek();
	gp("15");
	bctx.pos(pvar("pos", (offset const){55934, 2886, 13479}));
	bctx.rot(rvar("rot", (int32_t const[]){6252, -11207, 0}));
	bctx.add(var("shape", 2), var("tex", 9), var("scale", 3600));
	bctx.peek();
	gp("16");
	bctx.pos(pvar("pos", (offset const){53130, -14077, 6442}));
	bctx.rot(rvar("rot", (int32_t const[]){-10397, -286, -572}));
	bctx.add(var("shape", 0), var("tex", 9), var("scale", 8700));
	bctx.peek();
	gp("17");
	bctx.pos(pvar("pos", (offset const){56159, -26799, 2779}));
	bctx.rot(rvar("rot", (int32_t const[]){-6813, 5690, 1429}));
	bctx.add(var("shape", 0), var("tex", 9), var("scale", 5100));
	bctx.peek();
	gp("18");
	bctx.pos(pvar("pos", (offset const){54402, -34408, 120}));
	bctx.rot(rvar("rot", (int32_t const[]){32488, 2000, 0}));
	bctx.add(var("shape", 1), var("tex", 9), var("scale", 4000));
	bctx.peek();
	gp("19");
	bctx.pos(pvar("pos", (offset const){54636, -32902, 925}));
	bctx.rot(rvar("rot", (int32_t const[]){32488, 9307, 0}));
	bctx.add(var("shape", 1), var("tex", 9), var("scale", 4000));
	bctx.peek();
	gp("20");
	bctx.pos(pvar("pos", (offset const){61602, -34908, 1530}));
	bctx.rot(rvar("rot", (int32_t const[]){-2856, -286, -858}));
	bctx.add(var("shape", 0), var("tex", 5), var("scale", 2650));
	bctx.peek();
	gp("21");
	bctx.pos(pvar("pos", (offset const){62574, -18542, 10073}));
	bctx.rot(rvar("rot", (int32_t const[]){-10126, -286, -19028}));
	bctx.add(var("shape", 1), var("tex", 9), var("scale", 6500));
	bctx.peek();
	gp("22");
	bctx.pos(pvar("pos", (offset const){49080, 24394, 17401}));
	bctx.rot(rvar("rot", (int32_t const[]){-12540, 0, 0}));
	bctx.add(var("shape", 0), var("tex", 4), var("scale", 1000));
	bctx.peek();
	gp("31");
	bctx.pos(pvar("pos", (offset const){28473, 13025, 22722}));
	bctx.rot(rvar("rot", (int32_t const[]){-12010, -1715, 0}));
	bctx.add(var("shape", 0), var("tex", 7), var("scale", 1800));
	bctx.peek();
	gp("32");
	bctx.pos(pvar("pos", (offset const){32235, 29257, 1601}));
	bctx.rot(rvar("rot", (int32_t const[]){-21926, 0, -3993}));
	bctx.add(var("shape", 2), var("tex", 7), var("scale", 26000));
	bctx.peek();
	gp("33");
	bctx.pos(pvar("pos", (offset const){25835, 32237, 251}));
	bctx.rot(rvar("rot", (int32_t const[]){-22763, 1715, -2571}));
	bctx.add(var("shape", 2), var("tex", 7), var("scale", 18000));
	bctx.peek();
	gp("34");
	bctx.pos(pvar("pos", (offset const){11098, 30493, 2062}));
	bctx.rot(rvar("rot", (int32_t const[]){-10126, -10938, -1429}));
	bctx.add(var("shape", 0), var("tex", 4), var("scale", 1500));
	bctx.peek();
	gp("35");
	bctx.pos(pvar("pos", (offset const){29694, 36824, 2998}));
	bctx.rot(rvar("rot", (int32_t const[]){32768, 5971, 0}));
	bctx.add(var("shape", 2), var("tex", 8), var("scale", 7200));
	bctx.peek();
	gp("36");
	bctx.pos(pvar("pos", (offset const){26703, -12737, 5332}));
	bctx.rot(rvar("rot", (int32_t const[]){-32110, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	gp("37");
	bctx.pos(pvar("pos", (offset const){24803, -12737, 9632}));
	bctx.rot(rvar("rot", (int32_t const[]){6252, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	gp("38");
	bctx.pos(pvar("pos", (offset const){26703, -12137, 13532}));
	bctx.rot(rvar("rot", (int32_t const[]){-32110, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	gp("39");
	bctx.pos(pvar("pos", (offset const){25103, -12737, 17132}));
	bctx.rot(rvar("rot", (int32_t const[]){5971, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	gp("40");
	bctx.pos(pvar("pos", (offset const){27143, -12137, 21632}));
	bctx.rot(rvar("rot", (int32_t const[]){-32110, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	gp("41");
	bctx.pos(pvar("pos", (offset const){55462, -11315, 17601}));
	bctx.rot(rvar("rot", (int32_t const[]){3993, 0, -572}));
	bctx.add(var("shape", 0), var("tex", 8), var("scale", 2500));
	bctx.peek();
	gp("42");
	bctx.pos(pvar("pos", (offset const){55315, -11473, 21683}));
	bctx.rot(rvar("rot", (int32_t const[]){3993, 0, -572}));
	bctx.add(var("shape", 0), var("tex", 8), var("scale", 2000));
	bctx.peek();
	gp("43");
	bctx.pos(pvar("pos", (offset const){7547, -13308, 1361}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 8481, 572}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 3800));
	bctx.peek();
	gp("44");
	bctx.pos(pvar("pos", (offset const){66833, 31524, -959}));
	bctx.rot(rvar("rot", (int32_t const[]){-8481, -7371, -2571}));
	bctx.add(var("shape", 0), var("tex", 7), var("scale", 5600));
	bctx.peek();
	gp("45");
	bctx.pos(pvar("pos", (offset const){41503, -18707, 17232}));
	bctx.rot(rvar("rot", (int32_t const[]){27166, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	gp("46");
	bctx.pos(pvar("pos", (offset const){41003, -16937, 21032}));
	bctx.rot(rvar("rot", (int32_t const[]){-18324, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	gp("47");
	bctx.pos(pvar("pos", (offset const){64844, 21327, 7332}));
	bctx.rot(rvar("rot", (int32_t const[]){3141, 1429, 0}));
	bctx.add(var("shape", 2), var("tex", 8), var("scale", 6000));
	bctx.peek();
	gp("49");
	bctx.pos(pvar("pos", (offset const){57246, -11240, 21925}));
	bctx.rot(rvar("rot", (int32_t const[]){25822, 22763, 0}));
	bctx.add(var("shape", 1), var("tex", 4), var("scale", 1000));
	bctx.peek();
	//#add_here

	// Taking a break from building the mound, let's build house now.

	gp("floor");
	bctx.pos(pvar("pos", (offset const){39800, 1000, 24100}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 10800));
	bctx.peek();
	gp("ciel");
	bctx.pos(pvar("pos", (offset const){39800, 1000, 39468}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 10800));
	bctx.peek();
	gp("24");
	bctx.pos(pvar("pos", (offset const){33950, -3950, 30300}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 4950));
	bctx.peek();
	gp("25");
	bctx.pos(pvar("pos", (offset const){33950, 5950, 30300}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 4950));
	bctx.peek();
	gp("26");
	bctx.pos(pvar("pos", (offset const){32600, 1000, 34520}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 3600));
	bctx.peek();
	gp("27");
	bctx.pos(pvar("pos", (offset const){39350, -2600, 34518}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 23170, 0}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 3600));
	bctx.peek();
	gp("28");
	bctx.pos(pvar("pos", (offset const){39350, 4600, 34518}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 23170, 0}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 3600));
	bctx.peek();
	gp("ramp");
	bctx.pos(pvar("pos", (offset const){41424, 10410, 27769}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 12540, 0}));
	bctx.add(var("shape", 2), var("tex", 8), var("scale", 4000));
	bctx.peek();
	gp("ramp_l");
	bctx.pos(pvar("pos", (offset const){41424, -8410, 27769}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 12540, 0}));
	bctx.add(var("shape", 2), var("tex", 8), var("scale", 4000));
	bctx.peek();
	gp("wall");
	bctx.pos(pvar("pos", (offset const){39800, 11690, 31784}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, -23170}));
	plank(
		var("width", 6335),
		var("height", 10800),
		var("tex", 5),
		var("pad", 80)
	);
	bctx.peek();
	gp("wall2");
	bctx.pos(pvar("pos", (offset const){39800, -9690, 31784}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 0, -23170}));
	plank(
		var("width", 6335),
		var("height", 10800),
		var("tex", 5),
		var("pad", 80)
	);
	bctx.peek();
	gp("29");
	bctx.pos(pvar("pos", (offset const){51385, -2560, 31784}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, -23170}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 6335));
	bctx.peek();
	gp("30");
	bctx.pos(pvar("pos", (offset const){51040, 7329, 29014}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, -23170}));
	bctx.add(var("shape", 1), var("tex", 5), var("scale", 3565));
	bctx.peek();
	gp("hall1");
	bctx.pos(pvar("pos", (offset const){28732, 2203, 27590}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, -23170}));
	plank(
		var("width", 2200),
		var("height", 8800),
		var("tex", 8),
		var("pad", 0)
	);
	bctx.peek();
	gp("hall2");
	bctx.pos(pvar("pos", (offset const){38932, -203, 27590}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, -23170}));
	plank(
		var("width", 2200),
		var("height", 8800),
		var("tex", 8),
		var("pad", 0)
	);
	bctx.peek();
	gp("48");
	bctx.pos(pvar("pos", (offset const){53953, -4146, 29852}));
	bctx.rot(rvar("rot", (int32_t const[]){23170, 21063, 0}));
	bctx.add(var("shape", 2), var("tex", 8), var("scale", 6100));
	bctx.peek();

	gp("d1");
	bctx.pos(pvar("pos", (offset const){28376, 6286, 35268}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, -23170}));
	constelInst *ci = bctx.add(bigPlate, 1);
	// TODO constelInst's should be selectable in-game
	addHangarDoorRoute(tskRails_create(gs, ci));
	bctx.peek();

	gp("t1");
	bctx.pos(pvar("pos", (offset const){34000, 6500, 31300}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	ci = bctx.add(trolley, 15);
	tskRailsData *trolleyRails = tskRails_create(gs, ci);
	addTrolleyRoute(trolleyRails);
	trolleyRails->time += trolleyStagger;
	bctx.peek();

	gp("s1");
	addSpawn(pvar("pos", (offset const){35521, 21027, 7585}));
	gp("s2");
	addSpawn(pvar("pos", (offset const){54119, 1706, 24789}));
	gp("s3");
	addSpawn(pvar("pos", (offset const){56374, -27692, 9180}));
	gp("s4");
	addSpawn(pvar("pos", (offset const){40793, 1815, 26463}));

	bctx.pop();
}

//extern "C" void lvlUpd(gamestate *gs) {
extern void lv_peaks(gamestate *gs) {
	bctx.reset(gs);

	/*
	bctx.resel();
	int existingTasks = var("tsk_num", 0);
	if (existingTasks < gs->tasks.num) {
		for (int i = existingTasks; i < gs->tasks.num; i++) {
			taskInstance &task = gs->tasks[i];
			(*task.defn->destroy)(task.data);
		}
		gs->tasks.num = existingTasks;
	}
	// TODO make this common? I'm going to need
	//      some variant of this consistently if
	//      I'm editing in `constelInst`s.
	int existingCis = var("ci_num", 0);
	if (existingCis < gs->constels.num) {
		for (int i = existingCis; i < gs->constels.num; i++) {
			// This breaks things if anybody is
			// referencing that constelInst...
			deleteConstelInst(gs->constels[i]);
		}
		gs->constels.num = existingCis;
	}
	*/


	spawns.init();
	bctx.push();

	// Trolley def'n, may move this to a
	// dedicated file at some point.
	constel *trolley = mkConstel();
	gp("trolley");
	bctx.addPt(trolley, var("shape", 1), var("tex", 8), var("scale", 1700));
	bctx.peek();
	trolleyHalf(trolley);
	bctx.rot((int32_t const[]){32768,0,0});
	trolleyHalf(trolley);
	bctx.peek();
	// This is basically finalizing the `constel`,
	// though you could also specify the radius
	// manually if you knew it. This will only actually
	// matter once we have an "implicit" state for
	// constels to go into.
	trolley->estimateRadius();

	constel *bigPlate = mkConstel();
	gp("bigPlate");
	bctx.addPt(bigPlate, 1, var("tex", 8), var("scale", 5000));
	bigPlate->estimateRadius();

	gp("land");
	bctx.pos(pvar("pos", (offset const){0, 0, -10000}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 1), var("tex", 7), var("scale", 80000));
	bctx.peek();
	gp("50");
	bctx.pos(pvar("pos", (offset const){0, 0, -17112}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	bctx.add(var("shape", 0), var("tex", 4), var("scale", 3000));
	bctx.peek();

	taskKillPlane_create(gs, -30000);

	gp("rte_trolley");
	int32_t trolleyStagger = var("stagger", 290);
	mountain(gs, trolley, bigPlate, 0);
	bctx.rot((int32_t const[]){32768,0,0});
	mountain(gs, trolley, bigPlate, trolleyStagger);
	bctx.peek();

	// To scroll time for all `rails` items as one
	gp("rails");
	int32_t time = var("time", 0);
	for (taskInstance *t = gs->tasks.next; t != &gs->tasks; t = t->next) {
		taskInstance &ti = *t;
		if (ti.defn->id != TSK_RAILS) continue;
		tskRailsData *railsData = (tskRailsData*)ti.data;
		railsData->time += time;
		tskRails_timeHelper(railsData);
	}

	tskTdmData *tdmData = taskTdm_create(gs, spawns.num, 7);
	rangeconst(i, spawns.num) {
		memcpy(tdmData->spawns[i], spawns[i].pos, sizeof(offset));
	}

	/*#1
	gp();
	bctx.pos(pvar("pos", look(3000)));
	bctx.rot(rvar("rot"));
	bctx.add(var("shape"), var("tex", 4), var("scale", 1000));
	bctx.peek();
	*/
	/*#2
	gp();
	bctx.pos(pvar("pos", look(3000)));
	bctx.rot(rvar("rot"));
	plank(
		var("width", 1000),
		var("height", 1000),
		var("tex", 4),
		var("pad", 80)
	);
	bctx.peek();
	*/
	/*#3
	gp();
	bctx.pos(pvar("pos", (offset const){26703, -12737, 5332}));
	bctx.rot(rvar("rot", (int32_t const[]){-32110, 5690, 0}));
	bctx.add(var("shape", 2), var("tex", 5), var("scale", 7500));
	bctx.peek();
	*/

	spawns.destroy();
	// This file has a reference, but we're done with it now!
	trolley->decr();
	bigPlate->decr();
}
