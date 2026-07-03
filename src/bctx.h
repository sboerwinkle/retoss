#include "matrix.h"

#include "gamestate.h"

// buildCtx can add points to these,
// so the type at least needs to be declared here.
struct tskRailsInstructions;

typedef void (*solidConsumer)(solid*);

struct buildTransform {
	offset pos;
	iquat rot;
	int32_t scale;
	// Hasn't been rotated or scaled yet.
	// This lets us compose translations losslessly.
	offset posPending;
	char hasPosPending;
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
	void pos(int64_t scale, offset const v);
	void pos(offset const v);
	void finalizeTranslate();
	void rotQuat(iquat const r);
	void rot(int32_t const rotParams[3]);
	void scale(int32_t scale);

	void resel();
	void add(int32_t shape, int32_t tex, int64_t size);
	constelInst* add(constel *c, int32_t duration);
	void addPt(constel *c, int32_t shape, int32_t tex, int64_t size);
	void addPt(tskRailsInstructions *instr, int32_t time);
};

extern buildCtx bctx;

// Maybe this fits better in matrix.h?
// We don't really use this outside of buildCtx though.
extern void toQuat(iquat out, int32_t const rotParams[3]);

extern void bctx_init();
extern void bctx_destroy();
