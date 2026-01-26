#include "gamestate.h"

#include "player.h"

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

	offset impulse;
	range(i, 3) impulse[i] = normalForce*forceDir[i]/FIXP;
	range(i, 3) p->vel[i] += impulse[i];

	// This is where "boring" object physics ends.
	// However, we're adding friction, and making it
	// really spicy to boot so people can move around

	unitvec desire = {p->inputs[0], p->inputs[1], 0};
	unitvec rotatedDesire;
	rotateInputDesire(rotatedDesire, desire, forceDir);
	bound26(rotatedDesire, FIXP);
	//printf("%7d, %7d, %ld\n", desire[0], rotatedDesire[0], normalForce);

	offset landSpeed;
	// This works out to be along the surface
	range(i, 3) landSpeed[i] = contactVel[i] + impulse[i];

	// Todo Could add friction here, would need to work out specifics.
	//      (so stopping is faster than starting)
#define SPEED 200
	bound64(landSpeed, SPEED);
	// Spatial types are usually in 64-bit integers,
	// but this one in particular is computed from
	// bounded inputs (so we know it fits in 32 bits (actually it fits in 10)).
	int32_t desiredChange[3];
	range(i, 3) desiredChange[i] = rotatedDesire[i]*SPEED/FIXP - landSpeed[i];
#undef SPEED
	int64_t traction = normalForce*2;
	if (traction < (1<<25)) { // `traction` fits in a signed 26-bit integer.
		bound26(desiredChange, traction);
	}
	//printf("%03d, %03d, %03d (%03ld)\r", desiredChange[0], desiredChange[1], desiredChange[2], normalForce);
	fflush(stdout);
	range(i, 3) p->vel[i] += desiredChange[i];
	range(i, 3) dest[i] += desiredChange[i]; // Hopefully prevents gradual slipping?
}
