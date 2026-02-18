#include <math.h>

#include "gamestate.h"
#include "matrix.h"

#include "collision.h"

#include "main.h" // Just for `debug` computation, need to know if we're in the rootState

struct shapeSpec {
	int const numFaces;
	unitvec const *facings;
	int32_t const *distances;
};

shapeSpec shapeSpecs[3] = {
	{
		.numFaces = 6,
		.facings = (unitvec const []){
			{ FIXP,     0,     0},
			{-FIXP,     0,     0},
			{    0,  FIXP,     0},
			{    0, -FIXP,     0},
			{    0,     0,  FIXP},
			{    0,     0, -FIXP},
		},
		.distances = (int32_t const []){
			FIXP,
			FIXP,
			FIXP,
			FIXP,
			FIXP,
			FIXP,
		}
	},
	{
		.numFaces = 6,
		.facings = (unitvec const []){
			{ FIXP,     0,     0},
			{-FIXP,     0,     0},
			{    0,  FIXP,     0},
			{    0, -FIXP,     0},
			{    0,     0,  FIXP},
			{    0,     0, -FIXP}
		},
		.distances = (int32_t const []){
			FIXP,
			FIXP,
			FIXP,
			FIXP,
			FIXP/8,
			FIXP/8,
		}
	},
	{
		.numFaces = 6,
		.facings = (unitvec const []){
			{ FIXP,     0,     0},
			{-FIXP,     0,     0},
			{    0,  FIXP,     0},
			{    0, -FIXP,     0},
			{    0,     0,  FIXP},
			{    0,     0, -FIXP}
		},
		.distances = (int32_t const []){
			FIXP/8,
			FIXP/8,
			FIXP,
			FIXP,
			FIXP/8,
			FIXP/8,
		}
	},
};

int64_t collide_check(player *p, offset dest, int32_t radius, solid *s, unitvec forceDir_out, offset contactVel_out) {
	offset v1raw;
	range(i, 3) v1raw[i] = p->m.pos[i] - s->m.oldPos[i];
	offset v2raw;
	range(i, 3) v2raw[i] = dest[i] - s->m.pos[i];

	// I think converting these to `imat`s and then using them once each
	// is probably less efficient than just using the direct quaternion-application
	// function, but I'm assuming eventually we'll be re-using these
	// (specifically, once we have objects with multiple 'pushee' centers,
	//  or if we cache rotations of big things or something, idk)
	imat rot1, rot2;
	imatFromIquatInv(rot1, s->m.oldRot);
	imatFromIquatInv(rot2, s->m.rot);

	offset v1, v2;
	imat_applySm(v1, rot1, v1raw);
	imat_applySm(v2, rot2, v2raw);

	shapeSpec &sh = shapeSpecs[s->m.type];
	int winner1 = 0;
	int64_t best = INT64_MIN;
	//int64_t winnerDepth = 0;
	// Let's assume that our vectors can be safely multiplied by FIXP.
	// This makes our math a lot easier, but it does restrict how big items can be.
	// If 1000 units is a meter (which means a bittoss guy is one Minecraft block big),
	// that means no rotating vectors bigger than 100 million kilometers (500 jupiters end-to-end).
	// Or rather, if you rotate vectors that big, you'd need to know in advance that they're big
	// (like e.g. orbital dynamics), and invoke a dedicated routine that divides by FIXP *first*.
	// Who knows, at that scale you may even need more accurate rotations.

	// Okay, here's what we're going to do:
	// Look over all faces, find the one with the greatest "distance to face"
	//   If any of them are untouched before and after, we can still exit early.
	// Adjust our magical point (need a new way to track that) along that axis, look again.
	//   We don't have any early-exits here, but if we don't find any positive "dtf"s then we have our vector.
	// If we did find one, do vector math, adjust magical point, and check again
	// If we find one a third time, we do yet more vector math, but we don't check again
	//   (we should be at a corner now)
	// In any case, we have our magical pt now, and we convert it to a vector post haste.
	// We can use that to adjust `v1` as is, but `v2` would technically need a couple rotations.
	//   We're just going to apply it to `v2` naively as well, which might actually be better in some rotational cases lol
	// Okay then we've got to figure out the contact time, which becomes a bit rough
	//   The thing is, we want high-speed impacts with thin walls to register,
	//   but we also want high-speed flybys of corners to not register,
	//   and we kind of have to pick which one we want more.
	//   The secret third option involves rational types again,
	//     but we can't reliably do those multiplications in int64_t
	//     (we're trying to handle up to about 49 bits of position difference here)
	//     and at that point it's too slow to be in the standard collision code
	//   We choose to err on the side of "no hit",
	//   with the rationale that corner flybys will be way more common than
	//   high-speed thin-wall hits.
	// If we have a hit, we adjust along our magic vector to the same "height" as our magic pt.
	//   Interestingly, the contact time doesn't actually matter here, but I think it works.
	//   There could be situations where we're already beyond that "height" - in that case, no hit.

	// Phase 1: First face and early-exit check.
	range(i, sh.numFaces) {
		int32_t const *norm = sh.facings[i];
		int32_t limit = s->r*sh.distances[i]/FIXP;

		int64_t d1 = dot(v1, norm) - limit;
		int64_t d2 = dot(v2, norm) - limit;

		if (d1 >= radius && d2 >= radius) {
			// No hit, this is our "early exit"
			return 0;
		}
		if (d1 > best) {
			// Winner is the one that starts out furthest away
			best = d1;
			winner1 = i;
		}
	}

	// Okay, we didn't exit early, so maybe we have a hit.
	// Time to start working on our magic point, which is
	// the closest point on the surface of the shape at the
	// beginning of this step.
	offset magicPt;
	{
		int32_t const *norm = sh.facings[winner1];
		range(i, 3) magicPt[i] = v1[i] - best*norm[i]/FIXP;
	}

	// Phase 2: Check for a second positive face, which would mean an edge.
	// TODO: Be more scientific about any FIXP errors that might happen.
	//       I feel like between truncating `best` and truncating the `magicPt`
	//       displacement we might have some leftover distance left to cover (that we don't care about!)
	best = 1;
	int winner2;
	range(i, sh.numFaces) {
		int32_t const *norm = sh.facings[i];
		// TODO: we're recalculating this each time, maybe put it somewhere handy
		int32_t limit = s->r*sh.distances[i]/FIXP;

		int64_t d1 = dot(magicPt, norm) - limit;

		if (d1 > best) {
			best = d1;
			winner2 = i;
		}
	}

	//char debug = 0;
	unitvec sampleNorm;
	if (best == 1) {
		// Only one face required adjusting, so we don't need to do any math to get the
		// sample vector; we can just use that face's normal.
		memcpy(sampleNorm, sh.facings[winner1], sizeof(unitvec));
		// We can also skip the 3rd face check
		goto foundSampleNorm;
	}
	// Adjustment of `magicPt` won't be so straightforward this time...
	// Direction will be `winner2 - winner1*dot(w1,w2)`
	{
		int32_t const *norm1 = sh.facings[winner1];
		int32_t const *norm2 = sh.facings[winner2];
		int32_t cosine = dot(norm1, norm2);
		int32_t scale = FIXP-cosine*cosine/FIXP;
		range(i, 3) magicPt[i] -= best*(norm2[i]-norm1[i]*cosine/FIXP)/scale;
	}

	// Phase 3: One more go-round, to find corners. It's rough, but too bad!
	best = 1;
	int winner3;
	range(i, sh.numFaces) {
		int32_t const *norm = sh.facings[i];
		int32_t limit = s->r*sh.distances[i]/FIXP;

		int64_t d1 = dot(magicPt, norm) - limit;

		if (d1 > best) {
			best = d1;
			winner3 = i;
		}
	}

	if (best != 1) {
		/*{
			int64_t tmp = p - rootState->players.items;
			if (tmp >= 0 && tmp < rootState->players.num) debug = 1;
		}*/
		//if (debug) printf("w: %d %d %d\n", winner1, winner2, winner3);
		// Oh boy, the most fun.
		unitvec v3;
		cross(v3, sh.facings[winner1], sh.facings[winner2]);
		int32_t dot_prod = dot(v3, sh.facings[winner3]);
		// This one seems way too simple, but I think it's correct??
		range(i, 3) magicPt[i] -= best*v3[i]/dot_prod;
	}

	// Then we've got to actually to do the scaling to get our test point.
	{
		offset o;
		range(i, 3) o[i] = v1[i] - magicPt[i];
		//if (debug) printf("%ld, %ld, %ld ?\n", magicPt[0], magicPt[1], magicPt[2]);
		//if (debug) printf("%ld, %ld, %ld :(\n", o[0], o[1], o[2]);
		int64_t magEst = labs(o[0]) + labs(o[1]) + labs(o[2]);
		range(i, 3) o[i] = o[i]*FIXP/magEst;
		// Haha actually, I am a little worried about the consistency of large sqrts haha
		int32_t dist = sqrt(o[0]*o[0] + o[1]*o[1] + o[2]*o[2]);
		range(i, 3) sampleNorm[i] = o[i]*FIXP/dist;
		//if (debug) printf("%d, %d, %d :)\n", sampleNorm[0], sampleNorm[1], sampleNorm[2]);
	}
foundSampleNorm:;
	offset o;
	// We want this calculation to round *away* from 0. The wacky second term accomplishes that,
	// but I may make it more readable later. Goal is to help with edge weirdness.
	range(i, 3) o[i] = (sampleNorm[i]*-radius - ((sampleNorm[i]>>31)|1)*(FIXP-1)) / FIXP;
	range(i, 3) v1[i] += o[i];
	range(i, 3) v2[i] += o[i];

	// Alright, we've picked a particular point on the surface of our sphere (sampleNorm)
	// and adjusted v1 and v2 accordingly. Now we have to determine if we have a hit.
	int32_t lower = 0, upper = FIXP;
	range(i, sh.numFaces) {
		int32_t const *norm = sh.facings[i];
		int32_t limit = s->r*sh.distances[i]/FIXP;

		int64_t d1 = dot(v1, norm) - limit;
		int64_t d2 = dot(v2, norm) - limit;

		// Previously `== 0` was in the first (`lower`) branch; now it's in the second branch.
		// Goal is to help with edge weirdness.
		if (d1 > 0) {
			if (d2 > 0) return 0;
			int32_t t = FIXP*d1/(d1-d2);
			if (t > lower) lower = t;
			else continue;
		} else {
			if (d2 <= 0) continue;
			// This still works out to be between 0-FIXP since we flip
			// the sign on numer & denom compared to the other branch.
			int32_t t = FIXP*d1/(d1-d2);
			if (t < upper) upper = t;
			else continue;
		}
		// No hit.
		// The "equals" case is ambiguous; we choose to interpret as "no hit".
		if (lower >= upper) return 0;
	}

	// Okay! All that rigamarole later, we still haven't
	// returned, so the answer is "yes, we have a hit".
	range(i, 3) o[i] = magicPt[i] - v2[i];
	int64_t dist = dot(o, sampleNorm);
	if (dist <= 0) return 0; // We started inside but have already left.
	/*if (debug) {
		range(i, 3) o[i] = magicPt[i] - v1[i];
		int64_t tmp = dot(o, sampleNorm);
		printf("Hm, %ld -> %ld\n", tmp, dist);
	}*/

	// We need forward rotations, not inverse rotations, for this part.
	imat_flipRot(rot1);
	imat_flipRot(rot2);

	imat_apply(forceDir_out, rot2, sampleNorm);
	vec_norm(forceDir_out);
	// We're done with v1/v2 by now,
	// and I need them to figure out
	// how much `magicPt`'s rotation 
	// changes the contact velocity.
	imat_applySm(v1, rot1, magicPt);
	imat_applySm(v2, rot2, magicPt);
	range(i, 3) if (v1[i] != v2[i]) puts("woah"); // tmp
	range(i, 3) if (s->vel[i] != 0) puts("oof"); // tmp
	// For now the other thing is a player (a sphere that doesn't spin),
	// so its contact point doesn't matter.
	range(i, 3) contactVel_out[i] = p->vel[i] - s->vel[i] + v1[i] - v2[i];

	return dist;
}

// Next up is probably figuring out how to collide something that has a number of spheres
// with a whole nasty nested network of solids.
// Maybe a related problem, do we keep this network around? Or do we add things to it as we go???

static char raycast_inner(fraction *best, mover const *m, offset const vWorld, iquat const q, unitvec const dir) {
	imat rot;
	imatFromIquatInv(rot, q);
	offset vSolid;
	imat_applySm(vSolid, rot, vWorld);
	unitvec dirSolid;
	imat_apply(dirSolid, rot, dir);

	int shape;
	int64_t r;
	if (m->type & T_PLAYER) {
		shape = 0;
		r = PLAYER_SHAPE_RADIUS;
	} else {
		shape = m->type;
		solid *s = solidFromMover(m);
		r = s->r;
	}

	fraction lower = {.numer = 0, .denom = 1};
	fraction upper = *best;
	shapeSpec &sh = shapeSpecs[shape];
	range(i, sh.numFaces) {
		int32_t const *norm = sh.facings[i];
		fraction frac = {
			// Todo both addends contain a `/FIXP`, could probably factor that out if I care?
			.numer = dot(vSolid, norm) + r*sh.distances[i]/FIXP,
			.denom = dot(dirSolid, norm)
		};
		if (frac.denom < 0) {
			frac.denom *= -1;
			frac.numer *= -1;
			if (lower.lt(frac)) lower = frac;
			else continue;
		} else {
			if (frac.lt(upper)) upper = frac;
			else continue;
		}
		if (!lower.lt(upper)) return 0;
	}
	// We got to the end and didn't exit for any reason, probably we're a winner
	*best = lower;
	return 1;
}

char raycast(fraction *best, mover *m, offset const origin, unitvec const dir) {
	offset vWorld;
	range(i, 3) vWorld[i] = m->oldPos[i] - origin[i];

	return raycast_inner(best, m, vWorld, m->oldRot, dir);
}

char raycast_interp(fraction *best, mover *m, offset const origin1, offset const origin2, unitvec const dir, float interpRatio) {
	offset vWorld;
	range(i, 3) {
		int64_t x1 = m->oldPos[i] - origin1[i];
		int64_t x2 = m->pos[i]    - origin2[i];
		vWorld[i] = x1 + (int64_t)(interpRatio*(x2-x1));
	}
	iquat interpQuat;
	// I have no idea if interpolating quaternions in this way
	// is reasonably accurate, but it's sure cheap!
	range(i, 4) interpQuat[i] = m->oldRot[i] + (int32_t)(interpRatio*(m->rot[i] - m->oldRot[i]));

	return raycast_inner(best, m, vWorld, interpQuat, dir);
}
