#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "queue.h"

#include "main.h"
#include "graphics.h"
#include "gamestate.h"
#include "random.h"

#include "game.h"
#include "game_callbacks.h"

int zoomLvl = 0;

static char debugPrint = 0;

void game_init() {
	initGraphics();
}

gamestate* game_init2() {
	timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	uint32_t seed = now.tv_sec;

	gamestate *tmp = (gamestate*)malloc(sizeof(gamestate));
	range(i, boardAreaChunks) {
		mapChunk *mc = (mapChunk*)malloc(sizeof(mapChunk));
		tmp->board[i] = mc;
		mc->refs = 1;
		range(j, chunkAreaSpaces) {
			uint32_t rand = splitmix32(&seed);
			// This is kind of goofy, but I'm having a good time okay?
			mc->data[j] = (rand % 4) % 3;
		}
	}

	tmp->players.init();

	return tmp;
}

void game_destroy2() {}
void game_destroy() {}

void handleKey(int key, int action) {}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
	/*
	int x = xpos;
	int y = ypos;
	if (mouseDown) {
		float dx = x - mouseX;
		float dy = y - mouseY;
		float dist = sqrt(dx*dx + dy*dy);
		float radians = dist * 0.002;
		// Because of quaternion math, this represents a rotation of `radians*2`.
		// We cap it at under a half rotation, after which I'm not sure my
		// quaternions keep behaving haha
		if (radians > 1.5) radians = 1.5;
		float s = sin(radians);
		// Positive X -> mouse to the right -> rotate about +Z
		// Positive Y -> mouse down -> rotate around +X

		quat r = {cos(radians), s*dy/dist, 0, s*dx/dist};
		quat_rotateBy(tmpGameRotation, r);
		quat_norm(tmpGameRotation);
	}
	mouseX = x;
	mouseY = y;
	*/
}
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	//if (button == 0) mouseDown = (action == GLFW_PRESS);
	if (action == GLFW_PRESS) debugPrint = 1;
}
void scroll_callback(GLFWwindow *window, double x, double y) {
	// We don't handle any fancy non-integer scrolling, sorry
	zoomLvl += y;
}
void window_focus_callback(GLFWwindow *window, int focused) {}
void copyInputs() {}
int getInputsSize() { return 0; }
void serializeInputs(char * dest) {}
int playerInputs(player *p, list<char> const * data) {
	// We used 0 bytes!
	return 0;
}


//// Text command stuff ////

char handleLocalCommand(char * buf, list<char> * outData) {
	return 0;
}

char customLoopbackCommand(gamestate *gs, char const * str) {
	return 0;
}

char processBinCmd(gamestate *gs, player *p, char const *data, int chars, char isMe, char isReal) {
	return 0;
}

char processTxtCmd(gamestate *gs, player *p, char *str, char isMe, char isReal) {
	if (isCmd(str, "/state")) {
		/* TODO cleanup
		int32_t s;
		char const *input = str+6;
		if (getNum(&input, &s)) {
			gs->nonsense = s;
			printf("State set to %d (%hhd)\n", s, isReal);
		} else if (isReal && isMe) {
			printf("State is %d\n", gs->nonsense);
		}
		*/
		return 1;
	} else {
		if (isReal) puts(str);
		return 0;
	}
}

void prefsToCmds(queue<strbuf> *cmds) {
	// No prefs that need to be sent for now
}

//// graphics stuff! ////

void draw(gamestate *gs, float interpRatio, long drawingNanos, long totalNanos) {
	setupFrame();

	// Okay, first we need to determine where the edges of the screen lie in world coordinates.
	// Spaces are 16 world units wide, at least for now

	// Later on these will follow the player I guess.
	// For now this should be the center of the board?
	int centerX = 80*16;
	int centerY = 80*16;

	int worldRadiusX = 1.0/scaleX;
	int worldRadiusY = 1.0/scaleY;
	int leftChunk  = (centerX-worldRadiusX)/(chunkSizeSpaces*16);
	int rightChunk = (centerX+worldRadiusX)/(chunkSizeSpaces*16) + 1;
	int floorChunk = (centerY-worldRadiusY)/(chunkSizeSpaces*16);
	int cielChunk  = (centerY+worldRadiusY)/(chunkSizeSpaces*16) + 1;
	if (leftChunk < 0) leftChunk = 0;
	if (floorChunk < 0) floorChunk = 0;
	if (rightChunk > boardSizeChunks) rightChunk = boardSizeChunks;
	if (cielChunk > boardSizeChunks) cielChunk = boardSizeChunks;

	int written = 0;

	for (int cy = floorChunk; cy < cielChunk; cy++) {
		for (int cx = leftChunk; cx < rightChunk; cx++) {
			char *data = gs->board[cy*boardSizeChunks+cx]->data;
			int xBase = cx*chunkSizeSpaces*16 - centerX;
			int yBase = cy*chunkSizeSpaces*16 - centerY;
			for (int y = 0; y < chunkSizeSpaces; y++) {
				for (int x = 0; x < chunkSizeSpaces; x++) {
					char space = data[y*chunkSizeSpaces+x];
					if (!space) continue;
					written++;
					drawSprite(
						xBase + 16*x,
						yBase + 16*y,
						space
					);
				}
			}
		}
	}

	if (debugPrint) {
		debugPrint = 0;
		printf("%d,%d  %d,%d: %d\n", leftChunk, rightChunk, floorChunk, cielChunk, written);
	}
}
