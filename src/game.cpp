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
quat tmpGameRotation = {1,0,0,0};
quat quatWorldToCam = {1,0,0,0};

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
	velbox_init();
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
	velbox_destroy();
	// no "destroy" call for graphics at present, maybe should make a stub for symmetry's sake?
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
	int x = xpos;
	int y = ypos;
	if (mouseDown) {
		float dx = x - mouseX;
		float dy = y - mouseY;
		float dist = sqrt(dx*dx + dy*dy);
		float scale = mouseDown == 1 ? 0.002 : 0.0004;
		float radians = dist * scale;
		// Because of quaternion math, this represents a rotation of `radians*2`.
		// We cap it at under a half rotation, after which I'm not sure my
		// quaternions keep behaving haha
		if (radians > 1.5) radians = 1.5;
		float s = sin(radians);
		// Positive X -> mouse to the right -> rotate about +Z
		// Positive Y -> mouse down -> rotate around +X

		quat r = {cos(radians), s*dy/dist, 0, s*dx/dist};
		float *dest = mouseDown == 1 ? tmpGameRotation : quatWorldToCam;
		quat_rotateBy(dest, r);
		quat_norm(dest);
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

void draw(gamestate *gs, int myPlayer, float interpRatio, long drawingNanos, long totalNanos) {
	setupFrame();
	int64_t pos[3] = {0, 3, 0};
	// Args are pos, scale, tex #, and whether the texture should be used as a net or not.
	drawCube(pos, 1, 2, 1);
	pos[0] += 1; pos[1] += 1; pos[2] += 1; // Elsewhere...
	drawCube(pos, 1, 4, 0);
	pos[0] += 30;
	pos[1] += 5;
	drawCube(pos, 15, 4, 0);

	setup2d();
	setup2dText();
	drawText("Hello, World!1!");

	// We'll do more stuff here eventually (again)!
}
