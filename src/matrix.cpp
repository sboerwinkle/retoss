/////////////////////////////////
// Authors: mboerwinkle, sboerwinkle
/////////////////////////////////

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

#include "matrix.h"

void vec_norm(unitvec v) {
	// See comments about integer sqrt in `iquat_norm` (which was written first)
	int32_t len = sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
	v[0] = v[0]*FIXP/len;
	v[1] = v[1]*FIXP/len;
	v[2] = v[2]*FIXP/len;
}

void quat_norm(quat t){
	// If this `sqrt` call is too expensive for you, you could
	// replace it with an iteration of Newton's approximation.
	// You can't just remove it, however, or repeated calls to
	// the function won't actually converge properly.
	float len = sqrt(t[0]*t[0]+t[1]*t[1]+t[2]*t[2]+t[3]*t[3]);
	t[0]/=len;
	t[1]/=len;
	t[2]/=len;
	t[3]/=len;
}
void iquat_norm(iquat x) {
	// Assuming the `iquat` is approximately normal, the result should be
	// roughly 1*1, which fits.
	// We do rely on `sqrt` being reproducible across systems (to within an integer),
	// which seems like a reasonable assumption.
	int32_t len = sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]+x[3]*x[3]);
	x[0] = x[0]*FIXP/len;
	x[1] = x[1]*FIXP/len;
	x[2] = x[2]*FIXP/len;
	x[3] = x[3]*FIXP/len;
}
void quat_rotateBy(quat x, quat rot){
	quat t;
	quat_mult(t, x, rot);
	memcpy(x, t, sizeof(t));
}
void iquat_rotateBy(iquat x, iquat const rot) {
	iquat t;
	iquat_mult(t, x, rot);
	memcpy(x, t, sizeof(t));
}

// These are all busted, need to re-evaluate what I'm expecting to do with `strt`.
void quat_rotX(quat ret, quat strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), sin(r), 0, 0};
	quat_rotateBy(ret, tmp);
}
void quat_rotY(quat ret, quat strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), 0, sin(r), 0};
	quat_rotateBy(ret, tmp);
}
void quat_rotZ(quat ret, quat strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), 0, 0, sin(r)};
	quat_rotateBy(ret, tmp);
}

// We do our quaternion multiplications backwards from what I think is convention.
// It makes sense for me that if I have a quaternion that converts vectors from
// camera rotation to world rotation, and one that converts world rotation to item rotation,
// then `camera->world` * `world->item` => `camera->item`.
// These feels like the more natural way to compose rotations.
// This is why args `a` and `b` are flipped.
void quat_mult(quat ret, quat b, quat a){
	ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3]);
	ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2]);
	ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1]);
	ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0]);
}

void iquat_mult(iquat ret, iquat const b, iquat const a){
	ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3])/FIXP;
	ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2])/FIXP;
	ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1])/FIXP;
	ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0])/FIXP;
}

void quat_apply(float dest[3], quat q, float src[3]) {
	float x2sqj = src[0] * 2 * q[2];
	float x2sqk = src[0] * 2 * q[3];
	float y2sqk = src[1] * 2 * q[3];
	float y2sqi = src[1] * 2 * q[1];
	float z2sqi = src[2] * 2 * q[1];
	float z2sqj = src[2] * 2 * q[2];
	dest[0] = src[0] - x2sqj*q[2] - x2sqk*q[3]
		         - y2sqk*q[0] + y2sqi*q[2]
		         + z2sqi*q[3] + z2sqj*q[0];
	dest[1] = src[1] - y2sqk*q[3] - y2sqi*q[1]
		         - z2sqi*q[0] + z2sqj*q[3]
		         + x2sqj*q[1] + x2sqk*q[0];
	dest[2] = src[2] - z2sqi*q[1] - z2sqj*q[2]
		         - x2sqj*q[0] + x2sqk*q[1]
		         + y2sqk*q[2] + y2sqi*q[0];
}

// Our input components should all be in [-1,1].
// Our FIXP type can hold double-precision (i.e. multiplied)
// values roughly in the range of (-2,2), but we try to
// restrict it to [-1,1] since we want there to be some leeway.
//
// Honestly I'm not sure how often this will get used;
// if we've got multiple faces to check on a single solid,
// I think it quickly becomes better to convert the quat to a matrix.
void iquat_apply(unitvec dest, iquat q, unitvec const src) {
	// The `*2` we leave out, and apply it (sneaky like)
	// when we do the conversion back to single-precision later.
	int32_t xsqj = src[0] * q[2] / FIXP;
	int32_t xsqk = src[0] * q[3] / FIXP;
	int32_t ysqk = src[1] * q[3] / FIXP;
	int32_t ysqi = src[1] * q[1] / FIXP;
	int32_t zsqi = src[2] * q[1] / FIXP;
	int32_t zsqj = src[2] * q[2] / FIXP;
	dest[0] = src[0] + ( - xsqj*q[2] - xsqk*q[3]
		             - ysqk*q[0] + ysqi*q[2]
		             + zsqi*q[3] + zsqj*q[0] ) / (FIXP/2);
	dest[1] = src[1] + ( - ysqk*q[3] - ysqi*q[1]
		             - zsqi*q[0] + zsqj*q[3]
		             + xsqj*q[1] + xsqk*q[0] ) / (FIXP/2);
	dest[2] = src[2] + ( - zsqi*q[1] - zsqj*q[2]
		             - xsqj*q[0] + xsqk*q[1]
		             + ysqk*q[2] + ysqi*q[0] ) / (FIXP/2);
}

// Exactly the same as `iquat_apply`, but works on bigger types.
// The "Sm" suffix is because it won't work if the offsets are too big lol
void iquat_applySm(offset dest, iquat q, offset const src) {
	int64_t xsqj = src[0] * q[2] / FIXP;
	int64_t xsqk = src[0] * q[3] / FIXP;
	int64_t ysqk = src[1] * q[3] / FIXP;
	int64_t ysqi = src[1] * q[1] / FIXP;
	int64_t zsqi = src[2] * q[1] / FIXP;
	int64_t zsqj = src[2] * q[2] / FIXP;
	dest[0] = src[0] + ( - xsqj*q[2] - xsqk*q[3]
		             - ysqk*q[0] + ysqi*q[2]
		             + zsqi*q[3] + zsqj*q[0] ) / (FIXP/2);
	dest[1] = src[1] + ( - ysqk*q[3] - ysqi*q[1]
		             - zsqi*q[0] + zsqj*q[3]
		             + xsqj*q[1] + xsqk*q[0] ) / (FIXP/2);
	dest[2] = src[2] + ( - zsqi*q[1] - zsqj*q[2]
		             - xsqj*q[0] + xsqk*q[1]
		             + ysqk*q[2] + ysqi*q[0] ) / (FIXP/2);
}

void iquat_inv(iquat dest, iquat src) {
	dest[0] = src[0];
	dest[1] = -src[1];
	dest[2] = -src[2];
	dest[3] = -src[3];
}

void quat_print(quat t){
	printf("%.2f %.2f %.2f %.2f\n", t[0],t[1],t[2],t[3]);
}

void mat4Multf(float* res, float* m1, float* m2){
	for(int x = 0; x < 4; x++){
		for(int y = 0; y < 4; y++){
			float v = 0;
			for(int i = 0; i < 4; i++){
				v += m1[y+4*i]*m2[i+4*x];
			}
			res[y+4*x] = v;
		}
	}
}

void mat3FromQuat(float* M, quat rot){
	M[0] = 1-2*rot[2]*rot[2]-2*rot[3]*rot[3];
	M[1] = 2*rot[1]*rot[2]+2*rot[0]*rot[3];
	M[2] = 2*rot[1]*rot[3]-2*rot[0]*rot[2];

	M[3] = 2*rot[1]*rot[2]-2*rot[0]*rot[3];
	M[4] = 1-2*rot[1]*rot[1]-2*rot[3]*rot[3];
	M[5] = 2*rot[2]*rot[3]+2*rot[0]*rot[1];

	M[6] = 2*rot[1]*rot[3]+2*rot[0]*rot[2];
	M[7] = 2*rot[2]*rot[3]-2*rot[0]*rot[1];
	M[8] = 1-2*rot[1]*rot[1]-2*rot[2]*rot[2];
}

void mat4FromQuat(float* M, quat rot){
	M[ 0] = 1-2*rot[2]*rot[2]-2*rot[3]*rot[3];
	M[ 1] =   2*rot[1]*rot[2]+2*rot[0]*rot[3];
	M[ 2] =   2*rot[1]*rot[3]-2*rot[0]*rot[2];
	M[ 3] = 0;
	M[ 4] =   2*rot[1]*rot[2]-2*rot[0]*rot[3];
	M[ 5] = 1-2*rot[1]*rot[1]-2*rot[3]*rot[3];
	M[ 6] =   2*rot[2]*rot[3]+2*rot[0]*rot[1];
	M[ 7] = 0;
	M[ 8] =   2*rot[1]*rot[3]+2*rot[0]*rot[2];
	M[ 9] =   2*rot[2]*rot[3]-2*rot[0]*rot[1];
	M[10] = 1-2*rot[1]*rot[1]-2*rot[2]*rot[2];
	M[11] = 0;
	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;
}

void mat3FromIquat(float *M, iquat rot) {
	// All the operations on components of `rot`
	// involve a `*2`, so we just include that in
	// the calculation of our fixed-point divisor.
	float divisor = FIXP/2*FIXP;
	M[0] = 1-(rot[2]*rot[2]+rot[3]*rot[3])/divisor;
	M[1] =   (rot[1]*rot[2]+rot[0]*rot[3])/divisor;
	M[2] =   (rot[1]*rot[3]-rot[0]*rot[2])/divisor;

	M[3] =   (rot[1]*rot[2]-rot[0]*rot[3])/divisor;
	M[4] = 1-(rot[1]*rot[1]+rot[3]*rot[3])/divisor;
	M[5] =   (rot[2]*rot[3]+rot[0]*rot[1])/divisor;

	M[6] =   (rot[1]*rot[3]+rot[0]*rot[2])/divisor;
	M[7] =   (rot[2]*rot[3]-rot[0]*rot[1])/divisor;
	M[8] = 1-(rot[1]*rot[1]+rot[2]*rot[2])/divisor;
}

// Produces a matrix that represents the reverse rotation of `rot`
void imatFromIquatInv(imat M, iquat rot) {
	// All the operations on components of `rot`
	// involve a `*2`, so we just include that in
	// the calculation of our fixed-point divisor.
	int32_t const divisor = FIXP/2;

	M[0] = FIXP-(rot[2]*rot[2]+rot[3]*rot[3])/divisor;
	M[1] =      (rot[1]*rot[2]-rot[0]*rot[3])/divisor;
	M[2] =      (rot[1]*rot[3]+rot[0]*rot[2])/divisor;

	M[3] =      (rot[1]*rot[2]+rot[0]*rot[3])/divisor;
	M[4] = FIXP-(rot[1]*rot[1]+rot[3]*rot[3])/divisor;
	M[5] =      (rot[2]*rot[3]-rot[0]*rot[1])/divisor;

	M[6] =      (rot[1]*rot[3]-rot[0]*rot[2])/divisor;
	M[7] =      (rot[2]*rot[3]+rot[0]*rot[1])/divisor;
	M[8] = FIXP-(rot[1]*rot[1]+rot[2]*rot[2])/divisor;
}

// 'Sm' is because we assume the input offset is small enough (approx. 500 Jupiters across at 1000 units/m)
// that the multiplications can be performed safely within int64_t
void imat_applySm(offset dest, imat const rot, offset const src) {
	dest[0] = (src[0]*rot[0] + src[1]*rot[3] + src[2]*rot[6])/FIXP;
	dest[1] = (src[0]*rot[1] + src[1]*rot[4] + src[2]*rot[7])/FIXP;
	dest[2] = (src[0]*rot[2] + src[1]*rot[5] + src[2]*rot[8])/FIXP;
}
void imat_apply(unitvec dest, imat const rot, unitvec const src) {
	dest[0] = (src[0]*rot[0] + src[1]*rot[3] + src[2]*rot[6])/FIXP;
	dest[1] = (src[0]*rot[1] + src[1]*rot[4] + src[2]*rot[7])/FIXP;
	dest[2] = (src[0]*rot[2] + src[1]*rot[5] + src[2]*rot[8])/FIXP;
}
void imat_flipRot(imat rot) {
	int32_t tmp;
	tmp = rot[1];
	rot[1] = rot[3];
	rot[3] = tmp;

	tmp = rot[2];
	rot[2] = rot[6];
	rot[6] = tmp;

	tmp = rot[5];
	rot[5] = rot[7];
	rot[7] = tmp;
}

void imatFromIquat(int32_t *M, iquat rot) {
	// All the operations on components of `rot`
	// involve a `*2`, so we just include that in
	// the calculation of our fixed-point divisor.
	int32_t divisor = FIXP/2;
	M[0] = FIXP-(rot[2]*rot[2]+rot[3]*rot[3])/divisor;
	M[1] =      (rot[1]*rot[2]+rot[0]*rot[3])/divisor;
	M[2] =      (rot[1]*rot[3]-rot[0]*rot[2])/divisor;

	M[3] =      (rot[1]*rot[2]-rot[0]*rot[3])/divisor;
	M[4] = FIXP-(rot[1]*rot[1]+rot[3]*rot[3])/divisor;
	M[5] =      (rot[2]*rot[3]+rot[0]*rot[1])/divisor;

	M[6] =      (rot[1]*rot[3]+rot[0]*rot[2])/divisor;
	M[7] =      (rot[2]*rot[3]-rot[0]*rot[1])/divisor;
	M[8] = FIXP-(rot[1]*rot[1]+rot[2]*rot[2])/divisor;
}

void matEmbiggen(float M[16], float in[9], float x, float y, float z) {
	M[ 0] = in[0];
	M[ 1] = in[1];
	M[ 2] = in[2];
	M[ 3] = 0;
	M[ 4] = in[3];
	M[ 5] = in[4];
	M[ 6] = in[5];
	M[ 7] = 0;
	M[ 8] = in[6];
	M[ 9] = in[7];
	M[10] = in[8];
	M[11] = 0;
	M[12] = x;
	M[13] = y;
	M[14] = z;
	M[15] = 1;
}

void mat4Transf(float* res, float x, float y, float z){
	res[12] += x;
	res[13] += y;
	res[14] += z;
}

//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
// Modified for infinite zFar, since the precision losses are finite and I really like
// really big zFar anyway.
// Layout is X_gives, Y_gives, Z_gives, W_gives
// (i.e. each group of 4 corresponds to one _input_ component)
void perspective(float* m, float invSlopeX, float invSlopeY, float zNear) {
	float const tmp_debug_offset = 0;
	m[ 0] =  invSlopeX; 
	m[ 1] =  0;
	m[ 2] =  0;
	m[ 3] =  0;

	m[ 4] =  0;
	m[ 5] =  0;
	m[ 6] =  1;
	m[ 7] =  1;

	m[ 8] =  0;
	m[ 9] =  invSlopeY;
	m[10] =  0;
	m[11] =  0;

	m[12] =  0;
	m[13] =  0;
	m[14] = -2*zNear + tmp_debug_offset;
	m[15] =  tmp_debug_offset;
}

// Assumes `o` isn't, like, super-duper big
int64_t dot(offset const o, unitvec const v) {
	return (o[0]*v[0] + o[1]*v[1] + o[2]*v[2])/FIXP;
}

int32_t dot(unitvec const a, unitvec const b) {
	return (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])/FIXP;
}

void cross(unitvec output, unitvec const a, unitvec const b) {
	output[0] = (a[1]*b[2]-b[1]*a[2])/FIXP;
	output[1] = (a[2]*b[0]-b[2]*a[0])/FIXP;
	output[2] = (a[0]*b[1]-b[0]*a[1])/FIXP;
}

// `bound` should fit in (I believe) 25 bits?
// If you need a bigger bound, use a different function (presumably that pre-scales stuff?).
// If you're not sure on the bound's size, write both functions and then a wrapper to choose one.
void bound64(offset v, int32_t bound) {
	int64_t magEst = labs(v[0]) + labs(v[1]) + labs(v[2]);
	// True magnitude will be between `magEst` and `magEst/2` (actually magEst/1.732)
	if (magEst <= bound) return;
	int64_t d;
	if (magEst < bound*2) {
		// At this point we're still not sure if it needs truncation or not, so we have to check.
		// However, it should be small enough that we can do the sqrt safely.
		int64_t d_sq = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
		d = sqrt(d_sq);
		if (d <= bound) return;
	} else {
		// We lose precision here, but I'm not too torn up about it.
		// We assume all offsets can be safely multiplied by FIXP (without overflow),
		// so we use that to get some scaling.
		range(i,3) v[i] = v[i]*FIXP/magEst;
		int64_t d_sq = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
		d = sqrt(d_sq);
		// No "are we within bounds" check:
		// We just scaled the vector (so it tells us nothing),
		// plus we already know we're out-of-bounds just from `magEst`.
	}
	// Either way we got here, we know `v[i]*bound` fits in 64 bits.
	range(i, 3) v[i] = v[i]*bound/d;
}

// Some conservative back-of-napkin math means a 26-bit signed number
// can be squared and still represented exactly by a `double`.
// I'm going to assume that this means we have consistent `sqrt` across
// platforms up to that point.
void bound26(int32_t v[3], int32_t bound) {
	int64_t d_sq = (int64_t)v[0]*v[0] + (int64_t)v[1]*v[1] + (int64_t)v[2]*v[2];
	if (d_sq <= (int64_t)bound*bound) return;
	int64_t d = sqrt(d_sq);
	range(i, 3) v[i] = v[i]*bound/d;
}
