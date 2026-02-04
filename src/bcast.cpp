#include "gamestate.h"
#include "collision.h"
#include "box.h"

struct bcast_cand {
	fraction time;
	char isBox;
	void *item;

	bool operator<(bcast_cand const &other) const {
		return time.lt(other.time);
	}
};

// TODO Is this too big to be shuffling around like this?
//      Should I be going to more effort to just move around
//      pointers instead of these big-honkin' structs?
//      I probably don't mill through too many, so I could have
//      an add-only list with the actual data, and rework the
//      heap logic to assume it's always a pointer type.
static list<bcast_cand> candidates;
// Todo: Decide if `dir` and `origin` should be static state instead of passing them around everywhere

static void addBoxCand(unitvec const dir, offset const origin, box *b) {
	int32_t flip[3];
	unitvec look;
	range(i, 3) {
		flip[i] = (dir[i]>>31) | 1;
		look[i] = dir[i]*flip[i];
	}
	bcast_cand c;
	fraction &lower = c.time;
	lower = {.numer=0, .denom=1};
	fraction upper = {.numer=INT64_MAX/FIXP, .denom=1};
	int32_t time = vb_now - b->start;
	range(i, 3) {
		int64_t x = flip[i] * (b->pos[i] + b->vel[i]*time - origin[i]);
		fraction f1 = {.numer = x - b->r, .denom = look[i]};
		fraction f2 = {.numer = x + b->r, .denom = look[i]};
		// No guarantee `denom` is non-zero, be mindful of `lt` behavior in such cases.
		if (lower.lt(f1)) lower = f1;
		if (f2.lt(upper)) upper = f2;
		if (!lower.lt(upper)) return;
	}
	c.isBox = 1;
	c.item = b;
	candidates.heap_push(c);
}

static void addMoverCand(unitvec const dir, offset const origin, mover *m) {
	bcast_cand c;
	c.time = {.numer = INT64_MAX/FIXP, .denom = 1};
	// TODO Is this based off first position?
	//      For now it's just shooting and selecting;
	//      selecting doesn't really care, and shooting
	//      is (for now) at the start of the frame.
	//      Ugh nope it's based off "new" position.
	//      Using old position would require adding `oldRot` to `mover` state.
	//      Also if we do it before blocks move I'm not sure if things that
	//      just expired will be in the velbox tree or not?? Probably so,
	//      but the tree has ticked, right? What's really going on???
	if (raycast(&c.time, m, origin, dir)) {
		c.isBox = 0;
		c.item = m;
		candidates.heap_push(c);
	}
}

// Caller is responsible for choosing a `b` that's small enough that we can
// safely do `fraction` math on the distances involved.
void bcast_start(box *b, unitvec const dir, offset const origin) {
	candidates.num = 0;
	rangeconst(i, b->intersects.num) {
		addBoxCand(dir, origin, b->intersects[i].b);
	}
}

mover * bcast(fraction *out_time, unitvec const dir, offset const origin) {
	while (candidates.num) {
		bcast_cand c = candidates.heap_pop();
		if (c.isBox) {
			box *b = (box*)c.item;
			if (b->data) {
				addMoverCand(dir, origin, (mover*)b->data);
			} else {
				// Todo: Can this bulk-add be optimized at all? Probably not...
				rangeconst(i, b->kids.num) {
					addBoxCand(dir, origin, b->kids[i]);
				}
			}
		} else {
			// TODO: Save the heap_siftDown for the
			//       beginning of the call, so the
			//       last `bcast` doesn't have to pay
			//       that price. Just gets a bit weird
			//       since we init the heap strangely.
			*out_time = c.time;
			return (mover*)c.item;
		}
	}
	return NULL;
}

void bcast_init() {
	candidates.init();
}

void bcast_destroy() {
	candidates.destroy();
}
