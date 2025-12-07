#include "../serialize.h"

// Don't forget to include the reified header wherever you plunk this down, please!

// Global state. This prevents multi-threaded use of this file,
// but I think the box allocation logic wasn't thread-safe anyway.
TIME vb_now;

static char isLeaf(box *b) {
	return !!b->data;
}

// Todo This is definitely messy stuff, but it's where I am for now.
//      The only other option I can think of is allocating a box,
//      and then completely abusing `kids` and `intersects` to mean something
//      completely different.
//      What I'm trying to avoid is re-allocating the list memory each time
//      one of the functions that needs a local list is entered,
//      and this achieves that, just at the cost that I have to be careful
//      about which methods might potentially be on the callstack when the
//      method which needs global lists is in use.
static list<box*> *globalOptionsDest, *globalOptionsSrc;
static list<box*> *lookDownSrc, *lookDownDest;
static list<box*> refreshList;
static list<box*> boxClones;

static inline void swap(list<box*> *&a, list<box*> *&b) {
	list<box*> *tmp = a;
	a = b;
	b = tmp;
}

// Things that are just touching should not count as intersecting.
static char intersects(box *o, box *n) {
	TIME t_o = vb_now - o->start;
	TIME t_n = vb_now - n->start;
	// I believe this method is fully symmetrical except for which box we get `end` from.
	// Really we want whichever is smaller, but a `min` here probably isn't worth the effort.
	// Instead, we leave it up to the caller to put whichever one is probably smaller as `n`.
	TIME t2 = n->end - vb_now;
	INT r = o->r + n->r;
	range(d, DIMS) {
		INT vel = o->vel[d] - n->vel[d];
		// Removed a check here about very large relative velocities.
		// I could probably add it back, but I don't want to for now.
		// It would just assume it signified a collision on this axis and continue.

		INT d1 = o->pos[d] + o->vel[d] * t_o - n->pos[d] - n->vel[d] * t_n;
		INT d2 = d1 + vel * t2;

		// Outside before, and outside after, and...
		//   The measurement of "distance between" doesn't cross 0
		//   (in which case it would be disrupted by a shift of INT_MIN)
		if (abs(d1) >= r && abs(d2) >= r && (d2-d1 > 0) == (d2-MIN > d1-MIN)) return 0;
	}
	return 1;
}

// Assumes that `p` currently contains `b` ("currently" = vb_now),
// and checks if it's still contained at the end of `b`.
static char contains(box *p, box *b) {
	// These intentionally both use `b->end` as the ending time.
	TIME t_p = b->end - p->start;
	TIME t_b = b->end - b->start;
	INT tolerance = p->r - b->r;
	range(d, DIMS) {
		INT x = p->pos[d] + p->vel[d] * t_p - b->pos[d] - b->vel[d] * t_b;
		if (abs(x) > tolerance) return 0;
	}
	return 1;
}

static void recordIntersect(box *a, box *b) {
#ifdef DEBUG
	if (a == b) {
		fputs("What the ASSSS\n", stderr);
		exit(1);
	}
#endif
	int aNum = a->intersects.num;
	a->intersects.add({.b=b, .i=b->intersects.num});
	b->intersects.add({.b=a, .i=aNum});
}

// Todo could maybe rewrite this with lookDownDest / lookDownSrc if I wanted?
static void addWhale(box *n, list<box*> *children) {
	list<box*> const &c = *children;

	range(i, c.num) {
		box *b = c[i];
		// We don't record leaf-leaf intersects
		if (isLeaf(b)) continue;
		// Not really sure on the ordering here, validity can vary
		// pretty wildly for leaves. Putting the shorter box second
		// is a slight optimization in that it reports fewer intersects.
		if (intersects(b, n)) {
			recordIntersect(b, n);
			addWhale(n, &b->kids);
		}
	}
}

// This is only called for non-root, non-leaf boxes. They have `end` set.
static void setIntersects_refresh(box *n) {
#ifdef DEBUG
	if (n->intersects.num) { fputs("Intersect assertion 2 failed\n", stderr); exit(1); }
#endif
	n->intersects.add({.b=n, .i=0});

	// TODO Is compiler mad about const here? It should be.
	list<sect> const &candidates = n->parent->intersects;
	range(i, candidates.num) {
		box *candidate = candidates[i].b;

		// We assume we have a shorter lifetime than `candidate`, so order `intersects()` args accordingly.
		if (!intersects(candidate, n) && candidate->parent) continue;
		if (isLeaf(candidate)) {
			recordIntersect(n, candidate);
		} else {
			// Not sure about the meaning of this `const`.
			// If it means what I think it does, I may have to force the cast explicitly,
			// since I want the compiler to understand that it's not changing here.
			list<box*> const &peers = candidate->kids;
			range(j, peers.num) {
				box *test = peers[j];
				if (intersects(n, test)) {
					recordIntersect(n, test);
				}
			}
		}
	}
	n->parent->kids.add(n);
}

static void setIntersects_leaf(box *n) {
#ifdef DEBUG
	if (n->intersects.num) { fputs("setIntersects_leaf, but already has intersects\n", stderr); exit(1); }
#endif
	// Typically leafs don't list other leafs as intersects,
	// so this is kind of a break in that. However, nobody really
	// checks the intersects on a leaf (except during cleanup of that leaf),
	// so it's probably fine.
	n->intersects.add({.b=n, .i=0});

	list<sect> &candidates = n->parent->intersects;
	range(i, candidates.num) {
		box *candidate = candidates[i].b;

		// We assume we have a shorter lifetime than `candidate`, so order `intersects()` args accordingly.
		// We omit the check for the root in this `_leaf` version, as leafs shouldn't be that big.
		if (!intersects(candidate, n)) continue;
		// We could do a leaf check here,
		// but right now we just rely on the fact that leafs have no kids.
		addWhale(n, &candidate->kids);
	}
	n->parent->kids.add(n);
}

#define ALLOC_SIZE 100
static list<box*> boxAllocs, freeBoxes;

// Todo This is fine for now, but it really needs a re-work.
//      Now that we're keeping boxes as part of official state, we probably want them in contiguous memory.
//      That will mean either taking some third-party allocator that we can do this stuff with,
//      or doing it by hand:
//        - when copying state, start with a suitably-sized chunk, resizing it up front if necessary (re-alloc).
//        - newly requested boxes either fill in that chunk, or we can alloc a new (smaller) on-demand chunk
//        - we'll probably want to take a high-water mark before we clean up at the end,
//          so we know what "suitably-sized" means in step 2.
//        - on-demand chunks are probably wholesale freed, while the main chunk can be put in some pool for re-use
box* velbox_alloc() {
	if (freeBoxes.num == 0) {
		box *newAlloc = new box[ALLOC_SIZE];
		boxAllocs.add(newAlloc);
		range(i, ALLOC_SIZE) {
			newAlloc[i].kids.init();
			newAlloc[i].intersects.init();
			freeBoxes.add(&newAlloc[i]);
		}
	}
	box *ret = freeBoxes[--freeBoxes.num];
	ret->start = vb_now; // Technically this is an assumption, but it's probably right!
	ret->intersects.num = 0;
	ret->kids.num = 0;
	ret->inUse = 0;
	ret->data = NULL;
#ifndef NODEBUG
	// Hm, usually I don't like my DEBUG defines to actually change behavior. Is this okay? Maybe...
	//ret->clone.ptr = NULL;
#endif
	// Shouldn't need to init `clone`, we've got a comfortable strong ref (parentage tree)
	//   so we don't need to know when the first time we encounter it is (medium refs are garbage)
	return ret;
}

// The fact that things only valid for an instant still need to cover a range is a great source of off-by-one errors.
// Something that's valid just this tick:
// 012
// xx
// Something that needs to be refreshed every 5th tick:
// 0123456
// xxxxxx  (That's 6 time instants where containment etc is valid)
// What about something that refreshes every 5 ticks, and has to support a 5-tick child?
// We refresh in depth order (before children), so worst case is child refresh on frame 4.
// 0123456789012
//     cccccc
// xxxxxxxxxx
// Okay, fun. So the "valid for 5 frames" guy checks positions at 0 and 5,
// and refreshes when time hits 5. This probably means `end == 5`.
// Each successive layer only adds 4 to `end`, but still refreshes every 5 frames.
// So for a "small" layer, end will be vb_now+[1,5].
// For the next layer, `end` will be vb_now+[5,9] ... vb_now+[9,13]
// So we can't guess the layer if `end==vb_now+9` for instance,
// it could be a very fresh layer_1 or an almost stale layer_2.

// Todo: Might make this a table lookup.
//       There aren't that many layers after all,
//       and it saves a branch that way.
static TIME getValidity(int depth) {
	if (depth > VALID_FLOOR) return 1;
	else return (VALID_WINDOW-1)*(VALID_FLOOR - depth) + VALID_WINDOW;
}

static box* mkContainer(box *parent, box *n) {
	box *ret = velbox_alloc();
	ret->r = parent->r / SCALE;
	ret->parent = parent;
	ret->depth = parent->depth + 1;
	ret->start = n->start;
	ret->end = vb_now + getValidity(ret->depth);
	range(d, DIMS) {
		ret->pos[d] = n->pos[d];
		ret->vel[d] = n->vel[d];
	}
	setIntersects_refresh(ret);

	return ret;
}

static box* getMergeBase(box *guess, INT minParentR, INT p1[DIMS]) {
	// Leafs can have intersects at other levels, which makes them a bad merge base.
	// Plus they can't be parents anyways.
	if (isLeaf(guess)) guess = guess->parent;

	// We also skip up until we're the right height because it simplifies some later logic,
	// and we'd have to climb that high anyway.

	while (guess->r < minParentR) {
		guess = guess->parent;
#ifdef DEBUG
		if (!guess) {
			fputs("Looks like we're adding a box which is far too large.\n", stderr);
		}
#endif
	}
	while (1) {
		// Root case
		if (!guess->parent) return guess;

		INT tolerance = guess->r + guess->r/SCALE;
		// The idea is that if it's intersecting an imaginary child-sized box,
		// it should be intersecting any valid potential parents (which will
		// be encompassing said child-sized box).
		// This is slightly incorrect at the lowest level (where we're
		// housing `n`, not an imaginary child), but this is fine since this
		// method is basically just a heuristic for where to start looking.

		INT t1 = vb_now - guess->start;
		// Allowing an intersect at a variety of times (not just t1) might
		// let us find a winner sooner (which is better), but this is easier
		// to write (and reason about) for now.

		// Todo Rewrite this some way that's more likely to leverage SIMD instructions?
		//      Need to research, there's a possibility I could do that throughout.
		range(d, DIMS) {
			INT x = guess->pos[d] + t1*guess->vel[d] - p1[d];
			if (abs(x) >= tolerance) goto next;
		}
		// All dimensions OK, we have a winner
		return guess;

		next:;
		guess = guess->parent;
	}
}

static void clearIntersects(box *b) {
	list<sect> &intersects = b->intersects;
#ifdef DEBUG
	if (!intersects.num) {
		fputs("Intersect assertion 0 failed\n", stderr);
		exit(1);
	}
	if (intersects[0].b != b) { fputs("Intersect assertion 1 failed\n", stderr); exit(1); }
#endif
	// We skip the first one because it's going to be ourselves
	for (int i = 1; i < intersects.num; i++) {
		sect &s = intersects[i];
		box *o = s.b;
		int ix = s.i;

#ifdef DEBUG
		if (ix >= o->intersects.num) {
			fputs("Blam!\n", stderr);
		}
		if (o->intersects[ix].b != b) {
			fputs("well shit\n", stderr);
		}
#endif
		// Remove the corresponding intersect (o->intersects[ix]).
		// This reads a little funny at first; remember the `if` will usually be true.
		o->intersects.num--;
		if (o->intersects.num > ix) {
			o->intersects[ix] = o->intersects[o->intersects.num];
			sect &moved = o->intersects[ix];
#ifdef DEBUG
			if (moved.b->intersects[moved.i].b != o) {
				fputs("well dang\n", stderr);
			}
#endif
			moved.b->intersects[moved.i].i = ix;
		}
	}
	intersects.num = 0;
}

static void remove(box *b) {
	clearIntersects(b);
	freeBoxes.add(b);
}

// This isn't super efficient due to the linear search of `kids`
void velbox_remove(box *o) {
	o->parent->kids.rm(o);
	remove(o);
}

static box* mkParent(box *level, box *n, INT minParentR) {
	// `n->end` has already been set for some time...
	INT minGrandparentR = SCALE*minParentR;

	while (level->r >= minGrandparentR) {
		level = mkContainer(level, n);
	}

	return level;
}

static box* tryLookDown(box *mergeBase, box *n, INT minParentR, INT p1[DIMS]) {
	const list<box*> *optionsSrc;
	INT optionsR;
	int optionsDepth;
	if (mergeBase->parent) {
		lookDownSrc->num = 0;
		list<sect> &sects = mergeBase->intersects;
		for (int i = sects.num - 1; i >= 0; i--) {
			lookDownSrc->add(sects[i].b);
		}
		optionsSrc = lookDownSrc;
		optionsR = mergeBase->r;
		optionsDepth = mergeBase->depth;
	} else if (mergeBase->kids.num) {
		// I think we have a separate case for the root box so that we don't incorrectly
		// rule it out for parentage (since its radius is kind of a lie)
		optionsSrc = &mergeBase->kids;
		// We are once again assuming no titanically large whales
		optionsR = (*optionsSrc)[0]->r;
		optionsDepth = (*optionsSrc)[0]->depth;
	} else {
		// This will (should) only happen for the first non-root box added
		return NULL;
	}

	box* result = NULL;
	char finalLayer = 0;

	do {
		lookDownDest->num = 0;

		INT nextR = optionsR / SCALE;
		TIME duration;
		if (nextR < minParentR) {
			// This is the final depth we'll be checking.
			// This time, we're checking to see if we can contain `n`,
			// not a hypothetical ancestor box of `n`.
			// This is (partly) because `n` can be larger (both `r` and `end`)
			// than an appropriately-sized ancestor box at this depth.
			nextR = n->r;
			duration = n->end - vb_now;
			finalLayer = 1;
		} else {
			// We're assuming that `n` is freshly refreshed,
			// which should be accurate. There's no reason to
			// re-parent something before it's due for a refresh.
			duration = getValidity(optionsDepth+1);
		}
		INT tolerance = optionsR - nextR;
		range(i, optionsSrc->num) {
			box *test = (*optionsSrc)[i];
			if (isLeaf(test)) continue;

			// Previously we'd consider any boxes that could potentially contain the subject
			// in some existing child box, even if they couldn't contain a constructed (centered)
			// ancestor of the subject.
			// We no longer permit this. This simplifies the logic some, and should mean better parents
			// are found (from the more restrictive search reqts), but possibly at the cost of a denser
			// population of boxes in a given area.

			TIME t1 = vb_now - test->start;
			
			range(d, DIMS) {
				INT d1 = test->pos[d] + t1*test->vel[d] - p1[d];
				INT d2 = abs(d1 + duration*(test->vel[d] - n->vel[d]));
				d1 = abs(d1);
				if (d1 > tolerance || d2 > tolerance) goto fail;
			}

			result = test;
			// if (finalLevel) break; // Not sure if this is a worthwhile time save
			lookDownDest->addAll(&test->kids);

			fail:;
		}
		swap(lookDownSrc, lookDownDest);
		optionsSrc = lookDownSrc;
		optionsR = nextR;
		optionsDepth++;
	} while(!finalLayer);

	if (!result) return NULL;
	return mkParent(result, n, minParentR);
}

static box* lookUp(box *b, box *n, INT minParentR, INT p1[DIMS]) {
	for (; b->parent; b = b->parent) {
		INT r = b->r - b->r/SCALE;
		TIME duration = getValidity(b->depth+1);
		list<sect> &intersects = b->intersects;
		range(i, intersects.num) {
			box *test = intersects[i].b;
			if (isLeaf(test)) continue;

			TIME t1 = vb_now - test->start;
			range(d, DIMS) {
				INT d1 = test->pos[d] + t1*test->vel[d] - p1[d];
				INT d2 = abs(d1 + duration*(test->vel[d] - n->vel[d]));
				d1 = abs(d1);
				if (d1 > r || d2 > r) goto fail;
			}

			// Other option here would be to add to a list,
			// which then we would sort somehow
			b = test;
			goto done;

			fail:;
		}
	}
	done:;

	return mkParent(b, n, minParentR);
}

// `n` only needs start/end/pos/vel/r. No other fields are needed by this or any downstream method.
static box* findParent(box *guess, box *n) {
	// It is IMPORTANT to note that `n` must be small enough to have at least one nested level
	// below the apex. The apex is weird enough as it is, this condition reduces the number of
	// edge cases we have to consider.
	// (Note I'm not sure that's true any more, but why would you want boxes that apocalyptically big?)

	// Todo Maybe provide a faster path in the case where it fits fine in `guess->parent`,
	//      i.e. when it hasn't changed much from last time?
	INT minParentR = n->r * FIT;

	INT p1[DIMS];
	TIME t1 = vb_now - n->start;
	range(d, DIMS) {
		p1[d] = n->pos[d] + n->vel[d] * t1;
	}

	box *mergeBase = getMergeBase(guess, minParentR, p1);

	box *lookDownResult = tryLookDown(mergeBase, n, minParentR, p1);
	if (lookDownResult) return lookDownResult;

	// Please, mergeBase->parent was my father
	box *p = mergeBase->parent ? mergeBase->parent : mergeBase;
	return lookUp(p, n, minParentR, p1);
}

static void insert(box *guess, box *n) {
	box *p = findParent(guess, n);
	n->parent = p;
	// Don't add to `p->kids` yet. Part of the contract for "setIntersects" methods
}

static void writeQueryResults(box *b, box *target, list<void*> *results) {
	// We could skip the `intersects` check for a more optimistic query,
	// but I think I'll leave it in for now.
	if (!intersects(b, target)) return;

	if (isLeaf(b)) {
		results->add(b->data);
	} else {
		list<box*> const &kids = b->kids;
		range(i, kids.num) {
			writeQueryResults(kids[i], target, results);
		}
	}
}

// Returns the box that was actually used (suitable for the next `guess`).
// Results are added to the end of `results`, though the user is expected
// to do their own verification if each is eligible (i.e. may contain
// false positives).
box* velbox_query(box *guess, INT pos[DIMS], INT vel[DIMS], INT r, list<void*> *results) {
	box thing = {
		.r = r,
		.start = vb_now,
		.end = vb_now+1,
	};
	range(i, DIMS) {
		thing.pos[i] = pos[i];
		thing.vel[i] = vel[i];
	}
	// Verified that `findParent` (and downstream methods) do not need more
	// than the fields provided above.
	box *p = findParent(guess, &thing);
	p->inUse = 1;
	rangeconst(i, p->intersects.num) {
		writeQueryResults(p->intersects[i].b, &thing, results);
	}
	return p;
}

// Among other things, `n` needs `start` and `end`.
// If asking this of the client is a big hassle, we could do math involving `vb_now` I guess.
// (assuming `vb_now` is set - since that's only set by some ops that are given the root.)
void velbox_insert(box *guess, box *n) {
	insert(guess, n);
	// Don't actually need to set `inUse` in this case. Even if the leaf is only valid for this tick,
	// it will be checked (and removed) after its parent, so the parent will still survive by virtue
	// of having a child.
#ifndef NODEBUG
	if (n->intersects.num) {
		fputs("Intersect assertion 3 failed\n", stderr);
		exit(1);
	}
#endif
	// Todo Not sure we need `depth` on leafs
	n->depth = n->parent->depth+1;
	setIntersects_leaf(n);
}

// Pre: `b->end` should be populated. Should not be in parent's list of kids.
// Post: `b->parent` possibly updated, still not in list of kids.
static void reposition(box *b) {
#ifdef DEBUG
	if (!b->parent) puts("`reposition` precond 1 failed");
	if (b->parent->kids.has(b)) puts("`reposition` precond 2 failed");
#endif
	clearIntersects(b);
	box *p = b->parent;
	// This `contains` is basically a faster common path than the whole parent-finding business in `insert`.
	if (contains(p, b)) return;
	insert(p, b); // the first arg here is just a starting point, we're not adding it right back to `p` lol
}

// For leafs only
void velbox_update(box *b) {
	b->parent->kids.rm(b);
	clearIntersects(b);
	// Presumably its old parent is a good guess for the new parent.
	insert(b->parent, b);
	setIntersects_leaf(b);
}

void velbox_refresh(box *root) {
	// Interestingly, no other place that reads `end` can possibly be looking at the root.
	// The closest we come is `intersects()` checks with a parent's intersect, but we assume
	// we're valid for less time than the parent, so it's never the root we pull `end` from.
	root->end++;
	// So we just use it as a way to track vb_now lol
	vb_now = root->end;

	// The root never needs revalidation or intersect checking,
	// which is convenient since we assume everybody we're refreshing
	// is somebody's kid.
	globalOptionsSrc->num = 0;
	globalOptionsSrc->add(root);

	// `depth` = depth of kids, which is why it starts at 1.
	for (int depth = 1; globalOptionsSrc->num; depth++) {
		// If a kid's validity hits `cutoff`, it's no longer good enough if
		// one of *its* kids wants to refresh.
		TIME cutoff = vb_now + getValidity(depth+1) - 1;
		TIME newEnd = vb_now + getValidity(depth);
		globalOptionsDest->num = 0;
		// `refreshList` facilitates bulk-refreshing each level
		refreshList.num = 0;
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			list<box*> &kids = b->kids;
			globalOptionsDest->addAll(&kids);
			range(j, kids.num) { // not rangeconst, `kids.num` might change
				box *k = kids[j];
				if (isLeaf(k)) continue;
				if (k->end == cutoff) {
					refreshList.add(k);
					kids.rmAt(j);
					j--;
#ifndef NODEBUG
				} else if (k->end < cutoff) {
					puts("box's `end` got too low somehow");
					printf("vb_now %d; cutoff %d; depth %d; validity d+1 %d; end %d\n", vb_now, cutoff, depth, getValidity(depth+1), k->end);
					exit(1);
#endif
				}
			}
		}
		range(j, refreshList.num) {
			box *k = refreshList[j];
			k->end = newEnd;
			reposition(k);
			setIntersects_refresh(k);
		}

		swap(globalOptionsSrc, globalOptionsDest);
	}
}

void velbox_completeTick(box *root) {
	globalOptionsSrc->num = 0;
	globalOptionsSrc->add(root);
	while(globalOptionsSrc->num) {
		globalOptionsDest->num = 0;
		range(i, globalOptionsSrc->num) {
			box *b = (*globalOptionsSrc)[i];
			list<box*> &kids = b->kids;
			globalOptionsDest->addAll(&kids);
			range(j, kids.num) {
				box *k = kids[j];
				char del;
				if (isLeaf(k)) {
					del = (k->end == vb_now+1);
				} else {
					del = !(k->kids.num || k->inUse);
					k->inUse = 0;
				}
				if (del) {
					kids.rmAt(j);
					j--;
					remove(k);
				}
			}
		}
		swap(globalOptionsSrc, globalOptionsDest);
	}
}

box* velbox_getRoot() {
	box *ret = velbox_alloc();
	// I'm not sure if this matters lol
	ret->start = 0;
	// This is how we track `vb_now` across ticks, so it does actually matter
	ret->end = 0;
	// Not sure yet if this is imp't
	vb_now = 0;

	ret->parent = NULL;
	ret->r = MAX/SCALE + 1;
	ret->depth = 0;

	// `kids` can just stay empty for now
	ret->intersects.add({.b=ret, .i=0});
	range(d, DIMS) {
		// This *shouldn't* matter, but uninitialized data is never a *good* thing.
		ret->pos[d] = ret->vel[d] = 0;
	}
	return ret;
}

static void blindFree(box *b) {
	rangeconst(i, b->kids.num) {
		blindFree(b->kids[i]);
	}
	freeBoxes.add(b);
}

void velbox_freeRoot(box *r) {
	// Surely this won't come back to bite me
	blindFree(r);
}

static box* dup(box *b) {
	box *ret = velbox_alloc();
	ret->r = b->r;
	range(i, DIMS) {
		ret->pos[i] = b->pos[i];
		ret->vel[i] = b->vel[i];
	}
	ret->start = b->start;
	ret->end   = b->end;
	ret->inUse = b->inUse;
	ret->depth = b->depth;

	// Haha this line won't compile yet, we'll need more typing
	// Either way, it assumes any data pointers have already
	// been dup'd.
	// Todo maybe a silly optimization, but we could have some other magic "nothing" data ptr
	//      which points to itself in clone.ptr, which saves us an unpredictable branch here...
	if (b->data) ret->data = b->data->clone.ptr;

	b->clone.ptr = ret;

	ret->kids.setMaxUp(b->kids.num+2); // Todo there's got to be a better way
	ret->kids.num = b->kids.num;
	rangeconst(i, b->kids.num) {
		ret->kids[i] = dup(b->kids[i]);
		ret->kids[i]->parent = ret;
	}

	ret->intersects.setMaxUp(b->intersects.num+2);
	ret->intersects.addAll(&b->intersects); // We'll clean this up later.

	return ret;
}

static void dupIntersectsCleanup(box *b) {
	range(i, b->intersects.num) {
		b->intersects[i].b = (box*)b->intersects[i].b->clone.ptr;
	}
	int numKids = b->kids.num;
	range(i, numKids) {
		dupIntersectsCleanup(b->kids[i]);
	}
}

box* velbox_dup(box *root) {
	box *ret = dup(root);
	ret->parent = NULL;
	dupIntersectsCleanup(ret);
	return ret;
}

static void transBox(box *b) {
	// Todo Reset all `start`s to 0 during serialization,
	//      as a guard against time overflow.
	//      Might get a little weird with the root, IDK.
	trans64(&b->r);
	range(i, DIMS) trans64(&b->pos[i]);
	range(i, DIMS) trans64(&b->vel[i]);
	trans32(&b->start);
	trans32(&b->end);
	trans8(&b->inUse);
	trans8(&b->depth);

	// We don't transcribe box data, we rely on whoever we're pointing to to manage that.
	// After all, they probably have a ref to us anyway.

	transStrongRef(b, &boxClones);

	transItemCount(&b->kids);
	rangeconst(i, b->kids.num) {
		if (seriz_reading) b->kids[i] = velbox_alloc();
		transBox(b->kids[i]);
		if (seriz_reading) b->kids[i]->parent = b;
	}
}

static void transBoxIntersects(box *b) {
	transItemCount(&b->intersects);
	rangeconst(i, b->intersects.num) {
		trans32(&b->intersects[i].i);
		transWeakRef(&b->intersects[i].b, &boxClones);
	}
	rangeconst(i, b->kids.num) {
		transBoxIntersects(b->kids[i]);
	}
}

void velbox_trans(box **root) {
	transRefList(&boxClones);

	// Todo: Write a total number of boxes upfront,
	//       so we can alloc more intelligently

	if (seriz_reading) *root = velbox_alloc();
	transBox(*root);
	if (seriz_reading) (*root)->parent = NULL;

	transBoxIntersects(*root);
}

void velbox_init() {
	boxAllocs.init();
	freeBoxes.init();

	globalOptionsSrc = new list<box*>();
	globalOptionsSrc->init();
	globalOptionsDest = new list<box*>();
	globalOptionsDest->init();
	lookDownSrc = new list<box*>();
	lookDownSrc->init();
	lookDownDest = new list<box*>();
	lookDownDest->init();
	refreshList.init();
	boxClones.init();
}

void velbox_destroy() {
	range(i, boxAllocs.num) {
		box *chunk = boxAllocs[i];
		range(j, ALLOC_SIZE) {
			chunk[j].kids.destroy();
			chunk[j].intersects.destroy();
		}
		delete[] chunk;
	}
	boxAllocs.destroy();
	freeBoxes.destroy();
	globalOptionsSrc->destroy();
	delete globalOptionsSrc;
	globalOptionsDest->destroy();
	delete globalOptionsDest;
	lookDownSrc->destroy();
	delete lookDownSrc;
	lookDownDest->destroy();
	delete lookDownDest;
	refreshList.destroy();
	boxClones.destroy();
}
