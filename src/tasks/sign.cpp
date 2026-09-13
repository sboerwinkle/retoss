#include <math.h>
#include <string.h>

#include "../matrix.h"
#include "../util.h"

#include "../bctx.h"
#include "../main.h"
#include "../gamestate.h"
#include "../serialize.h"
#include "../task.h"

#include "sign.h"

struct taskSignShared {
	int refs;
	char *msg;
};

struct signSolid : interactable {
	taskSignShared *shared;
};

static void action(gamestate *gs, interactable *s, mover *src) {
	if (gs != rootState) return;
	if (src != &gs->players[myPlayer].m) return;
	puts(((signSolid*)s)->shared->msg);
}

static interactInfo info = { .action = &action };

static void incr(taskSignShared *shared) {
	shared->refs++;
}

static void decr(taskSignShared *shared) {
	shared->refs--;
	if (shared->refs) return;
	free(shared->msg);
	delete shared;
}

static char step(gamestate *gs, void *_data) {
	return 0;
}

static char trans(gamestate *gs, void **ptr) {
	signSolid *s;
	if (seriz_reading) {
		*ptr = s = new signSolid;
		s->info = &info;
		s->shared = new taskSignShared;
		s->shared->refs = 1;
	} else {
		s = (signSolid*)*ptr;
	}

	transAllocedStr(&s->shared->msg);
	transSolid(s, FLAG_INTERACT);

	return 0;
}

static void copy(void **_to, void *_from) {
	signSolid *from = (signSolid*)_from;
	signSolid *to = new signSolid;
	*_to = to;

	cpSolid(to, from);
	to->shared = from->shared;
	to->info = &info;

	incr(to->shared);
}

static void destroy(void *_data) {
	signSolid *s = (signSolid*)_data;
	decr(s->shared);
	delete s;
}

void taskSign_create(buildCtx *b, char const *msg, int64_t r, int32_t shape, int32_t tex) {
	signSolid *s = new signSolid;
	addTaskStart(b->gs, TSK_SIGN, s);

	s->info = &info;

	s->shared = new taskSignShared;
	s->shared->refs = 1;
	s->shared->msg = strdup(msg);

	// TODO This stuff should probably be moved to bctx state,
	// it's going to be common for everything that's a solid
	// and passing it around here is a pain.
	s->m.type = shape%NUM_SHAPES + FLAG_INTERACT;
	s->r = r;
	s->tex = tex;
	solidValidate(s);

	b->populate(s);
	b->prevBox = staticPosition(b->gs, s, b->prevBox);
}

void taskSign_define(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
