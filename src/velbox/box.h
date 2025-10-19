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
	// Note for non-fish boxes, this will always be the same for all axes
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
	cloneable *data;
};

box* velbox_alloc();
void velbox_insert(box *guess, box *n);
void velbox_remove(box *o);

// Intent is that you call `velbox_step` on all your leaf boxes,
// and then call velbox_refresh to step all the interal boxes and update intersects as necessary.
void velbox_step(box *b, INT *p1, INT *p2);
void velbox_refresh(box *root);
// If you need to make changes to a subset of boxes, you call velbox_update on all of them,
// and then velbox_single_refresh on all of them.
void velbox_update(box *b);
void velbox_single_refresh(box *b);

box* velbox_getRoot();
void velbox_freeRoot(box *r);
void velbox_init();
void velbox_destroy();
