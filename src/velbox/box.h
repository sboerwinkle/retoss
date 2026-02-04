// Ugh, this is all too tangled up with the current project.
// I want boxes to be "cloneable", but that #include can't
// be in this file (since the relative path doesn't make sense,
// given that this file is meant to be included elsewhere...)
//#include "../cloneable.h"

// If you want to define these using GCC args, that's fine -
// an alternative is to "wrap" this file (and the .cpp file)
// with files that just #define all this and then #include
// the "original" file. This should make it easier to target
// the scopes on the definitions.

// This file (and the .cpp file) also need `list` and `range` defined.
// Because I have no idea how "packages" are done in professional C,
// and because I'm just doing as a personal project, it's up to
// whoever includes this file to provide those definitions.

#ifndef INT
#define INT int64_t
#endif

#ifndef MAX
#define MAX INT64_MAX
#endif

#ifndef MIN
#define MIN INT64_MIN
#endif

#ifndef TIME
#define TIME int32_t
#endif

#ifndef DIMS
#define DIMS 3
#endif

#ifndef SCALE
#define SCALE 4
#endif

#ifndef FIT
#define FIT 3
#endif

#ifndef VALID_WINDOW
#define VALID_WINDOW 5
#endif

// This should be somewhere around 4000-ish, may revisit later.
// That's 12 bits, leaves 52 bits (ish), which is 13 layers
#ifndef VALID_FLOOR
#define VALID_FLOOR 13
#endif

// These defaults puts us at a orbit time of around 5 seconds before
// math potentially starts breaking down. That's not so bad.

extern TIME vb_now;

struct box;

struct sect {
	box *b;
	int i;
};

struct box : cloneable {
	// Should not exceed INT_MAX/2 (so it can be safely added to itself)
	INT r;

	INT pos[DIMS];
	INT vel[DIMS];
	TIME start;
	TIME end;

	box *parent;
	list<box*> kids;
	list<sect> intersects;

	char inUse;
	char depth;
	// Points to some known struct that has any req'd info on type or whatever.
	// Could have "type" here directly, but the advantage is if it's just one field,
	// we can just have `list<foo*>` as the way potential intersects are returned
	// from a query!
	// TODO How is this type not #define'd??
	void *data;
};

extern list<box*> boxSerizPtrs;

extern box* velbox_alloc();

extern void velbox_remove(box *o);
extern box* velbox_findParent(box *guess, INT pos[DIMS], INT vel[DIMS], INT r);
extern box* velbox_query(box *guess, INT pos[DIMS], INT vel[DIMS], INT r, list<void*> *results);
extern void velbox_insert(box *guess, box *n);

// For leafs
extern void velbox_update(box *b);
// This should happen as the first thing. Advances time and updates intersects as necessary.
extern void velbox_refresh(box *root);
// This should happen as the last thing. Cleans up expired boxes mostly,
// so there's less stuff to dup or serizialize.
extern void velbox_completeTick(box *root);

extern box* velbox_getRoot();
extern void velbox_freeRoot(box *r);

extern box *velbox_dup(box *r);
extern void velbox_trans(box *r);

extern void velbox_init();
extern void velbox_destroy();
