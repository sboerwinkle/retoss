#include <math.h>

#include "gamestate.h"
#include "matrix.h"

#include "collision.h"

//#include "main.h" // Just for `debug` computation, need to know if we're in the rootState

// In case I ever need to go back to full dot products,
// I'm still expressing them as such.
// In reality it's a lot simpler when you're taking a
// dot product with a basis vector!
// This function will probably be inlined.
static int64_t dot(offset v, int dim, int sign) {
	return sign*v[dim];
}
static int32_t dot(unitvec v, int dim, int sign) {
	return sign*v[dim];
}

// Todo `radius` should be int64_t
int64_t collide_check(offset const oldPos, offset const dest, int32_t radius, solid *s, unitvec forceDir_out, offset contactVel_out, int32_t *time_out) {
	/*
	char debug = s->m.type == 1 && s->tex == 5;
	char debug =
		s->m.pos[0] == -6300 && s->m.pos[1] == 5400 && s->m.pos[2] == 1600
		&& dest[0] >= -6400 && oldPos[0] <= -6200;
	*/
	offset v1raw;
	range(i, 3) v1raw[i] = oldPos[i] - s->m.oldPos[i];
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
	range(i, 6) {
		// Okay, previously this was a dot product,
		// but we can way simplify that now that our
		// normal vectors (`facings`) are always
		// basis vectors (i.e. x,y,z).
		int dim = i/2;
		int sign = (i%2) ? -1 : 1;
		int64_t limit = s->dimens[dim];
		int64_t d1 = dot(v1, dim, sign) - limit;
		int64_t d2 = dot(v2, dim, sign) - limit;

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
		int dim = winner1/2;
		int sign = (winner1%2) ? -1 : 1;
		range(i, 3) magicPt[i] = v1[i];
		magicPt[dim] -= sign*best;
	}

	// Phase 2: Check for a second positive face, which would mean an edge.
	// TODO: Be more scientific about any FIXP errors that might happen.
	//       I feel like between truncating `best` and truncating the `magicPt`
	//       displacement we might have some leftover distance left to cover (that we don't care about!)
	best = 1;
	int winner2;
	range(i, 6) {
		int dim = i/2;
		int sign = (i%2) ? -1 : 1;
		int64_t limit = s->dimens[dim];

		int64_t d1 = dot(magicPt, dim, sign) - limit;

		if (d1 > best) {
			best = d1;
			winner2 = i;
		}
	}

	unitvec sampleNorm;
	if (best == 1) {
		// Only one face required adjusting, so we don't need to do any math to get the
		// sample vector; we can just use that face's normal.
		sampleNorm[0] = sampleNorm[1] = sampleNorm[2] = 0;
		int dim = winner1/2;
		int sign = (winner1%2) ? -1 : 1;
		sampleNorm[dim] = sign*FIXP;
		// We can also skip the 3rd face check
		goto foundSampleNorm;
	}
	// Adjustment of `magicPt` won't be so straightforward this time...
	// Direction will be `winner2 - winner1*dot(w1,w2)`
	{
		/*
		Lol. I'm keeping this logic (again, if I ever need it...),
		but when our winners are basis vectors, their dot is zero.
		int32_t const *norm1 = sh.facings[winner1];
		int32_t const *norm2 = sh.facings[winner2];
		int32_t cosine = dot(norm1, norm2);
		int32_t scale = FIXP-cosine*cosine/FIXP;
		range(i, 3) magicPt[i] -= best*(norm2[i]-norm1[i]*cosine/FIXP)/scale;
		*/
		int dim = winner2/2;
		int sign = (winner2%2) ? -1 : 1;
		magicPt[dim] -= sign*best;
	}

	// Phase 3: One more go-round, to find corners. It's rough, but too bad!
	best = 1;
	int winner3;
	range(i, 6) {
		int dim = i/2;
		int sign = (i%2) ? -1 : 1;
		int64_t limit = s->dimens[dim];

		int64_t d1 = dot(magicPt, dim, sign) - limit;

		if (d1 > best) {
			best = d1;
			winner3 = i;
		}
	}

	if (best != 1) {
		/* Again, leftover logic that's more than we need.
		// Oh boy, the most fun.
		unitvec v3;
		cross(v3, sh.facings[winner1], sh.facings[winner2]);
		int32_t dot_prod = dot(v3, sh.facings[winner3]);
		// This one seems way too simple, but I think it's correct??
		range(i, 3) magicPt[i] -= best*v3[i]/dot_prod;
		*/
		int dim = winner3/2;
		int sign = (winner3%2) ? -1 : 1;
		magicPt[dim] -= sign*best;
	}

	// Then we've got to actually to do the scaling to get our test point.
	{
		offset o;
		range(i, 3) o[i] = v1[i] - magicPt[i];
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
	range(i, 6) {
		int dim = i/2;
		int sign = (i%2) ? -1 : 1;
		// We pad this out a couple units.
		// This makes it easier to catch on edges when you shouldn't,
		// but hopefully fixes weird jitter going over edges.
		// Todo: See if we can skip this now that we're doing less fixed-point math
		int64_t limit = s->dimens[dim] + 2;

		int64_t d1 = dot(v1, dim, sign) - limit;
		int64_t d2 = dot(v2, dim, sign) - limit;

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

	*time_out = lower;

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
	// We don't know the geometry of the caller, so they're responsible for
	// accounting for their own velocity and spin.
	range(i, 3) contactVel_out[i] = s->m.oldPos[i] - s->m.pos[i] + v1[i] - v2[i];

	return dist;
}

char collide_sphere(offset const p1, offset const p2, int64_t radius, mover *m, int32_t *time) {
	offset v1, v2;
	offset delta;
	range(i, 3) {
		v1[i] = m->oldPos[i] - p1[i];
		v2[i] = m->pos[i] - p2[i];
		delta[i] = v1[i] - v2[i];
	}
	unitvec dir;
	int64_t mg = mag(delta);
	range(i, 3) {
		dir[i] = delta[i] * FIXP / mg;
	}

	int64_t dist = raycast_sphere(v1, dir, radius);

	if (dist >= 0 && dist <= mg) {
		*time = dist * FIXP / mg;
		return 1;
	}
	return 0;
}

// Todo: Actually returns the distance until closest approach (if it hits), not
//       the distance until it hits the surface. Doesn't really matter for our
//       current usages. Will have to handle trying to square `radius` if it
//       needs to be better...
int64_t raycast_sphere(offset const pos, unitvec const dir, int64_t radius) {
	int64_t x = dot(pos, dir);
	if (x < 0) return -1;
	offset p;
	range(i, 3) {
		p[i] = pos[i] - x * dir[i] / FIXP;
	}
	int64_t lowerBound = std::max(labs(p[0]), std::max(labs(p[1]), labs(p[2])));
	if (lowerBound >= radius) return -1;
	if (mag(p) >= radius) return -1;
	return x;
}

// Next up is probably figuring out how to collide something that has a number of spheres
// with a whole nasty nested network of solids.
// Maybe a related problem, do we keep this network around? Or do we add things to it as we go???

static char raycast_inner(fraction *best, mover const *m, offset const vWorld, imat const rot, unitvec const dir) {
	int64_t const *dimens;
	int32_t type = m->type & T_MASK;
	if (type == T_PLAYER) {
		dimens = (int64_t const[]){PLAYER_SHAPE_RADIUS, PLAYER_SHAPE_RADIUS, PLAYER_SHAPE_RADIUS};
	} else if (type == T_PROJ) {
		fraction tmp = { .denom = FIXP };
		tmp.numer = raycast_sphere(vWorld, dir, m->b->r);
		if (tmp.numer < 0) return 0;
		if (best->lt(tmp)) return 0;
		*best = tmp;
		return 1;
	} else {
		solid *s = solidFromMover(m);
		dimens = s->dimens;
	}

	offset vSolid;
	imat_applySm(vSolid, rot, vWorld);
	unitvec dirSolid;
	imat_apply(dirSolid, rot, dir);

	fraction lower = {.numer = 0, .denom = 1};
	fraction upper = *best;
	range(i, 6) {
		int dim = i/2;
		int sign = (i%2) ? -1 : 1;
		fraction frac = {
			.numer = dot(vSolid, dim, sign) + dimens[dim],
			.denom = dot(dirSolid, dim, sign)
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
	imat rot;
	imatFromIquatInv(rot, m->oldRot);

	return raycast_inner(best, m, vWorld, rot, dir);
}

char raycast_interp(fraction *best, mover *m, offset const origin1, offset const origin2, unitvec const dir, float interpRatio) {
	offset vWorld;
	range(i, 3) {
		int64_t x1 = m->oldPos[i] - origin1[i];
		int64_t x2 = m->pos[i]    - origin2[i];
		vWorld[i] = x1 + (int64_t)(interpRatio*(x2-x1));
	}
	imat rot1, rot2;
	imatFromIquatInv(rot1, m->oldRot);
	imatFromIquatInv(rot2, m->rot);
	range(i, 9) rot1[i] += (int32_t)(interpRatio*(rot2[i] - rot1[i]));

	return raycast_inner(best, m, vWorld, rot1, dir);
}
