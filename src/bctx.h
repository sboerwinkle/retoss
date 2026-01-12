#include "matrix.h"

#include "gamestate.h"

typedef void (*solidConsumer)(solid*);

struct buildTransform {
	offset pos;
	iquat rot;
	int32_t scale;
};

struct buildCtx {
	list<buildTransform> transformStack;
	buildTransform transf;
	gamestate *gs;
	box *prevBox;
	char selecting;
	solidConsumer solidCallback;

	void reset(gamestate *gs);
	void complete();

	void push();
	void pop();
	void peek();

	void pos(int64_t x, int64_t y, int64_t z);
	void pos(offset const v);
	void rotQuat(iquat const r);
	void rot(int32_t const rotParams[3]);
	void scale(int32_t scale);

	void resel();
	void add(int32_t shape, int32_t tex, int64_t size);
};

extern buildCtx bctx;

extern void bctx_init();
extern void bctx_destroy();
