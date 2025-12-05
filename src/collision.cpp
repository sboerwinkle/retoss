#include "gamestate.h"
#include "matrix.h"

static unitvec const cubeFaceNorms[6] = {
	{ FIXP,     0,     0},
	{-FIXP,     0,     0},
	{    0,  FIXP,     0},
	{    0, -FIXP,     0},
	{    0,     0,  FIXP},
	{    0,     0, -FIXP}
};

// Assumes `o` isn't, like, super-duper big
static int64_t dot(offset const o, unitvec const v) {
	return (o[0]*v[0] + o[1]*v[1] + o[2]*v[2])/FIXP;
}

// This is suuper lazy, and starts to behave real weird if things are spinning too fast.
// However, it should be good enough, I hope!
// I'll have to see what I can do about keeping some of this pre-calculated if possible...
void collide_check(player *p, offset dest, int32_t radius, solid *s) {
	offset v1raw;
	range(i, 3) v1raw[i] = p->pos[i] - s->oldPos[i];
	offset v2raw;
	range(i, 3) v2raw[i] = dest[i] - s->pos[i];

	// I think converting these to `imat`s and then using them once each
	// is probably less efficient than just using the direct quaternion-application
	// function, but I'm assuming eventually we'll be re-using these
	// (specifically, once we have objects with multiple 'pushee' centers,
	//  or if we cache rotations of big things or something, idk)
	imat rot1, rot2;
	imatFromIquatInv(rot1, s->oldRot);
	imatFromIquatInv(rot2, s->rot);

	offset v1, v2;
	imat_applySm(v1, rot1, v1raw);
	imat_applySm(v2, rot2, v2raw);

	int winner = 0;
	int64_t best = INT64_MIN;
	int64_t winnerDepth = 0;
	// Let's assume that our vectors can be safely multiplied by FIXP.
	// This makes our math a lot easier, but it does restrict how big items can be.
	// If 1000 units is a meter (which means a bittoss guy is one Minecraft block big),
	// that means no rotating vectors bigger than 100 million kilometers (500 jupiters end-to-end).
	// Or rather, if you rotate vectors that big, you'd need to know in advance that they're big
	// (like e.g. orbital dynamics), and invoke a dedicated routine that divides by FIXP *first*.
	// Who knows, at that scale you may even need more accurate rotations.
	range(i, 6) {
		int32_t const *norm = cubeFaceNorms[i];
		// Cubes have the same offset for all their faces,
		// so `limit` is constant across faces in this case.
		int32_t limit = s->r + radius;

		int64_t d1 = dot(v1, norm) - limit;
		int64_t d2 = dot(v2, norm) - limit;

		if (d1 >= 0 && d2 >= 0) {
			// No hit
			return;
		}
		if (d1 > best) {
			// Winner is the one that starts out furthest away
			best = d1;
			winner = i;
			winnerDepth = d2;
		}
	}

	// Nothing needs to be done - either no hits,
	// or we already left on the axis that we wanted to resolve on.
	if (winnerDepth >= 0) return;

	// Hit, collide with face `winner`.
	unitvec forceDir;
	// Lots of ways I could do this; e.g. a dedicated method to apply our existing rotation matrix but in reverse.
	iquat_apply(forceDir, s->rot, cubeFaceNorms[winner]);
	range(i, 3) dest[i] -= winnerDepth*forceDir[i]/FIXP;
}

// Next up is probably figuring out how to collide something that has a number of spheres
// with a whole nasty nested network of solids.
// Maybe a related problem, do we keep this network around? Or do we add things to it as we go???
