#include <math.h>

#include "gamestate.h"
#include "bcast.h"

#include "player.h"

int32_t pl_traction = 3; // This is 3x. If I want some fancy fraction, add a denominator, idk.
int32_t pl_speed = 350;
int64_t pl_walkForce = 60;
int64_t pl_jumpForce = 700; // This used to be much smaller (3x walk speed), but this should be more fun.
int64_t pl_jump = 300;
int64_t pl_gummy = 30;

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

// "We've had one jump helper function, yes."
// For any context about wtf I'm doing, check `jumpHelper`.
static void secondJumpHelper(int32_t *jumpLateralDest, unitvec const jumpNormal, char vertical) {
	int32_t const latSpeed = pl_jump; // Previously this was `pl_speed`, but I'm trying this instead
	int64_t yzSq = jumpNormal[1]*jumpNormal[1] + jumpNormal[2]*jumpNormal[2];
	int64_t nearContactDiv = yzSq/FIXP;
	if (!nearContactDiv) {
		jumpLateralDest[0] = 0;
		// This is much more likely when `vertical`,
		// but also possible for non-vertical cases if `desire`
		// lies parallel to a vertical wall.
		if (vertical) {
			jumpLateralDest[1] = 0;
			jumpLateralDest[2] = latSpeed;
		} else {
			jumpLateralDest[1] = latSpeed;
			jumpLateralDest[2] = 0;
		}
		return;
	}
	int64_t nearContactY = jumpNormal[1] * pl_jump / nearContactDiv;
	int64_t nearContactZ = jumpNormal[2] * pl_jump / nearContactDiv;
	int64_t lateralMotionSpentSq = pl_jump*pl_jump*jumpNormal[0]*jumpNormal[0]/nearContactDiv/FIXP;
	int64_t leftoverSq = latSpeed*latSpeed - lateralMotionSpentSq;
	int32_t yzDist = sqrt(yzSq);
	if (leftoverSq <= 0) {
		int64_t speed = latSpeed;
		if (jumpNormal[0] < 0) speed *= -1;
		jumpLateralDest[0] = -yzDist * speed / FIXP;
		jumpLateralDest[1] = jumpNormal[0] * jumpNormal[1] / FIXP * speed / yzDist;
		jumpLateralDest[2] = jumpNormal[0] * jumpNormal[2] / FIXP * speed / yzDist;
	} else {
		// We were able to make it to the Y,Z plane with motion left to spare.
		// Find the best spot on the Y,Z plane we can reach.
		// At the end we'll subtract off the jump amount to just leave the lateral amount.
		jumpLateralDest[0] = 0;
		if (vertical) {
			jumpLateralDest[1] = nearContactY;
			jumpLateralDest[2] = nearContactZ;
		} else {
			int32_t slideY = jumpNormal[2]*FIXP/yzDist;
			int32_t slideZ = -jumpNormal[1]*FIXP/yzDist;
			int64_t leftover = sqrt(leftoverSq);
			// And here's where it gets weird, like with various quadrants to consider.
			// I did confirm that, for a line with slope `m` (presumably negative),
			// the optimal placement satisfies `y=-m*x`.
			if (jumpNormal[1] <= 0 || jumpNormal[2] <= 0) {
				if (jumpNormal[2] <= 0) leftover *= -1;
				jumpLateralDest[1] = nearContactY + slideY*leftover/FIXP;
				jumpLateralDest[2] = nearContactZ + slideZ*leftover/FIXP;
			} else {
				int64_t guessY, guessZ;
				if (jumpNormal[1] > jumpNormal[2]) {
					guessY = nearContactY - slideY*leftover/FIXP;
					guessZ = nearContactZ - slideZ*leftover/FIXP;
					if (jumpNormal[2]*guessZ > jumpNormal[1]*guessY) {
						// We went too far, calculate (and snap to) optimum position.
						// (nearContactZ+slideZ*x/FIXP) / (nearContactY+slideY*x/FIXP) = jumpNormal[1]/jumpNormal[2]
						// (nearContactZ+slideZ*x/FIXP)*jumpNormal[2] = (nearContactY+slideY*x/FIXP)*jumpNormal[1]
						// x*(slideZ*jn[2]/FIXP-slideY*jn[1]/FIXP) = ncy*jn[1]-ncz*jn[2]
						int64_t slideAmt = (nearContactY*jumpNormal[1]-nearContactZ*jumpNormal[2])/((slideZ*jumpNormal[2]-slideY*jumpNormal[1])/FIXP);
						jumpLateralDest[1] = nearContactY + slideY*slideAmt/FIXP;
						jumpLateralDest[2] = nearContactZ + slideZ*slideAmt/FIXP;
					} else {
						jumpLateralDest[1] = guessY;
						jumpLateralDest[2] = guessZ;
					}
				} else {
					// I think I could maybe collapse this with the `if` case,
					// but it's not super straightforward so I'm leaving it as-is.
					guessY = nearContactY + slideY*leftover/FIXP;
					guessZ = nearContactZ + slideZ*leftover/FIXP;
					if (jumpNormal[1]*guessY > jumpNormal[2]*guessZ) {
						// (same as other case)
						int64_t slideAmt = (nearContactY*jumpNormal[1]-nearContactZ*jumpNormal[2])/((slideZ*jumpNormal[2]-slideY*jumpNormal[1])/FIXP);
						jumpLateralDest[1] = nearContactY + slideY*slideAmt/FIXP;
						jumpLateralDest[2] = nearContactZ + slideZ*slideAmt/FIXP;
					} else {
						jumpLateralDest[1] = guessY;
						jumpLateralDest[2] = guessZ;
					}
				}
			}
		} // End of "we had motion leftover" cases.
		range(i, 3) jumpLateralDest[i] -= jumpNormal[i]*pl_jump/FIXP;
	}
}

static void jumpHelper(int32_t *jumpLateralDest, unitvec const forceDir, unitvec const desire) {
	// The X,Y direction the player wants to move (`desire`) defines a vertical plane.
	// The priorities of jumping are to:
	//   1. Get the player's motion (relative to the touched block) to lie in that plane
	//   2. To maximize travel distance for the jump (assuming they start at "ground level").
	// The first step is doing a rotation so that the desired vertical plane is the Y,Z plane.
	//   (We'll rotate back later)
	// This function just assumes lateral motion (along the surface) can be any vector inside
	// a circle centered at the origin; the realities of traction are handled outside this fn.
	// This does mean there could be times where the jump could reach the Y,Z plane, but instead
	// targets a different point in the Y,Z plane which can't quite be reached w/ current traction.
	// This is because handling all the edge cases for that is a nightmare.
	char vertical = !(desire[0] || desire[1]);
	unitvec jumpNormal;
	int32_t xyDist = 0; // Only used in vertical case, we need it to rotate back.
	// TODO with all these transforms back and forth,
	//      I'd love to keep some of this at a higher precision.
	//      Need to review this once it's all done and see if we
	//      have the bits to spare.
	if (vertical) {
		// Jumping straight up is a special case added later,
		// so we fudge some stuff so we can mostly follow the
		// original flow.
		jumpNormal[2] = forceDir[2];
		jumpNormal[1] = 0;
		jumpNormal[0] = sqrt(forceDir[0]*forceDir[0]+forceDir[1]*forceDir[1]);
		xyDist = jumpNormal[0];
	} else {
		jumpNormal[2] = forceDir[2]; // We're rotating around the Z axis, so that one is unchanged.
		jumpNormal[0] = (desire[1]*forceDir[0] - desire[0]*forceDir[1]) / FIXP;
		jumpNormal[1] = (desire[0]*forceDir[0] + desire[1]*forceDir[1]) / FIXP;
	}

	int32_t rotatedResult[3];
	secondJumpHelper(rotatedResult, jumpNormal, vertical);

	jumpLateralDest[2] = rotatedResult[2];
	if (vertical) {
		// We expect `rotatedResult[1]` to be 0 in this case.
		// There's no concept of "forward", which is what that would represent.
		// Also, if xyDist is 0 (like a flat floor), `rotatedResult[0]` should also be 0.
		if (!xyDist) xyDist = 1;
		jumpLateralDest[0] = rotatedResult[0] * forceDir[0] / xyDist;
		jumpLateralDest[1] = rotatedResult[0] * forceDir[1] / xyDist;
	} else {
		jumpLateralDest[0] = ( desire[1]*rotatedResult[0] + desire[0]*rotatedResult[1]) / FIXP;
		jumpLateralDest[1] = (-desire[0]*rotatedResult[0] + desire[1]*rotatedResult[1]) / FIXP;
	}

}

static void getLatChange(offset output, int32_t *input, offset landSpeed, int64_t latForce) {
	range(i, 3) output[i] = input[i] - landSpeed[i];
	bound64(output, latForce);
}

void pl_phys_standard(unitvec const forceDir, offset const contactVel, int64_t dist, offset dest, player *p) {
	// update pos according to normal + pos difference (input?)
	// update vel according to normal + input vel difference
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

	offset landSpeed;
	// This works out to be along the surface
	range(i, 3) landSpeed[i] = contactVel[i] + normalForce*forceDir[i]/FIXP;

	// We know these values are bounded, and should fit comfortably in 32 bits.
	offset latChange;
	int64_t appliedForce;
	if (p->jump) {
		int32_t jumpLateralDest[3];
		jumpHelper(jumpLateralDest, forceDir, desire);

		appliedForce = normalForce + pl_jump;

		getLatChange(latChange, jumpLateralDest, landSpeed, pl_jumpForce);

		int64_t dot =
			(contactVel[0] + appliedForce*forceDir[0]/FIXP + latChange[0])*desire[0] +
			(contactVel[1] + appliedForce*forceDir[1]/FIXP + latChange[1])*desire[1];
		if (dot >= 0) {
			p->jump = 0; // Debated back and forth about this line, I think I want it.
			goto applyForces;
		}
		// TODO Print: jumpLateralDest, rotated form, final lateral speed, final global speed, move direction (X,Y), decision.
	}

	unitvec rotatedDesire;
	rotateInputDesire(rotatedDesire, desire, forceDir);
	bound26(rotatedDesire, FIXP); // Todo I think this might be moot, since I'm circularizing eralier.
	range(i, 3) rotatedDesire[i] = rotatedDesire[i]*pl_speed/FIXP;

	if (normalForce > pl_gummy) {
		appliedForce = normalForce - pl_gummy;
	} else {
		// appliedForce would be 0, so nothing to do.
		return;
	}

	{
		int64_t latForce = 0;
		if (forceDir[2] > 0) {
			latForce = appliedForce*forceDir[2]*pl_traction/FIXP;
			if (latForce > pl_walkForce) latForce = pl_walkForce;
		}
		getLatChange(latChange, rotatedDesire, landSpeed, latForce);
	}

	applyForces:;

	//printf("%03d, %03d, %03d (%03ld)\r", desiredChange[0], desiredChange[1], desiredChange[2], normalForce);
	//fflush(stdout);

	// This is lateral force (traction; movement over surface)
	range(i, 3) p->vel[i] += latChange[i];
	range(i, 3) dest[i] += latChange[i]; // Hopefully prevents gradual slipping?
	// This is normal force (stopping the collision, plus maybe a jump)
	range(i, 3) p->vel[i] += appliedForce*forceDir[i]/FIXP;
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
	tr.expiry = vb_now + 45; // 3 sec, roughly
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
