#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "queue.h"

#include "main.h"
#include "graphics.h"
#include "gamestate.h"

#include "game.h"
#include "game_callbacks.h"

int zoomLvl = 0;

static char debugPrint = 0;

struct {
	struct {
		char l, r, d;
	} dir;
	struct {
		char z, x, c, v;
	} cmd;
} activeInputs = {}, sharedInputs = {};

void game_init() {
	initGraphics();
	gamestate_init();
}

// TODO gamestate init logic should be the responsibility of gamestate.cpp.
//      `game_init2` can be responsible for level gen if it wants to,
//      but not data integrity.
gamestate* game_init2() {
	timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	uint32_t seed = now.tv_sec;

	gamestate *tmp = (gamestate*)malloc(sizeof(gamestate));
	init(tmp);
	shuffle(tmp, seed);

	return tmp;
}

void game_destroy2() {}
void game_destroy() {
	gamestate_destroy();
}

void handleKey(int key, int action) {
	// I can't imagine a scenario where I actually need to know about repeat events
	if (action == GLFW_REPEAT) return;

	     if (key == GLFW_KEY_RIGHT) activeInputs.dir.r = action;
	else if (key == GLFW_KEY_LEFT) activeInputs.dir.l = action;
	else if (key == GLFW_KEY_DOWN) activeInputs.dir.d = action;
	else if (!action) return;
	else if (key == GLFW_KEY_Z) activeInputs.cmd.z = 1;
	else if (key == GLFW_KEY_X) activeInputs.cmd.x = 1;
	else if (key == GLFW_KEY_C) activeInputs.cmd.c = 1;
	else if (key == GLFW_KEY_V) activeInputs.cmd.v = 1;
}

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

void copyInputs() {
	// Realisitcally I could get away with doing less here,
	// especially if I didn't have `sharedInputs.cmd`.
	// The basic movement stuff doesn't require the thread that
	// serializes outputs (the "game" thread, I believe) to do
	// writes, and since we're looking at individual chars,
	// unsynchronized reads to the input thread's data would be fine.
	// However, `sharedInputs.cmd` requires some minimal writing,
	// at which point it's cleaner just to do things correctly,
	// under mutex lock. (Which I have this very nice framework
	// to organize, so that's basically free anyway!)
	sharedInputs.dir = activeInputs.dir;
	sharedInputs.cmd.z |= activeInputs.cmd.z;
	sharedInputs.cmd.x |= activeInputs.cmd.x;
	sharedInputs.cmd.c |= activeInputs.cmd.c;
	sharedInputs.cmd.v |= activeInputs.cmd.v;
	activeInputs.cmd = {};
}

// In theory I think this would let us have a dynamic size of input data per-frame.
// In practice we don't use that, partly because anything that *might* winds up
// being better implemented as a command, which ensures delivery (even if late,
// or stacked up with commands from other frames).
int getInputsSize() { return 1; }
void serializeInputs(char * dest) {
	if (sharedInputs.dir.d) {
		*dest = 2;
	} else {
		*dest = sharedInputs.dir.r - sharedInputs.dir.l;
	}
	// Each item in outboundTextQueue can be TEXT_BUF_LEN chars long,
	// and that's like 200. These short, static strings are fine.
	if (sharedInputs.cmd.c) {
		strcpy(outboundTextQueue.add().items, "/_c");
	}
	// Order's shuffled from keyboard layout b/c it affects the order
	// commands are processed in (if a player issues 2 in the same frame)
	if (sharedInputs.cmd.z) {
		strcpy(outboundTextQueue.add().items, "/_z");
	}
	if (sharedInputs.cmd.x) {
		strcpy(outboundTextQueue.add().items, "/_x");
	}
	if (sharedInputs.cmd.v) {
		strcpy(outboundTextQueue.add().items, "/_v");
	}
	sharedInputs.cmd = {};
}
int playerInputs(player *p, list<char> const * data) {
	// Shouldn't happen. Not sure what a malicious
	// client could even get from this, but safety
	// is always a good habit to have.
	if (!data->num) return 0;

	p->move = (*data)[0];
	// We used 1 byte.
	return 1;
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
	if (isCmd(str, "/_z")) actionBuild(gs, p);
	else if (isCmd(str, "/_x")) actionDig(gs, p);
	else if (isCmd(str, "/_c")) actionBomb(gs, p);
	else if (isCmd(str, "/_v")) {
		if (isReal) puts("Unimplemented!");
	} else {
		// If unprocessed, "main.cpp" puts this in a text chat buffer.
		// We don't render that though, so it's basically lost.
		if (isReal) printf("Unprocessed cmd: %s\n", str);
		return 0;
	}
	// We hit one of the earlier blocks, the command was processed.
	return 1;
}

void prefsToCmds(queue<strbuf> *cmds) {
	// No prefs that need to be sent for now
}

//// graphics stuff! ////
static double lastCenterX = boardSizeSpaces/2*16, lastCenterY = boardSizeSpaces/2*16;

static void drawPlayer(gamestate *gs, int i, int centerX, int centerY) {
	player &p = gs->players[i];
	int x = p.x*16 - centerX;
	int y = p.y*16 - centerY;

	// Draw legs (currently always the passive legs)
	drawSprite(x, y, 2, 2);

	// Then draw the head...
	int headIx = i%5;
	// Sprite sheet layout is kinda funny, and I don't have a dictionary of any sort currently.
	// For now, this is it lol
	if (headIx < 2) drawSprite(x, y, headIx, 1);
	else drawSprite(x, y, headIx-2, 0);
}

void draw(gamestate *gs, int myPlayer, float interpRatio, long drawingNanos, long totalNanos) {
	setupFrame();

	// Okay, first we need to determine where the edges of the screen lie in world coordinates.
	// Spaces are 16 world units wide, at least for now

	{
		// Calculate screen center. We smooth this out some,
		// but not based on interpRatio since we don't want
		// it to jump even if the timeline jumps.
		player &p = gs->players[myPlayer];
		double desiredCenterX = p.x * 16;
		double desiredCenterY = p.y * 16;
		double dx = desiredCenterX - lastCenterX;
		double dy = desiredCenterY - lastCenterY;
		double dist = sqrt(dx*dx + dy*dy);
		if (dist) {
			// This doesn't perfectly account for different frame rates,
			// since the approach speed is partially based on distance
			// but I only recompute approach speed once per frame (as
			// opposed to doing some calculus to make it nicer).
			double speed = (dist + 32) * totalNanos / 1'000'000'000;
			if (speed > dist) speed = dist;
			lastCenterX += speed * dx / dist;
			lastCenterY += speed * dy / dist;
		}
	}
	int centerX = round(lastCenterX);
	int centerY = round(lastCenterY);

	int worldRadiusX = 1.0/scaleX;
	int worldRadiusY = 1.0/scaleY;
	// We have to add 8 (half a space) to all these calculations
	// because each space is centered at its coordinates
	// (as opposed to its coordinates being at its upper-left
	//  corner or something, in which case *this* math would be
	//  slightly cleaner).
	int leftChunk  = (centerX-worldRadiusX+8)/(chunkSizeSpaces*16);
	int rightChunk = (centerX+worldRadiusX+8)/(chunkSizeSpaces*16) + 1;
	int floorChunk = (centerY-worldRadiusY+8)/(chunkSizeSpaces*16);
	int cielChunk  = (centerY+worldRadiusY+8)/(chunkSizeSpaces*16) + 1;
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
						space-1, 2
					);
				}
			}
		}
	}
	range(i, gs->players.num) {
		// Want to draw own player last
		if (i == myPlayer) continue;
		drawPlayer(gs, i, centerX, centerY);
	}
	drawPlayer(gs, myPlayer, centerX, centerY);

	if (debugPrint) {
		debugPrint = 0;
		printf("%d,%d  %d,%d: %d\n", leftChunk, rightChunk, floorChunk, cielChunk, written);
	}
}
