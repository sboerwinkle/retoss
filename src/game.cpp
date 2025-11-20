#include <GLFW/glfw3.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "queue.h"

#include "quaternion.h"

#include "main.h"
#include "graphics.h"
#include "gamestate.h"

#include "game.h"
#include "game_callbacks.h"

static int mouseX = 0, mouseY = 0;
static char mouseDown = 0;
static char debugPrint;
quat tmpGameRotation = {1,0,0,0};
quat quatCamRotation = {1,0,0,0};

struct {
	char u, d, l, r;
} activeInputs = {}, sharedInputs = {};

void game_init() {
	initGraphics();
	velbox_init();
	gamestate_init();
}

// TODO gamestate init logic should be the responsibility of gamestate.cpp.
//      `game_init2` can be responsible for level gen if it wants to,
//      but not data integrity.
gamestate* game_init2() {
	gamestate *tmp = (gamestate*)malloc(sizeof(gamestate));
	init(tmp);
	/*
	timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	uint32_t seed = now.tv_sec;
	shuffle(tmp, seed);
	*/

	return tmp;
}

void game_destroy2() {}
void game_destroy() {
	gamestate_destroy();
	velbox_destroy();
	// no "destroy" call for graphics at present, maybe should make a stub for symmetry's sake?
}

void handleKey(int key, int action) {
	// I can't imagine a scenario where I actually need to know about repeat events
	if (action == GLFW_REPEAT) return;

	     if (key == GLFW_KEY_RIGHT) activeInputs.r = action;
	else if (key == GLFW_KEY_LEFT)  activeInputs.l = action;
	else if (key == GLFW_KEY_DOWN)  activeInputs.d = action;
	else if (key == GLFW_KEY_UP)    activeInputs.u = action;
	else if (key == GLFW_KEY_X)	debugPrint = 1;
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
	int x = xpos;
	int y = ypos;
	if (mouseDown) {
		float dx = x - mouseX;
		float dy = y - mouseY;
		float dist = sqrt(dx*dx + dy*dy);
		float scale = mouseDown == 1 ? 0.002 : -0.0008;
		float radians = dist * scale;
		// Because of quaternion math, this represents a rotation of `radians*2`.
		// We cap it at under a half rotation, after which I'm not sure my
		// quaternions keep behaving haha
		if (radians > 1.5) radians = 1.5;
		float s = sin(radians);
		// Positive X -> mouse to the right -> rotate about +Z
		// Positive Y -> mouse down -> rotate around +X

		quat r = {cos(radians), s*dy/dist, 0, s*dx/dist};
		if (mouseDown == 1) {
			quat_rotateBy(tmpGameRotation, r);
			quat_norm(tmpGameRotation);
		} else {
			quat o;
			quat_mult(o, r, quatCamRotation);
			quat_norm(o);
			memcpy(quatCamRotation, o, sizeof(quat));
		}
	}
	mouseX = x;
	mouseY = y;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	if (action == GLFW_PRESS && (button == 0 || button == 1)) mouseDown = button+1;
	else mouseDown = 0;
}
void scroll_callback(GLFWwindow *window, double x, double y) {}
void window_focus_callback(GLFWwindow *window, int focused) {}

void copyInputs() {
	// This matter a lot more if you're doing anything non-atomic writes,
	// or if serializing the inputs means writing something
	// (like for commands you want to be sent out exactly once).
	sharedInputs = activeInputs;
}

// In theory I think this would let us have a dynamic size of input data per-frame.
// In practice we don't use that, partly because anything that *might* winds up
// being better implemented as a command, which ensures delivery (even if late,
// or stacked up with commands from other frames).
int getInputsSize() { return 3*sizeof(int32_t); }
void serializeInputs(char * dest) {
	// TODO I have all this nice stuff for serializing, and I'm going to ignore it
	int32_t *p = (int32_t*) dest;

	float keyboard_x = (float)(sharedInputs.r-sharedInputs.l);
	float keyboard_y = (float)(sharedInputs.u-sharedInputs.d);
	float dirKeyboard[3] = {keyboard_x, keyboard_y, 0};
	float dirWorld[3];
	quat_apply(dirWorld, quatCamRotation, dirKeyboard);
	range(i, 3) p[i] = 100*dirWorld[i];
}
int playerInputs(player *p, list<char> const * data) {
	if (data->num < 12) return 0;
	int32_t *ptr = (int32_t*)(data->items);
	range(i, 3) p->pos[i] += ptr[i];
	return 12;
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
	if (0) {
		// Previously there were actual commands to process here.
		// I like this else/if structure though so I'm keeping it.
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

// The supplied gamestate is not being changed by anyone else (owned by the graphics thread),
// but the game thread *can* be cloning it (`dup`) if there's no newer server data yet.
// Graphics thread must bear this in mind if it wants to do any writes to data in `gs`.
void draw(gamestate *gs, int myPlayer, float interpRatio, long drawingNanos, long totalNanos) {
	setupFrame(gs->players[myPlayer].pos);

	rangeconst(i, gs->solids.num) {
		solid *s = gs->solids[i];
		drawCube(s->pos, s->r, s->tex & 31, !!(s->tex & 32));
	}

	setup2d();
	setup2dText();
	char msg[20];
	snprintf(msg, 20, "Hello, World!1! %2d.", gs->vb_root->kids.num);
	drawText(msg, 1, 1);

	player *p = &gs->players[myPlayer];
	snprintf(msg, 20, "t: %5d", p->tmp);
	drawText(msg, 1, 8);
	snprintf(msg, 20, "(%5d,%5d,%5d)", p->prox->pos[0], p->prox->pos[1], p->prox->pos[2]);
	drawText(msg, 1, 15);
	snprintf(msg, 20, "r %6d", p->prox->r);
	drawText(msg, 1, 22);


	if (debugPrint) debugPrint = 0; // lol
	// We'll do more stuff here eventually (again)!
}
