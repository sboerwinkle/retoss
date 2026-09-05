#include <stdio.h>

#include "../util.h"

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"

#include "../tasks/killPlane.h"
#include "../tasks/tdmScore.h"

static char *map;
static int w, h;

static void decompose(int32_t v, int *x, int *y) {
	*x = (int)(int16_t)v;
	*y = v>>16;
	if (*x < 0) (*y)++;
}

static int32_t compose(int x, int y) {
	return ((int32_t)y<<16) + (int32_t)x;
}

static char check(int32_t v) {
	int x, y;
	decompose(v, &x, &y);
	if (x < 0 || y < 0 || x >= w || y >= h) return -1;
	return map[y*w + x];
}

struct region {
	// 4 corners
	int32_t c0, c1, c2, c3;
};

static void _grow(region *r, int32_t v1, int32_t v2, char targ) {
	int32_t nc1 = r->c1;
	int32_t nc2 = r->c2;
	int32_t nc3 = r->c3;

	while (1) {
		nc1 += v1;
		nc2 += v1 + v2;
		nc3 += v2;
		{
			int x, y;
			decompose(nc2, &x, &y);
			if ((x < 0 || x >= w) && (y < 0 || y >= h)) return;
		}

		// We're growing along 2 edges, but we do add one tile to each of
		// the *other* 2 edges each expansion. Need to check them to make
		// sure we don't expose any flickery walls.
		if (check(nc1) != targ && (check(nc1-v2)&0x7F) < targ) return;
		if (check(nc3) != targ && (check(nc3-v1)&0x7F) < targ) return;

		int32_t crsr = nc1;
		char benefit = 0;
		while (1) {
			char c = check(crsr);
			// Check if it generates an exposed flickery edge
			if (c != targ && (check(crsr+v1) & 0x7F) < targ) return;
			// Check if it would make this tile too tall
			if ((c & 0x7F) < targ) return;
			if ((c & 0x7F) == targ) {
				// Flickery top
				if (c < 0) return;
				// Else, we're an exact height match and this tile
				// isn't already handled, so we've helped someone.
				benefit = 1;
			}
			if (crsr == nc2) break;
			crsr += v2;
		}
		crsr = nc3;
		while (1) {
			char c = check(crsr);
			if (c != targ && (check(crsr+v2) & 0x7F) < targ) return;
			if ((c & 0x7F) < targ) return;
			if ((c & 0x7F) == targ) {
				if (c < 0) return;
				benefit = 1;
			}
			if (crsr == nc2) break;
			crsr += v1;
		}
		if (benefit) {
			// Go back through and mark "benefit" tiles as resolved
			crsr = nc1;
			while (1) {
				char c = check(crsr);
				if (c == targ) {
					int x, y;
					decompose(crsr, &x, &y);
					map[y*w + x] |= 0x80;
				}
				if (crsr == nc2) break;
				crsr += v2;
			}
			crsr = nc3;
			while (1) {
				char c = check(crsr);
				if (c == targ) {
					int x, y;
					decompose(crsr, &x, &y);
					map[y*w + x] |= 0x80;
				}
				if (crsr == nc2) break;
				crsr += v1;
			}
			r->c1 = nc1;
			r->c2 = nc2;
			r->c3 = nc3;
		}
	}
}

static void rot(int32_t *v) {
	int x, y;
	decompose(*v, &x, &y);
	*v = compose(-y, x);
}

static void rotate(region *r, int32_t *v1, int32_t *v2) {
	int32_t tmp = r->c0;
	r->c0 = r->c1;
	r->c1 = r->c2;
	r->c2 = r->c3;
	r->c3 = tmp;

	rot(v1);
	rot(v2);
}

static void grow(region *r, int32_t x, int32_t y, char targ) {
	r->c0 = r->c1 = r->c2 = r->c3 = compose(x, y);

	int32_t v1 = compose(1, 0);
	int32_t v2 = compose(0, 1);
	range(i, 4) {
		_grow(r, v1, v2, targ);
		rotate(r, &v1, &v2);
	}
}

void lv_heightmap(gamestate *gs, char const *data, int len) {
	w = h = 0;
	map = NULL;

	bctx.reset(gs);
	prepareGamestateForLoad(gs, 0);
	coreSetup(gs);

	taskKillPlane_create(gs, -20000);
	tskTdmData *tdmData = taskTdm_create(gs, 5);

	char line[80];
	int ix = 0;
	while (1) {
		if (ix >= len || ix >= 80) {
			puts("/hmap: First line not terminated, or too long");
			return;
		}
		if (data[ix] == '\n') {
			memcpy(line, data, ix);
			line[ix] = '\0';
			ix++;
			break;
		}
		ix++;
	}
	int mapStartIx = ix;
	char const *cursor = line;
	int32_t gridSize, ceilHeight;
	if (!getNum(&cursor, &gridSize) || !getNum(&cursor, &ceilHeight)) {
		puts("/hmap: First line must contain block size and ceiling height");
	}
	int floorTex = 8, wallTex = 5, ceilTex = 8;
	getNum(&cursor, &floorTex) && getNum(&cursor, &wallTex) && getNum(&cursor, &ceilTex);

	bctx.push();

	int mapX = 0, mapY = 0;
	for (; ix < len; ix++) {
		if (data[ix] == '\n') {
			if (mapX > w) w = mapX;
			mapX = 0;
			mapY++;
			continue;
		}
		char c = data[ix];
		if (c >= 'A' && c <= 'Z') {
			c -= 'A';
			int height = ceilHeight * c / 25;
			// Coordinates need to be at center of block
			int64_t x = gridSize*mapX + gridSize/2;
			int64_t y = -gridSize*mapY - gridSize/2;

			bctx.pos(x, y, height + PLAYER_SHAPE_RADIUS);
			bctx.finalizeTranslate();
			taskTdm_addSpawn(tdmData, bctx.transf.pos);
			bctx.peek();
		}

		mapX++;
	}
	h = mapY;

	// Make a 2D array so we can get fancy.
	map = (char*)malloc(w*h);
	// Iterate again to add stuff to our map
	mapX = 0;
	mapY = 0;
	for (ix = mapStartIx; ix < len; ix++) {
		char c = data[ix];
		if (c == '\n') {
			while (mapX < w) {
				map[mapY*w+mapX] = -1;
				mapX++;
			}
			mapY++;
			mapX = 0;
			continue;
		}
		if (c == ' ') c = 'a';
		if (c >= 'a' && c <= 'z') {
			c -= 'a';
		} else if (c >= 'A' && c <= 'Z') {
			c -= 'A';
		} else {
			printf("Bad /hmap char: %c\n", c);
			c = 0;
		}
		map[mapY*w + mapX] = c;

		mapX++;
	}

	for (mapY = 0; mapY < h; mapY++) {
		for (mapX = 0; mapX < w; mapX++) {
			char c = map[mapY*w + mapX];
			// Skip negative things b/c they're resolved,
			// and skip 0 because we don't need floor there.
			if (c <= 0) continue;
			map[mapY*w + mapX] |= 0x80; // Set high bit, it's now negative

			region r;
			grow(&r, mapX, mapY, c);

			int height = ceilHeight * c / 25;
			int size = gridSize * (r.c1 - r.c0 + 1);
			int32_t m2 = r.c0 + r.c2;
			int m2x, m2y;
			decompose(m2, &m2x, &m2y);
			// Coordinates need to be at center of block
			int64_t x = gridSize/2 * (1+m2x);
			int64_t y = gridSize/2 * -(1+m2y);

			if (height > size) {
				// If it's "too tall", fill this space with a pillar standing on end.
				// Proportions are 1x1x8. Horizontal dimension is `size`, so vertical
				// dimension will be `size*8`, and we need to offset by half of that.
				bctx.pos(x, y, height - size*4);
				// I've got a messed up system for recording rotations without floating point
				// numbers, may clean it up later. This is 90 degrees around one axis.
				bctx.rot((int32_t const[]){0, 23170, 0});
				// shape=2 (pillar), tex=5 (brick)
				bctx.add(2, wallTex, size*4);
				bctx.peek();
			} else if (height) {
				// Regular case, spawn a cube instead of a pillar.
				bctx.pos(x, y, height - size/2);
				bctx.add(0, wallTex, size/2);
				bctx.peek();
			}
		}
	}

	// Make floor and ceiling
	if (w < h) w = h;
	int64_t floorR = gridSize*w/2;
	int height = floorR/8;
	bctx.pos(floorR, -floorR, -height);
	bctx.add(1, floorTex, floorR);
	if (ceilTex != -1) {
		bctx.pos(0, 0, height*2 + ceilHeight);
		bctx.add(1, ceilTex, floorR);
	}

	free(map);
	taskTdm_spawnAll(gs, tdmData);
}
