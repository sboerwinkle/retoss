#include <math.h>
#include "../util.h"

#include "../matrix.h"
#include "../gamestate.h"
#include "../bctx.h"
#include "../collision.h"
#include "../task.h"
#include "../serialize.h"

#include "dynamics.h"

#define RADIUS_I_GUESS 800
#define KNOB_SPACING (RADIUS_I_GUESS*3/8)
#define KNOB_R (RADIUS_I_GUESS*5/8)
#define MAX_ARM 1600
// Need `FIXP*MAX_ARM*MAX_ARM` to fit in int64_t.
// This means MAX_ARM needs to fit in 24 bits,
// and if it's too big we can scale it some.
#define ROT_INERT_SCALE (MAX_ARM/(1<<24) + 1)
// Rotational inertia has units kg*m*m,
// and since space is fine-grained in this world,
// rotational inertia values are quite large.
#define ROT_INERT 640'000

static const offset cornerDefns[8] = {
	{ KNOB_SPACING,  KNOB_SPACING,  KNOB_SPACING},
	{-KNOB_SPACING,  KNOB_SPACING,  KNOB_SPACING},
	{-KNOB_SPACING,  KNOB_SPACING, -KNOB_SPACING},
	{ KNOB_SPACING,  KNOB_SPACING, -KNOB_SPACING},
	{ KNOB_SPACING, -KNOB_SPACING, -KNOB_SPACING},
	{-KNOB_SPACING, -KNOB_SPACING, -KNOB_SPACING},
	{-KNOB_SPACING, -KNOB_SPACING,  KNOB_SPACING},
	{ KNOB_SPACING, -KNOB_SPACING,  KNOB_SPACING},
};

struct pendingCollide {
	int64_t dist;
	offset o1;
	unitvec forceDir;
	offset contactVel;
};

static void step(gamestate *gs, void *_data) {
	tskDynamicsData *data = (tskDynamicsData*)_data;

	memcpy(data->s.m.oldPos, data->s.m.pos, sizeof(offset));
	memcpy(data->s.m.oldRot, data->s.m.rot, sizeof(iquat));
	iquat_mult(data->s.m.rot, data->s.m.oldRot, data->rvel);
	iquat_norm(data->s.m.rot);

	data->vel[2] -= gs_gravity;
	range(i, 3) data->s.m.pos[i] += data->vel[i];

	box *p = data->s.b->parent;
	// For now these are 1-frame only boxes,
	// so we know it's dead!
	velbox_reclaimDead(data->s.b);

	list<mover*> toCheck;
	list<pendingCollide> collisions;
	// TODO: Usually I'm pretty aggressive about leaving lists allocated
	//       if I know I'm going to need them again. This could go in
	//       the task's data, I guess.
	toCheck.init();
	collisions.init();
	p = velbox_query(p, data->s.m.oldPos, data->vel, 2000, &toCheck);
	offset total_vel = {0}, total_pos = {0};
	offset total_rvel = {0}; // , total_turn = {0};
	rangeconst(iter, toCheck.num) {
		collisions.num = 0;
		pendingCollide *collide = &collisions.add();
		solid *s = solidFromMover(toCheck[iter]);
		// Todo: Could do a finer-grain check here using a sphere that contains the entire box?
		//       Probably worth it, even if it catches only a few false positives, since we have
		//       to do like 8 other checks otherwise...
		range(knob, 8) {
			offset o2, corner_p1, corner_p2;
			int64_t *o1 = collide->o1;
			// TODO I can convert the `iquat`s to `imat`s and
			//      take linear combinations of (rows? cols?)
			//      to get each corner.
			// TODO "Sm" naming is still dumb, still need to fix it
			iquat_applySm(o1, data->s.m.oldRot, cornerDefns[knob]);
			iquat_applySm(o2, data->s.m.rot, cornerDefns[knob]);
			range(k, 3) {
				corner_p1[k] = o1[k] + data->s.m.oldPos[k];
				corner_p2[k] = o2[k] + data->s.m.pos[k];
			}
			int64_t dist = collide_check(corner_p1, corner_p2, KNOB_R, s, collide->forceDir, collide->contactVel);
			if (dist) {
				collide->dist = dist;
				collide = &collisions.add();
			}
		}

		collisions.num--;
		if (!collisions.num) continue;

		int64_t totalWeight = 0;
		rangeconst(_c, collisions.num) totalWeight += collisions[_c].dist;
		rangeconst(_c, collisions.num) {
			pendingCollide &c = collisions[_c];
			// Our distances could be at quite large scales, so we have to reduce our weight down
			// to a more manageable size.
			// We're doing a simple weight by distance; I also considered a more involved method
			// (which would require sorting the collisions before processing them) where basically
			// each unit of distance gets some amount of weight, which is split between all
			// collisions with at least that much `dist` (and then normalized, ofc). This winds up
			// favoring the "weightier" collisions more than simple linear weighting does.
			int32_t weight = c.dist * FIXP / totalWeight;

			int64_t *o1 = c.o1;

			// TODO Terrible for now, optimize later
			range(k, 3) {
				o1[k] -= (int64_t) c.forceDir[k] * KNOB_R / FIXP;
			}
			// This only works b/c we're not changing rotations mid-frame
			offset o2;
			iquat_applySm(o2, data->rvel, o1);

			// Terrible horrible collision logic :(
			// Todo: Move this to a method? In a way the compiler will inline.
			range(k, 3) c.contactVel[k] += data->vel[k] + o2[k] - o1[k];
			int64_t normalForce = -dot(c.contactVel, c.forceDir);
			if (normalForce < 0) continue;

			// This bound might be a little conservative, I'd have to go check.
			if (normalForce > (1<<25)) normalForce = 1<<25;
			offset force, remainder;
			range(k, 3) {
				force[k] = c.forceDir[k] * normalForce / FIXP;
				remainder[k] = - c.contactVel[k] - force[k];
			}
			// Friction
			bound64(remainder, normalForce/2);
			range(k, 3) {
				force[k] += remainder[k];
				total_pos[k] += (c.dist*c.forceDir[k]/FIXP + remainder[k])*weight;
			}

			int64_t forceMag = mag(force);
			if (!forceMag) continue;
			range(k, 3) c.forceDir[k] = force[k] * FIXP / forceMag;

			// `rotationAxis` winds up having a magnitude equal to the lever arm (moment arm),
			// which we will make use of.
			offset rotationAxis;
			cross64(rotationAxis, o1, c.forceDir);

			// This is all a little goofy,
			// could probably just get `mag(rotationAxis)` instead of all this
			// `ROT_INSERT_SCALE` business. I'm leaving it for now though,
			// I think the advantages are a bit better performance and possibly
			// even more precision?

			int64_t armSqScaled = 0;
			range(k, 3) {
				int64_t tmp = rotationAxis[k]/ROT_INERT_SCALE;
				armSqScaled += tmp*tmp;
			}

			int32_t linearRatio = (int64_t) FIXP * ROT_INERT / (ROT_INERT + armSqScaled);
			range(k, 3) {
				total_vel[k] += force[k] * linearRatio / FIXP * weight;
				// TODO I think I still have an overflow problem...?
				total_rvel[k] += rotationAxis[k]/ROT_INERT_SCALE
					* (forceMag/ROT_INERT_SCALE)
					* (FIXP-linearRatio)
					/ armSqScaled
					* weight;
			}

			// What follows is a lot of thinking that I used to arrive at the above.

			// Linear response is just F/M (newtons -> m/s)
			// Angular response is Fdd/I (ignoring the sideways bit - lol)
			// fdd/I + f/M = R
			// So we have R, and I and M are arbitrary constants.
			// And we want to find:
			//     f/MR (ratio of force that gets applied linearly)
			//     fd/I (angular response in goddamn radians)
			// Okay, well, basic algebra to the rescue:
			//     fd/I = (R - f/M) / d
			// I can maybe find `d`, but I actually still don't know `f`...
			//     fddM + fI = RIM
			//     f = RIM / (ddM + I)
			// Also, I have one easy-to-compute intermediate, `X`, the cross product.
			// It's maybe easier to find the square of... by itself, it is a vector.
			//     X = dR
			// Scratch work:
			//     f/MR = 1 - fdd/IR
			//     f/MR = I / (ddM + I)
			//          = 1 - Mdd/(ddM + I)
			//     So yeah, our 2 def'ns agree. Good.
			// Next we assign M=1, because we only ever cared about the ratio of M vs I.
			// Also, we can't straight up find `cross(force, o1)` because of sizes,
			// so we'll need to reassign `forceDir` as a `unitvec` anyway!
			// Then since we know bounds on `d` we can safely compute `dd`.
			//     f/MR = I / (dd + I)
			// Okay, back to the angular response, may I rest in peace.
			//     fd/I = (R - f)/d
			//     fd/I = (R - RI/(dd+I))/d
			//          = R*(1-L)/d
			// That's kind of self-evident, just take what's left after the linear response
			// and convert to radians by dividing by the lever arm.
			// Well, but I'd really love to get that with a `* d`, since I need to convert
			// to a vector quantity anyways...
			//          = dR*(1-L)/dd
			// This is actually more reasonable than it looks. We probably already know |R|
			// as a side effect of converting it to a unitvec (or, we can swing it such that
			// we do), and we already have a scaled computation of `dd` lying around from
			// a previous step.
		}

		offset rvel; // , turn;
		range(i, 3) {
			data->vel[i] += (total_vel[i] + (total_vel[i] >= 0 ? FIXP-1 : 1-FIXP)) / FIXP;
			data->s.m.pos[i] += (total_pos[i] + (total_pos[i] >= 0 ? FIXP-1 : 1-FIXP)) / FIXP;
			rvel[i] = total_rvel[i] / FIXP;
			//turn[i] = total_turn[i] / FIXP;
			total_vel[i] = total_pos[i] = 0;
			total_rvel[i] = 0; // , total_turn[i] = 0;
		}
		// So right now `rvel` is in radians, but obviously we want a quaternion.
		// Also, we're going to limit acceleration to 1 radian per frame per frame,
		// which is frankly pretty fast. I might still relax this limit some later.
		int64_t magRvel = mag(rvel);
		if (!magRvel) continue;
		int32_t halfAngle;
		if (magRvel >= FIXP) halfAngle = 10430;
		else halfAngle = 10430*magRvel/FIXP;
		int32_t quatSin = shittySin(halfAngle);
		iquat rvel_quat;
		range(i, 3) rvel_quat[i+1] = quatSin * rvel[i] / magRvel;
		rvel_quat[0] = sqrt(
			FIXP*FIXP
			- rvel_quat[1]*rvel_quat[1]
			- rvel_quat[2]*rvel_quat[2]
			- rvel_quat[3]*rvel_quat[3]
		);
		iquat tmp;
		iquat_mult(tmp, data->rvel, rvel_quat);
		memcpy(data->rvel, tmp, sizeof(iquat));
		iquat_norm(data->rvel);

		// Todo: if stuff is pre-computed, we'd have to re-compute it here.
	}

	collisions.destroy();
	toCheck.destroy();

	solidPutVb(&data->s, p, 1);
}

static char trans(gamestate *gs, void **ptr) {
	if (seriz_reading) {
		*ptr = malloc(sizeof(tskDynamicsData));
	}
	tskDynamicsData *data = (tskDynamicsData*)*ptr;

	transSolid(&data->s);

	transOffset(data->vel);
	transIquat(data->rvel);
	return 0;
}

static void copy(void **_to, void *_from) {
	tskDynamicsData *from = (tskDynamicsData*)_from;
	tskDynamicsData *to = (tskDynamicsData*)malloc(sizeof(tskDynamicsData));
	*_to = to;

	cpSolid(&to->s, &from->s);
	memcpy(to->vel, from->vel, sizeof(offset));
	memcpy(to->rvel, from->rvel, sizeof(iquat));
	//printf("[%d, %d)...\n", to->s.b->start, to->s.b->end);
}

static void destroy(void *_data) {
	tskDynamicsData *data = (tskDynamicsData*)_data;

	// Nope, velbox heirarchy is torn down by now.
	//velbox_remove(data->s.b);

	free(data);
}

void tskDynamics_create(gamestate *gs, buildCtx *c, offset const vel, iquat const rvel) {
	taskInstance &task = gs->tasks.add();
	task.defn = taskLookup(TSK_DYNAMICS);
	tskDynamicsData *data = (tskDynamicsData*)malloc(sizeof(tskDynamicsData));
	task.data = data;

	memcpy(data->s.m.pos, c->transf.pos, sizeof(offset));
	memcpy(data->s.m.rot, c->transf.rot, sizeof(iquat));
	data->s.m.type = 0; // Means "cube" I guess
	data->s.r = RADIUS_I_GUESS;
	data->s.tex = 8;

	memcpy(data->vel, vel, sizeof(offset));
	memcpy(data->rvel, rvel, sizeof(iquat));

	range(i, 3) data->s.m.oldPos[i] = data->s.m.pos[i] - data->vel[i];
	vb_now = gs->clock; // TODO oof
	solidPutVb(&data->s, gs->vb_root, 1);
	//printf("[%d, %d)", data->s.b->start, data->s.b->end);
	//printf("^ %d\n", gs->clock);
	// Unlike `oldPos`, this isn't used in `solidPutVb`,
	// so we don't really care what it is yet.
	memset(data->s.m.oldRot, 0, sizeof(iquat));
}

void defineTask_dynamics(taskDefn *d) {
	d->step = &step;
	d->trans = &trans;
	d->copy = &copy;
	d->destroy = &destroy;
}
