#include "../gamestate.h"
#include "../bctx.h"
#include "../dl.h"

void house2(gamestate *gs, int64_t wall, int32_t const * r1) {
	pushVarIgnore();
	bctx.push();

	int64_t edge = wall/8;
	int32_t r2[3] = {-r1[0], r1[1], r1[2]};
	// int64_t door = var("door", 1600); // Can't do this, roof must be square

	gp("2");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){0, 3, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-1, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("3");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){0, 5, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-1, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("4");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){0, 7, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-1, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("5");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-1, 8, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 3, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("6");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-3, 8, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 3, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("7");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-5, 8, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 3, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("8");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-7, 8, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 3, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("9");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-8, 7, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-3, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("10");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-8, 5, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-3, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("11");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-8, 3, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-3, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("12");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-4, 5, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("13");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-4, 3, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("14");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-4, 1, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 2, 0}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(1, 5, wall);
	gp("15");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){0, 0, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 1, 0}));
	bctx.rot(r1);
	bctx.pos(-wall, 0, 0);
	bctx.add(1, 5, wall);
	gp("16");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-4, 0, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 1, 0}));
	bctx.rot(r2);
	bctx.pos(wall, 0, 0);
	bctx.add(1, 5, wall);
	gp("17");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-4, 0, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 1, 0}));
	bctx.rot(r1);
	bctx.pos(-wall, 0, 0);
	bctx.add(1, 5, wall);
	gp("18");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-8, 0, 1}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 1, 0}));
	bctx.rot(r2);
	bctx.pos(wall, 0, 0);
	bctx.add(1, 5, wall);
	gp("19");
	bctx.peek();
	bctx.pos(wall, pvar("v_wall", (offset const){-4, 4, 2}));
	bctx.pos(edge, pvar("v_edge", (offset const){-2, 2, 0}));
	bctx.pos(0, 0, (wall*2+edge)/4);
	bctx.add(1, 8, wall*4+edge*2);

	bctx.pop();
	popVarIgnore();
	//#add_here

	/*#1
	gp();
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){0,0,1600}));
	bctx.rot(rvar("rot", (int32_t const[]){0, 23170, 0}));
	bctx.add(var("shape", 1), 5, var("scale", 1600));
	 */
	/*#2
	gp();
	bctx.peek();
	bctx.pos(pvar("pos", (offset const){0,0,1600}));
	bctx.rot(rvar("rot", (int32_t const[]){-23170, 23170, 0}));
	bctx.add(var("shape", 1), 5, var("scale", 1600));
	 */
}
