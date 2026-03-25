#include "../bctx.h"
#include "../dl.h"

void house1(int64_t wall, int64_t door) {
	int64_t wallEdge = wall/8;
	int64_t wallScale = wall/2;
	int64_t full = (wall+wallEdge)*2+door;
	int32_t const r_x[3] = {     0, 23170, 0};
	int32_t const r_y[3] = {-23170, 23170, 0};
	offset pos = {0, 0, wall/2};

	pushVarIgnore();
	bctx.push();

	pos[0] = -wall/2;
	pos[1] = wallEdge/2;
	bctx.pos(pos);
	bctx.rot(r_x);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -wall-wallEdge/2;
	pos[1] = wall/2;
	bctx.pos(pos);
	bctx.rot(r_y);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -full+wallEdge+wall/2;
	pos[1] = wallEdge/2;
	bctx.pos(pos);
	bctx.rot(r_x);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -full+wallEdge/2;
	pos[1] = wall/2;
	bctx.pos(pos);
	bctx.rot(r_y);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -full+wallEdge/2;
	pos[1] = wall*3/2;
	bctx.pos(pos);
	bctx.rot(r_y);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -full+wall/2;
	pos[1] = wall*2+wallEdge/2;
	bctx.pos(pos);
	bctx.rot(r_x);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -full+wall*3/2;
	pos[1] = wall*2+wallEdge/2;
	bctx.pos(pos);
	bctx.rot(r_x);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -wallEdge/2;
	pos[1] = full-wallEdge-wall*3/2;
	bctx.pos(pos);
	bctx.rot(r_y);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -wallEdge/2;
	pos[1] = full-wallEdge-wall/2;
	bctx.pos(pos);
	bctx.rot(r_y);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -wall/2;
	pos[1] = full-wallEdge/2;
	bctx.pos(pos);
	bctx.rot(r_x);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -wall*3/2;
	pos[1] = full-wallEdge/2;
	bctx.pos(pos);
	bctx.rot(r_x);
	bctx.add(1, 5, wallScale);

	bctx.peek();
	pos[0] = -full/2;
	pos[1] = full/2;
	pos[2] = wall+full/16;
	bctx.pos(pos);
	bctx.add(1, 8, full/2);

	bctx.pop();
	popVarIgnore();
}
