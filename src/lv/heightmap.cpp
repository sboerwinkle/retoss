#include <stdio.h>

#include "../util.h"

#include "../gamestate.h"
#include "../bctx.h"
#include "../dl_helpers.h"

#include "../tasks/killPlane.h"
#include "../tasks/tdmScore.h"

void lv_heightmap(gamestate *gs, char const *data, int len) {
	bctx.reset(gs);
	prepareGamestateForLoad(gs, 0);
	coreSetup(gs);

	taskKillPlane_create(gs, -20000);
	tskTdmData *tdmData = taskTdm_create(gs, 5);

	char line[80];
	int idx = 0;
	while (1) {
		if (idx >= len || idx >= 80) {
			puts("/hmap: First line not terminated, or too long");
			return;
		}
		if (data[idx] == '\n') {
			memcpy(line, data, idx);
			line[idx] = '\0';
			idx++;
			break;
		}
		idx++;
	}
	char const *cursor = line;
	int32_t blockSize, ceilHeight;
	if (!getNum(&cursor, &blockSize) || !getNum(&cursor, &ceilHeight)) {
		puts("/hmap: First line must contain block size and ceiling height");
	}

	bctx.push();

	int w = 0, h = 0;
	int currentWidth = 0;
	for (; idx < len; idx++) {
		if (data[idx] == '\n') {
			if (currentWidth > w) w = currentWidth;
			currentWidth = 0;
			h++;
			continue;
		}
		char c = data[idx];
		char spawn;
		if (c == ' ') c = 'a';
		if (c >= 'a' && c <= 'z') {
			c -= 'a';
			spawn = 0;
		} else if (c >= 'A' && c <= 'Z') {
			c -= 'A';
			spawn = 1;
		} else {
			putchar(c);
			continue;
		}
		int height = ceilHeight * c / 25;
		// Coordinates need to be at center of block
		int64_t x = blockSize*currentWidth + blockSize/2;
		int64_t y = -blockSize*h - blockSize/2;

		if (height > blockSize) {
			// If it's "too tall", fill this space with a pillar standing on end.
			// Dimensions are 1x1x8, and we need to halve the height (to get block center),
			// so it works out to `blockSize*4`.
			bctx.pos(x, y, height - blockSize*4);
			// I've got a messed up system for recording rotations without floating point
			// numbers, may clean it up later. This is 90 degrees.
			bctx.rot((int32_t const[]){0, 23170, 0});
			// shape=2 (pillar), tex=5 (brick)
			bctx.add(2, 5, blockSize*4);
			bctx.peek();
		} else if (height) {
			// Regular case, spawn a cube instead of a pillar.
			bctx.pos(x, y, height - blockSize/2);
			bctx.add(0, 5, blockSize/2);
			bctx.peek();
		}

		if (spawn) {
			bctx.pos(x, y, height + PLAYER_SHAPE_RADIUS);
			bctx.finalizeTranslate();
			taskTdm_addSpawn(tdmData, bctx.transf.pos);
			bctx.peek();
		}

		currentWidth++;
	}

	// Make floor and ceiling
	if (w < h) w = h;
	int64_t floorR = blockSize*w/2;
	int height = floorR/8;
	bctx.pos(floorR, -floorR, -height);
	bctx.add(1, 8, floorR);
	bctx.pos(0, 0, height*2 + ceilHeight);
	bctx.add(1, 8, floorR);

	taskTdm_spawnAll(gs, tdmData);
}
