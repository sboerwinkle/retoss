// Constellations.
// A set of solids, fixed relative to each other, that may appear multiple times (like a blueprint).
// Eventually the idea is that each instance won't always be "explicit" in the world,
// and can be "implicit" if nothing's colliding with it.

struct constelPt {
	offset o;
	iquat rot;
	int64_t r;
	int32_t type; // TODO validation for type/tex at least
	int32_t tex;
};

// `constel`s are COW (copy-on-write), to reduce gamestate copy overhead.
struct constel {
	list<constelPt> points;
	int64_t r;
	int refCount;
	int serizIx;

	void incr();
	void decr();
	// Not super smart, will likely be larger than necessary.
	// Still, quick-and-easy if you don't have a custom `r`
	// to populate.
	void estimateRadius();
};

struct constelInst {
	constel *c;
	mover m;
	// box *prox; // Will need this once it's ever implicit.
	int32_t duration;
	list<solid> solids;
	clone_t clone;
};

extern void transConstel(constel **c);
extern void constelSerizFinalize();
