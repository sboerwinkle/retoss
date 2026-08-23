#include <stdio.h>

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"
#include "../tasks/dynamics.h"

extern "C" void lvlUpd(gamestate *gs) {
	bctx.resel();

	int existingTasks = var("tsk_num", 0);
	if (existingTasks < gs->tasks.num) {
		for (int i = existingTasks; i < gs->tasks.num; i++) {
			taskInstance &task = gs->tasks[i];
			(*task.defn->destroy)(task.data);
		}
		gs->tasks.num = existingTasks;
	}

	bctx.push();

	gp("0");
	bctx.pos(pvar("pos", (offset const){-24200, 5500, 1200}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 0, 0}));
	int64_t const *v = pvar("vel", (offset const){7400, 0, 0});
	iquat rv;
	toQuat(rv, rvar("rvel", (int32_t const[]){0, 0, 0}));
	tskDynamics_create(gs, &bctx, v, rv);

	//#add_here

	/*#1
	gp();
	bctx.pos(pvar("pos", look(3000)));
	bctx.rot(rvar("rot"));
	bctx.add(var("shape"), var("tex", 4), var("scale", 1000));
	bctx.peek();
	 */
}
