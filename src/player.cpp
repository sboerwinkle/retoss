#include <math.h>

#include "gamestate.h"
#include "bcast.h"

#include "player.h"

int32_t pl_traction = 3; // This is 3x. If I want some fancy fraction, add a denominator, idk.
int32_t pl_speed = 350;
int64_t pl_walkForce = 60;
// `pl_jump > pl_speed` means that for some slopes it's faster to bunny-hop. This is fine.
int64_t pl_jump = 400;
int64_t pl_gummy = 80;

// TODO: We'll deal with this later, but I don't think the input desire should be rotated.
//       Instead, just project it onto the lateral plane and scale it up.
//       It may come out to be the zero vector, that's okay too.
//       Maybe better is to double or triple the length, project it down,
//       and shrink it if necessary.
static void rotateInputDesire(unitvec out, unitvec const in, unitvec const norm) {
	// Todo: There's bound to be lots of optimizations I can do here,
	//       since we know lots of things about our inputs.
	//       We're not going to bother with that for now.
	unitvec const up = {0, 0, FIXP};
	int32_t const cosine = dot(norm, up);
	unitvec axis;
	cross(axis, up, norm);

	int32_t divisor = FIXP+cosine;
	// Wikipedia article gives the matrix for a rotation around a vector by an angle in terms of
	// the vector <u_x,u_y,u_z> and the angle Theta. A common expression is `u_i*u_j*(1-cos(Theta))`.
	// However, our cross product components correspond to `u_i*sin(Theta)`. If we multiply two of those:
	// `u_i*u_j*sin(Theta)^2`           // Multiply and group terms
	// `u_i*u_j*(1-cos(Theta)^2)`       // Trig identity
	// `u_i*u_j*(1+cos(Th))*(1-cos(Th)) // Factor quadratic term
	// At this point, all we have to do is divide by `1+cos(Th)` to arrive at the common expression
	// we were after. The other expressions required to construct this matrix all work out nicely
	// so that we never actually have to normalize our rotation vector (`axis`), or do any square roots
	// at all! Somewhat surprisingly, we don't even have to use `FIXP` past this point!
	// Isn't that convenient? It's not a coincidence, it's just math, but it's sure convenient.

	if (!divisor) {
		// What if the vectors are opposites?
		// It's a 180 deg rotation, but there's no way to pick the rotation axis.
		// Arbitrarily, we choose Y.
		out[0] = -in[0];
		out[1] = in[1];
		out[2] = -in[2];
		return;
	}

	int32_t xy = axis[0]*axis[1]/divisor;
	int32_t xz = axis[0]*axis[2]/divisor;
	int32_t yz = axis[1]*axis[2]/divisor;
	int32_t xx = axis[0]*axis[0]/divisor;
	int32_t yy = axis[1]*axis[1]/divisor;
	int32_t zz = axis[2]*axis[2]/divisor;
	// Also this is laid out as the transpose of the "real" matrix.
	// We follow OpenGL's convention on matrix layout, but that unfortunately doesn't
	// lend itself to pretty presentation in the source code.
	imat const rotationMat = {
		xx + cosine, xy + axis[2], xz - axis[1],
		xy - axis[2], yy + cosine, yz + axis[0],
		xz + axis[1], yz - axis[0], zz + cosine,
	};
	imat_apply(out, rotationMat, in);
}

static void getLatChange(offset output, int32_t *input, offset landSpeed, int64_t latForce) {
	range(i, 3) output[i] = input[i] - landSpeed[i];
	bound64(output, latForce);
}

void pl_phys_standard(unitvec const forceDir, offset const contactVel, int64_t dist, offset dest, player *p) {
	// Push us out of the wall
	range(i, 3) dest[i] += dist*forceDir[i]/FIXP;
	int64_t normalForce = -dot(contactVel, forceDir);
	// Typically `normalForce` should be positive,
	// but for various reasons the amount something moves
	// won't always equal its velocity, so we can't
	// rely on that fact.
	if (normalForce <= 0) return;

	unitvec desire = {p->inputs[0], p->inputs[1], 0};
	// For some reason we don't circularize the square inputs earlier,
	// so I'm just doing it here.
	bound26(desire, FIXP);

	if (p->jump) {
		unitvec jumpDir;
		if (desire[0] || desire[1]) {
			// The constant mult+div is basically `/sqrt(2)`.
			// Could maybe do this as multiplying by a hardcoded float,
			// since there shouldn't be much room for interpretation by the FPU,
			// but I've been doing everything else as fixed-point anyway.
			jumpDir[0] = desire[0] * 23170 / FIXP;
			jumpDir[1] = desire[1] * 23170 / FIXP;
			// Obviously the true value is irrational;
			// we rounded the horiz component down, so
			// we round the vert component up to get a
			// bit closer to 1.0 total magnitude.
			jumpDir[2] = 23171;
		} else {
			// Todo: Second place now that I'm using this.
			//       Neither case (jump, shoot) is super common though...
			//       is it worth saving this somewhere?
			unitvec look;
			iquat_apply(look, p->m.rot, ((unitvec const){0, FIXP, 0}));
			// If we're looking completely up or down we get weird results.
			// Pick a differet vector if we're too close to either pole.
			// Wouldn't have to go through this nonsense if we sent pitch/yaw directly...
			if (look[2] > 30000) {
				iquat_apply(look, p->m.rot, ((unitvec const){0, 0, -FIXP}));
			} else if (look[2] < -30000) {
				iquat_apply(look, p->m.rot, ((unitvec const){0, 0, FIXP}));
			}
			int32_t div = sqrt(look[0]*look[0] + look[1]*look[1]);

			// This angle was computed as "cos = 0.1", not sure about like degrees/radians
			jumpDir[0] = look[0] * 3277 / div;
			jumpDir[1] = look[1] * 3277 / div;
			jumpDir[2] = 32604;
		}
		if (dot(forceDir, jumpDir) > 0) {
			// Debated back and forth about this line, I think I want it.
			p->jump = 0;

			range(i, 3) p->vel[i] += jumpDir[i]*pl_jump/FIXP - contactVel[i];
			// `dest` has already been updated to push us out of the collision plane,
			// and unlike the walking case we don't want to update `dest` with the velocity change.
			// (mostly this is to make chained wall jumps feel more impactful,
			//  but at the cost of normal jumps effectively happening one frame later)
			return;
		}
	}

	if (normalForce <= pl_gummy) {
		// appliedForce would be 0, so nothing to do.
		return;
	}
	int64_t appliedForce = normalForce - pl_gummy;
	// Push player velocity out of collision face (except `pl_gummy` amount)
	range(i, 3) p->vel[i] += appliedForce*forceDir[i]/FIXP;

	if (forceDir[2] <= 0) {
		// No traction (latForce), no need to do more.
		return;
	}
	int64_t latForce = appliedForce*forceDir[2]*pl_traction/FIXP;
	if (latForce > pl_walkForce) latForce = pl_walkForce;

	unitvec rotatedDesire;
	rotateInputDesire(rotatedDesire, desire, forceDir);
	bound26(rotatedDesire, FIXP); // Todo I think this might be moot, since I'm circularizing eralier.
	range(i, 3) rotatedDesire[i] = rotatedDesire[i]*pl_speed/FIXP;

	offset landSpeed;
	// This works out to be along the surface
	range(i, 3) landSpeed[i] = contactVel[i] + normalForce*forceDir[i]/FIXP;

	offset latChange;
	getLatChange(latChange, rotatedDesire, landSpeed, latForce);

	//printf("%03d, %03d, %03d (%03ld)\r", desiredChange[0], desiredChange[1], desiredChange[2], normalForce);
	//fflush(stdout);

	// This is lateral force (traction; movement over surface)
	range(i, 3) p->vel[i] += latChange[i];
	// Mostly to prevent gradual slipping
	range(i, 3) dest[i] += latChange[i];
}

static void shoot(gamestate *gs, player *p) {
	// Todo: Surely we'll need this more often, right? Save it somewhere?
	unitvec look;
	iquat_apply(look, p->m.rot, ((unitvec const){0, FIXP, 0}));

	fraction const limit = {.numer = 100'000, .denom = FIXP};
	// Todo: I think this may be a bit sloppy at the moment, since a box that's a suitable
	//       parent for something that large will necessarily be bigger than we need.
	//       Might be able to improve this with a small velbox tweak involving `minParentR`.
	box *queryArea = velbox_findParent(p->prox, p->m.oldPos, p->vel, limit.numer);
	bcast_start(queryArea, look, p->m.oldPos);
	fraction time;
	mover *result;
	do {
		result = bcast(&time, look, p->m.oldPos);
	} while (result == &p->m);
	if (!result) {
		time = limit;
	} else if (limit.lt(time)) {
		time = limit;
		result = NULL;
	}
	if (result && result->type & T_PLAYER) {
		player *shootee = playerFromMover(result);
		if (shootee->hits < 2) {
			shootee->hits++;
			// 7 seconds to heal feels about right??
			shootee->hitsCooldown = 15*7;
		} else {
			shootee->hits = 3;
			// Enough time for them to get off one more shot
			shootee->hitsCooldown = 10;
		}
	}

	trail &tr = gs->trails.add();
	memcpy(tr.origin, p->m.oldPos, sizeof(offset));
	memcpy(tr.dir, look, sizeof(unitvec));
	tr.len = time.numer*FIXP/time.denom;
	tr.expiry = vb_now + TRAIL_LIFETIME;
}

void pl_postStep(gamestate *gs, player *p) {
	p->jump &= 1; // Keep only the 'jump continuing' bit
	char shootInput = p->shoot;
	p->shoot &= 1;
	if (p->cooldown) p->cooldown--;
	if (shootInput && !p->cooldown) {
		p->cooldown = 10;
		shoot(gs, p);
	}
}
