#include <math.h>

#include "gamestate.h"

#include "collision.h"
#include "game.h"
#include "game_gamestate.h"
#include "main.h"
#include "random.h"

#include "player.h"

static int32_t pl_traction = 3; // This is 3x. If I want some fancy fraction, add a denominator, idk.
static int32_t pl_speed = 350;
static int64_t pl_walkForce = 60;
// `pl_jump > pl_speed` means that for some slopes it's faster to bunny-hop. This is fine.
static int64_t pl_jump = 400;
static int64_t pl_gummy = 80;

static list<mover*> queryResults;

static char playerPhysLe(mover* const &a, mover* const &b) {
	// Simple for now.
	// We check higher objects first,
	// mostly because this reduces the ability to jump off
	// horizontal seams in walls.
	return a->pos[2] >= b->pos[2];
}

void playerUpdate(gamestate *gs, player *p) {
	// We copy `rot`=>`oldRot` when player input happens.
	memcpy(p->m.oldPos, p->m.pos, sizeof(p->m.pos));
	if (!p->alive) {
		int divisor = 64;
		// TODO some way to go real slow (but I'm out of net inputs for now lol)
		if (p->shoot & 1) divisor /= 4;
		range(i, 3) p->m.pos[i] += p->inputs[i] / divisor;

		p->prox = gs->vb_root;
		return;
	}

	p->vel[2] -= gs_gravity; // gravity

	offset dest;
	range(i, 3) dest[i] = p->m.pos[i] + p->vel[i];

	queryResults.num = 0;
	p->prox = velbox_query(p->prox, p->m.pos, p->vel, 2000, &queryResults);
	unitvec forceDir;
	offset contactVel;
	int32_t time;
	queryResults.qsort(playerPhysLe);
	rangeconst(j, queryResults.num) {
		// Todo: We are blindly assuming this mover is part of a solid.
		//       It's a safe bet for now, since we only keep players in the
		//       velbox space briefly (and not right now), but it's brittle.
		solid *s = solidFromMover(queryResults[j]);
		// todo: I think at this point `p->m.pos` and `p->m.oldPos` are the same vector?
		//       If so, should change to `oldPos`, since that makes more sense in context.
		int64_t dist = collide_check(p->m.pos, dest, PLAYER_SHAPE_RADIUS, s, forceDir, contactVel, &time);
		if (!dist) continue;
		range(i, 3) contactVel[i] += p->vel[i];
		pl_phys_standard(gs, forceDir, contactVel, dist, dest, p);
	}

	memcpy(p->m.pos, dest, sizeof(dest));

	if (p->hitsCooldown) {
		p->hitsCooldown--;
		if (!p->hitsCooldown) {
			if (p->hits >= 3) {
				killPlayer(p);
				// Todo: Add gibs
			} else {
				p->hits = 0;
			}
		}
	}
}

void playerAddBox(gamestate *gs, player *_p) {
	player &p = *_p;

	if (!p.alive) {
		// Usually false
		if (p.m.b) {
			velbox_reclaimDead(p.m.b);
			p.m.b = NULL;
		}
		return;
	}

	// Usually true
	if (p.m.b) {
		velbox_reclaimDead(p.m.b);
	}

	box *b = p.m.b = velbox_alloc();
	memcpy(b->pos, p.m.oldPos, sizeof(b->pos));
	range(j, 3) b->vel[j] = p.m.pos[j] - p.m.oldPos[j];
	// TODO define for SHAPE_CUBE (=0)
	b->r = PLAYER_SHAPE_RADIUS*shapeDiagonalMultipliers[0];
	b->end = b->start + 1;
	b->data = &p.m;
	velbox_insert(p.prox, b);
}

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

void pl_phys_standard(gamestate *gs, unitvec const forceDir, offset const contactVel, int64_t dist, offset dest, player *p) {
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

	if (p->jump & 3) {
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
			// `p->jump` also counts jumps (modulus 8) so that we can generate
			// unique soundIds for back-to-back jumps.
			// If I didn't clear the `1` and `2` bits, you could jump off surfaces
			// so long as you held space. I think I like it as-is though.
			// (You could also trigger multiple jumps in one frame, a bit weird.)
			p->jump = (p->jump+4) & 0x1C;

			{
				offset v;
				range(i, 3) v[i] = p->vel[i] - contactVel[i];
				uint32_t soundId =
					0xFF00'0000
					+ (p - gs->players.items) * 0x1'0000
					+ p->jump;
				addSound(gs->clock+1, dest, v, soundId, SND_JUMP);
			}
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

void pl_postStep(gamestate *gs, player *p) {
	p->jump &= ~2; // Clear 'jump this frame' bit, as this frame has passed.
	char shootInput = p->shoot;
	p->shoot &= 1;

	toolInst *tool = p->tool;
	(*tool->defn->use)(gs, p, shootInput, tool);
}

// Doesn't really sensibly account for negative hits,
// but 0-hits are kind of meaningful.
void player_hit(gamestate *gs, int32_t soundTime, player *p, int hits) {
	// Sound stuff first.
	// For the moment `gs` is only needed for the sound, so passing it as `NULL`
	// is treated as "no sound please"
	if (gs) {
		int who = p - gs->players.items;
		p->hitsCount++;
		uint32_t soundId =
			0xFF00'0100
			+ who * 0x1'0000
			+ p->hitsCount;
		uint32_t seed = soundId;
		int variant = splitmix32(&seed) % 3;
		addPlayerSound(soundTime, who, soundId, SND_OOF_A + variant);
	}

	int oldHits = p->hits;
	p->hits += hits;

	if (oldHits < 3) {
		if (p->hits < 3) {
			// 7 seconds to heal feels about right??
			p->hitsCooldown = 15*7;
		} else {
			// Enough time for them to get off one more shot
			p->hitsCooldown = 10;
		}
	}
}

void player_init() {
	queryResults.init();
}

void player_destroy() {
	queryResults.destroy();
}
