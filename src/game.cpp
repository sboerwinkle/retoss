#include <GLFW/glfw3.h>
#include <stdio.h>

#include "util.h"
#include "queue.h"

#include "main.h"
#include "graphics.h"

#include "game.h"
#include "game_callbacks.h"

void game_init() {
	initGraphics();
}

gamestate* game_init2() {
	gamestate *tmp = (gamestate*)malloc(sizeof(gamestate));
	// We're leaving `nonsense` uninitialized.
	// This is fine, but like bad if this was real.
	tmp->players.init();
	return tmp;
}

void game_destroy2() {}
void game_destroy() {}

void resetPlayer(gamestate *gs, int i) {}

void handleKey(int key, int action) {}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {}
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {}
void scroll_callback(GLFWwindow *window, double x, double y) {}
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
		int32_t s;
		char const *input = str+6;
		if (getNum(&input, &s)) {
			gs->nonsense = s;
			printf("State set to %d (%hhd)\n", s, isReal);
		} else if (isReal && isMe) {
			printf("State is %d\n", gs->nonsense);
		}
		return 1;
	} else {
		if (isReal) puts(str);
		return 0;
	}
}

void prefsToCmds(queue<strbuf> *cmds) {
	// No prefs that need to be sent for now
}

//// game stuff I guess? ////

void runTick (gamestate *gs) {}

gamestate* dup(gamestate *orig) {
	gamestate *ret = (gamestate*)malloc(sizeof(gamestate));
	ret->nonsense = orig->nonsense;
	ret->players.init(orig->players);
	return ret;
}

void prepareGamestateForLoad(gamestate *gs, char isSync) {
	// Usually this would be resetting a bunch of stuff!
	// But there's no stuff really.
	// No player state, and currently game state is
	// overwritten by `serialize.cpp` w/ no issues
}

void doCleanup(gamestate *gs) {
	gs->players.destroy();
}

//// graphics stuff! ////

void draw(gamestate *gs, float interpRatio, long drawingNanos, long totalNanos) {
	setupFrame();
}
