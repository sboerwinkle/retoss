#include "util.h"

#include "gamestate.h"
#include "serialize.h"

#include "constel.h"

static constel dummyConstel;
static list<constel*> serizPtrs;

void constel::incr() {
	this->refCount++;
}

void constel::decr() {
	this->refCount--;
	if (this->refCount) return;

	// Destroy constel
	points.destroy();
	free(this);
}

void constel::estimateRadius() {
	rangeconst(i, points.num) {
		constelPt &pt = points[i];
		int64_t ptRadius;
		if (pt.type < 0 || pt.type >= NUM_SHAPES) {
			printf("ERROR: constel: bad constelPt.type: %d\n", pt.type);
			ptRadius = pt.r;
		} else {
			// TODO: Bounds check that result fits losslessly in `double` (before type casting).
			ptRadius = pt.r * shapeDiagonalMultipliers[pt.type];
		}
		ptRadius += mag(pt.o);
		if (ptRadius > r) r = ptRadius;
	}

	if (r < 0) {
		puts("Kind of weird to create a constel with no points!");
		r = 1'000;
	}
}

constel* mkConstel() {
	constel* ret = (constel*)malloc(sizeof(constel));

	ret->refCount = 1;
	ret->points.init();
	ret->r = -1;
	ret->serizIx = -1;

	return ret;
}

// Last time we had a COW structure we had something like:
// void mkWriteable(constel **c)
// which would duplicate the structure if `refCount>1`
// so we were the only one using our copy (hence, safe to write).
//
// We don't need that for `constel`s yet (or maybe ever),
// since we don't write to them after creation.

void transConstel(constel **_c) {
	constel* &c = *_c;
	if (seriz_reading) {
		int ix = read32();
		if (ix < 0 || ix > serizPtrs.num) {
			c = &dummyConstel;
			if (seriz_error()) {
				printf("Constel index should be no greater than %d, but got %d (0x%X)\n", serizPtrs.num, ix, ix);
			}
		} else if (ix < serizPtrs.num) {
			c = serizPtrs[ix];
		} else {
			// First record of a new constel
			c = mkConstel();
			serizPtrs.add(c);
		}
		c->incr();
	} else {
		if (c->serizIx == -1) {
			c->serizIx = serizPtrs.num;
			serizPtrs.add(c);
			c->incr();
		}
		write32(c->serizIx);
	}
}

static void transConstelPts(constel *c) {
	transItemCount(&c->points);
	rangeconst(i, c->points.num) {
		constelPt &pt = c->points[i];
		range(j, 3) trans64(&pt.o[j]);
		range(j, 4) trans32(&pt.rot[j]);
		trans64(&pt.r);
		trans32(&pt.type);
		trans32(&pt.tex);
	}
}

void constelSerizFinalize() {
	rangeconst(i, serizPtrs.num) {
		constel *c = serizPtrs[i];

		trans64(&c->r);
		transConstelPts(c);

		// Clear out for next time.
		// Could put this in a separate
		// `constelSerizBegin` if I wanted.
		c->serizIx = -1;
		// This should never result in a free,
		// but for now we are counting `serizPtrs` towards `refs`.
		c->decr();
	}
	serizPtrs.num = 0;
}

void constel_init() {
	dummyConstel.points = {.items=NULL, .num=0, .max=0};
	dummyConstel.r = 1'000;
	dummyConstel.refCount = 1;

	serizPtrs.init();
}

void constel_destroy() {
	serizPtrs.destroy();
}
