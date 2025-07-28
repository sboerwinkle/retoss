#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "util.h"
#include "list.h"
#include "queue.h"
#include "bloc.h"

#include "config.h"
#include "game.h"
#include "graphics.h"
#include "mypoll.h"
#include "net.h"
#include "net2.h"
#include "serialize.h"
#include "watch.h"
#include "file.h"

#include "main.h"
#include "game_callbacks.h"

char globalRunning = 1;

static int myPlayer;

// Text input / sending queued text is
// something I'm putting in the "core"
// engine. Probably all games need it.
char textBuffer[TEXT_BUF_LEN];
char textSendInd = 0;
static queue<strbuf> outboundTextQueue;

struct {
	int width, height;
	char changed;
} screenSize;

// For rendering that isn't frame-locked.
// Depends on what a `gamestate` is.
static struct {
	gamestate *pickup, *dropoff;
	long nanos;
} renderData = {0};
pthread_mutex_t renderMutex = PTHREAD_MUTEX_INITIALIZER;
gamestate *renderedState;
long renderStartNanos = 0;
char manualGlFinish = 1;

static int typingLen = -1;

static char chatBuffer[TEXT_BUF_LEN];
static char loopbackCommandBuffer[TEXT_BUF_LEN];

// Mostly, nobody outside this file should use `rootState`,
// but it's helpful to have for debug prints somtimes.
gamestate *rootState;
static gamestate *phantomState;

static GLFWwindow *display;

static long inputs_nanos = 0;
static long update_nanos = 0;
static long follow_nanos = 0;
static struct timespec t1, t2, t3, t4;
static int performanceFrames = 0, performanceIters = 0;
#define PERF_FRAMES_MAX 120

// 1 sec = 1 billion nanos
#define BILLION  1000000000
// This should be 666.. repeating,
// but we want to step a little slower.
// The interplay of these constants affects
// how patient and aggressive we are when
// it comes to watching for and responding to
// lag spikes. I'm not sure about these values,
// but hopefully they're about right?
#define STEP_NANOS   67666666
#define FASTER_NANOS 66000000
#define PENALTY_FRAMES 90

static int fasterFrames = 0;
static time_t startSec;

#define BIN_CMD_SYNC 128
#define BIN_CMD_LOAD 129
static queue<list<char>> outboundData;
static list<char> syncData; // Temporary buffer for savegame data, for "/sync" command
static int syncNeeded = 0;
pthread_mutex_t sharedInputsMutex = PTHREAD_MUTEX_INITIALIZER;
static char isLoader = 0;
static char prefsSent = 0;

// Simple utility to get "time" in just a regular (long) number.
// `startSec` is so that we're more confident we don't overflow.
static long nowNanos() {
	timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	return BILLION * (now.tv_sec - startSec) + now.tv_nsec;
}

// TODO Not totally sure if this stays? Something like it probably will
static void resetPlayers(gamestate *gs) {
	range(i, gs->players.num) resetPlayer(gs, i);
}

// TODO may have to be reworked or moved out of this file?
static void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	resetPlayers(gs);
}

static void saveGame(const char *name) {
	list<char> data;
	data.init();
	serialize(rootState, &data);
	writeFile(name, &data);
	data.destroy();
}

char isCmd(const char* input, const char *cmd) {
	int l = strlen(cmd);
	return !strncmp(input, cmd, l) && (input[l] == ' ' || input[l] == '\0');
}

static void serializeControls(int32_t frame, list<char> *_out) {
	list<char> &out = *_out;

	u8 inputSize = getInputsSize();
	int firstBlock = inputSize + 6; // 1 size + 4 frame + ? input data + 1 cmd count

	out.setMaxUp(firstBlock);
	out.num = firstBlock;

	// The server needs to know some things like frame index
	// and where commands start/end so it can uphold its end
	// of the contract (not broadcasting late frames, making
	// sure commands aren't dropped, etc). The format of the
	// "input data" section is irrelevant to it, however, so
	// we just use a byte to describe that section's length.
	// This is more flexible, but hardcoding would work too.
	out[0] = inputSize;
	*(int32_t*)(out.items + 1) = htonl(frame);
	serializeInputs(&out[5]);

	// out[firstBlock-1] is the number of commands, populate that at the end
	u8 numCmds = 0;

	// Maybe a hook for other commands to be inserted here,
	// like `doReload` would use.

	if (syncData.num) {
		out.setMaxUp(out.num + 4 + syncData.num);
		*(int32_t*)(out.items + out.num) = htonl(syncData.num);
		out.num += 4;
		// First byte of syncData should identify to clients what kind of command it is.
		// Server doesn't care about this though, just needs length + data for the cmd.
		out.addAll(syncData);
		syncData.num = 0;
		numCmds++;
	}
	while (outboundTextQueue.size()) {
		char *const text = outboundTextQueue.pop().items;

		// Could also modify this to accept `&out` and
		// have an add'l valid return code, in case it
		// needs to write any custom command business.
		char handled = handleLocalCommand(text, &out);
		if (handled) {
			// 1: handled, nothing sent
			//      e.g. `/incr`
			//      e.g. `/export` (which goes in loopbackCommandBuffer))
			// 2: handled, +1 cmd
			//      e.g. `/import`
			numCmds += handled - 1;
			continue;
		}

		if (isCmd(text, "/perf")) {
			performanceFrames = PERF_FRAMES_MAX;
			performanceIters = 10;
		} else if (isCmd(text, "/load")) {
			const char *file = "savegame";
			if (text[5]) file = text + 6;
			int initial = out.num;
			// We'll record the size here later
			out.setMaxUp(out.num += 4);

			out.add((char)BIN_CMD_LOAD);
			if (readFile(file, &out)) {
				// If reading the file failed, don't send anything out at all
				out.num = initial;
			} else {
				printf("Loading game from %s\n", file);
				*(int32_t*)(out.items + initial) = htonl(out.num - initial - 4);
				numCmds++;
			}
		} else if (isCmd(text, "/loader")) {
			int32_t x;
			const char *c = text + 7;
			if (getNum(&c, &x)) isLoader = !!x;
			printf("loader: %s\n", isLoader ? "Y" : "N");
		} else if (isCmd(text, "/save") || isCmd(text, "/sync")) {
			// Commands that don't get roundtripped to the server.
			// Some - like /save and /export - we partly do this because they write to the filesystem
			// and we don't want to listen to the server about what we should write and where
			// (even if it's restricted to the `data/` directory and should be safe).
			// Also, other clients don't care if we issue these commands, so no reason to broadcast them.
			strcpy(loopbackCommandBuffer, text);
			// We could probably process them right here,
			// but it's less to worry about if the serialization out happens at the same
			// point in the tick cycle as the deserialization in.
		} else {
			int initial = out.num;
			int len = strlen(text);
			out.setMaxUp(out.num += len + 4);
			*(int32_t*)(out.items + initial) = htonl(len);
			memcpy(out.items + initial + 4, text, len);
			numCmds++;
		}
	}

	out[firstBlock - 1] = numCmds;
}

static void updateResolution() {
	if (screenSize.changed) {
		screenSize.changed = 0;
		setDisplaySize(screenSize.width, screenSize.height);
	}
}

static void handleSharedInputs(int outboundFrame) {
	list<char> *out = &outboundData.add();
	if (out->max > 1000) out->setMax(100); // Todo maybe bring this in line with the limits in net2.cpp, just for consistency

	mtx_lock(sharedInputsMutex);

	serializeControls(outboundFrame, out);

	mtx_unlock(sharedInputsMutex);

	sendData(out->items, out->num);
}

static void ensurePrefsSent() {
	if (prefsSent) return;
	prefsSent = 1;

	mtx_lock(sharedInputsMutex);
	prefsToCmds(&outboundTextQueue);
	mtx_unlock(sharedInputsMutex);
}

void shareInputs() {
	mtx_lock(sharedInputsMutex);

	copyInputs();
	if (textSendInd) {
		textSendInd = 0;
		strcpy(outboundTextQueue.add().items, textBuffer);
	}

	mtx_unlock(sharedInputsMutex);
}

static void processLoopbackCommand() {
	const char* const c = loopbackCommandBuffer;
	int chars = strlen(c);
	if (isCmd(c, "/save")) {
		const char *name = "savegame";
		if (chars > 6) name = c + 6;
		printf("Saving game to %s\n", name);
		saveGame(name);
	} else if (isCmd(c, "/sync")) {
		syncData.num = 0;
		syncData.add(BIN_CMD_SYNC);
		serialize(rootState, &syncData);
	} else if (!customLoopbackCommand(rootState, c)) {
		printf("Unknown loopback command: %s\n", c);
	}
}

static void processCmd(gamestate *gs, player *p, char const *data, int chars, char isMe, char isReal) {
	if (chars && (*(unsigned char*)data == BIN_CMD_LOAD || *(unsigned char*)data == BIN_CMD_SYNC)) {
		if (!isReal) return;
		char isSync = *(unsigned char*)data == BIN_CMD_SYNC;

		if (isSync) {
			if (syncNeeded > MAX_AHEAD) {
				syncNeeded = 0;
				// Probably already `isLoader == isMe`, but maybe not.
				// (e.g. if the loader left and someone else did the sync)
				isLoader = isMe;
			}
			// After we've been synced in, if there are no other auto-syncs on the horizon,
			// that's a good time to send our stuff and be fairly sure it won't be lost.
			if (!syncNeeded) ensurePrefsSent();
		} else {
			isLoader = isMe;
		}

		prepareGamestateForLoad(rootState, isSync);

		list<const char> const fakeList = {.items=data+1, .num = chars-1, .max = chars-1};

		deserialize(rootState, &fakeList, isSync);
		return;
	}
	if (processBinCmd(gs, p, data, chars, isMe, isReal)) return;
	if (chars < TEXT_BUF_LEN) {
		char tmpBuffer[TEXT_BUF_LEN];
		memcpy(tmpBuffer, data, chars);
		tmpBuffer[chars] = '\0';
		if (isCmd(tmpBuffer, "/syncme")) {
			if (isReal && !isMe) syncNeeded = 1;
		} else if (!processTxtCmd(gs, p, tmpBuffer, isMe, isReal)) {
			memcpy(chatBuffer, tmpBuffer, chars+1);
		}
	} else {
		fputs("Incoming \"chat\" buffer was too long, ignoring\n", stderr);
	}
}

static void doWholeStep(gamestate *state, list<list<char>> const *_inputData, char isReal) {
	list<list<char>> const &inputData = *_inputData;
	int numPlayers = inputData.num;
	list<player> &players = state->players;
	players.setMaxUp(numPlayers);
	while (players.num < numPlayers) {
		resetPlayer(state, players.num++);
	}

	range(i, numPlayers) {
		char isMe = i == myPlayer;
		list<char> const &data = inputData[i];

		int nextIx = playerInputs(&players[i], &data);

		if (data.num > nextIx && (isMe || isReal)) {
			u8 numCmds = data[nextIx];
			int index = nextIx + 1;
			while (numCmds--) {
				int32_t len = ntohl(*(int32_t*)(data.items + index));
				if (index+4+len > data.num) {
					fputs("net2.cpp should ensure we don't have invalid lengths here!\n", stderr);
					break;
				}
				processCmd(state, &players[i], data.items + index + 4, len, isMe, isReal);
				index += 4+len;
			}
		}
	}
	if (isReal && *loopbackCommandBuffer) {
		processLoopbackCommand();
		*loopbackCommandBuffer = '\0';
	}

	runTick(state);
}

void showMessage(gamestate const * const gs, char const * const msg) {
	// Anybody can request a message, and we don't hold it against them.
	// However, the message that is drawn to the screen isn't tied to a gamestate -
	// which means that if some gamestate gets rolled back (as happens very often),
	// any message it requested is still going to be visible even if that doesn't make sense.
	// As a result, we don't show messages until they happen on the "real" timeline.
	if (gs != rootState) return;

	int len = strnlen(msg, TEXT_BUF_LEN-1);
	// We're weird about this partly because of overflows,
	// and partly so if the draw thread catches us at a bad time
	// it writes a minimum of nonsense to the screen
	// (since the new terminator is put in first).
	chatBuffer[len] = '\0';
	memcpy(chatBuffer, msg, len);
}

static void newPhantom(gamestate *gs) {
	phantomState = dup(gs);
}

static void insertOutbound(list<char> *dest, list<char> *src) {
	// The trouble is that the data we sent out includes some header info,
	// like the frame and size. By the time we have our nice list of
	// who's doing what this frame, those two fields have been removed.
	// We have to do the same here. Fortunately, it's assumed these
	// lists are stricly read-only, so it's okay to make a "fake" list here!
	dest->num = src->num - 5;
	dest->max = src->max - 5; // Does this even matter if RO?
	dest->items = src->items + 5;
}

static void* gameThreadFunc(void *startFramePtr) {
	long performanceTotal = 0;
	int32_t outboundFrame = *(int32_t*)startFramePtr;

	list<list<char>> playerDatas;
	mtx_lock(netMutex);
	// `playerDatas` holds our current guesses for each player's inputs.
	// It is a list of "weak references" to lists, just in that it isn't
	// reponsible for freeing any of them. It will assume they're valid,
	// so we need to make sure that's safe! As a result, there is always
	// at least one finalized frame. The first finalized frame is set up
	// with some dummy values for just this purpose, during net2_init().
	playerDatas.init(frameData.peek(0));
	mtx_unlock(netMutex);

	long destNanos = nowNanos() - STEP_NANOS*3; // Start out a bit behind

	while (globalRunning) {
		long sleepNanos = destNanos - nowNanos();
		if (sleepNanos > 999999999) {
			puts("WARN - Tried to wait for more than a second???");
			// No idea why this would ever come up, but also it runs afoul of the spec to
			// request an entire second or more in the tv_nsec field.
			sleepNanos = 999999999;
		}
		char behindClock = 0;
		if (sleepNanos > 0) {
			timespec t;
			t.tv_sec = 0;
			t.tv_nsec = sleepNanos;
			if (nanosleep(&t, NULL)) continue; // Something happened, we don't really care what, do it over again
		} else {
			behindClock = 1;
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

		// Wake up and send player inputs
		handleSharedInputs(outboundFrame);
		outboundFrame = (outboundFrame + 1) % FRAME_ID_MAX;

		clock_gettime(CLOCK_MONOTONIC_RAW, &t2);

		mtx_lock(netMutex);
		// Do one more step of simulation on the phantom state and send to render thread.
		// We won't have to re-simulate every frame, so if we waited until after incorporating
		// the latest data we'd have extra variability in when frames get to the render thread.
		// Also, this keeps the latency between the keyboard and the screen slightly lower.
		{
			int outboundSize = outboundData.size();
			insertOutbound(&playerDatas[myPlayer], &outboundData.peek(outboundSize - 1));
			// There's always one leftover finalized frame, so we adjust by one here (or rather, don't adjust by one when normally we should, for index vs size)
			if (outboundSize < frameData.size()) {
				// This scenario is a little unlikely, as it means we've received data from at least one other client
				// for a frame that we just sent our data for. Clients don't like sending their data earlier than necessary
				// (or rather, they only simulate far enough ahead to get their data in on time).
				list<char> *netInputs = frameData.peek(outboundSize).items;
				range(i, playerDatas.num) {
					if (netInputs[i].num) playerDatas[i] = netInputs[i];
					// It's also possible that the netInputs we're reading are actually from a finalized frame,
					// in which case it would be more accurate to completely reset `playerDatas` to those values
					// and ignore `outboundData` completely (since finalized frames are authoritative).
					// However, that's even more unlikely - it means our client is way behind -
					// and I don't want to bother adding complexity for that case.
				}
			}
			doWholeStep(phantomState, &playerDatas, 0);

			gamestate *disposeMe = NULL;

			// Doing this outside the mutex probably doesn't matter much,
			// but waiting for the lock could be a bit inconsistent,
			// so this is marginally better I think
			long now = nowNanos();
			mtx_lock(renderMutex);
			if (renderData.dropoff) {
				disposeMe = renderData.dropoff;
				renderData.dropoff = NULL;
			} else if (renderData.pickup) {
				disposeMe = renderData.pickup;
			}
			renderData.pickup = phantomState;
			renderData.nanos = now;
			mtx_unlock(renderMutex);

			if (disposeMe) {
				doCleanup(disposeMe);
				free(disposeMe);
			}
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &t3);

		// Now we can consider the prospect of re-simulating from the rootState
		if (finalizedFrames > 1) {
			int toAdvance = finalizedFrames - 1;
			int outboundSize = outboundData.size();
			if (outboundSize < toAdvance) toAdvance = outboundSize; // Unlikely
			range(i, toAdvance) {
				// Bookkeeping for automatic sync of new clients.
				// We have to wait a bit to make sure they're up-to-date with any early-submitted inputs.
				// This part is rare.
				if (syncNeeded && syncNeeded <= 2*MAX_AHEAD) {
					syncNeeded++;
					if (syncNeeded == MAX_AHEAD && isLoader && !*loopbackCommandBuffer) {
						strcpy(loopbackCommandBuffer, "/sync");
					}
				}
				// An "official" step, all clients expect to agree on the state here
				doWholeStep(rootState, &frameData.peek(i+1), 1);
			}
			frameData.multipop(toAdvance);
			outboundData.multipop(toAdvance);
			outboundSize -= toAdvance;
			finalizedFrames -= toAdvance;

			char clockOk = behindClock; // If we're behind the clock, then don't blame issues on the clock; we just need to catch up
			playerDatas.num = 0;
			playerDatas.addAll(frameData.peek(0));
			newPhantom(rootState);
			int frameDataSize = frameData.size();
			range(outboundIx, outboundSize) {
				insertOutbound(&playerDatas[myPlayer], &outboundData.peek(outboundIx));
				if (outboundIx+1 < frameDataSize) {
					list<char> *netInputs = frameData.peek(outboundIx+1).items;
					range(i, playerDatas.num) {
						if (netInputs[i].num) {
							playerDatas[i] = netInputs[i];
							// If the server has any input from us ahead of time, we're going fast enough.
							if (i == myPlayer) clockOk = 1;
						}
					}
				}
				doWholeStep(phantomState, &playerDatas, 0);
			}
			if (!clockOk) fasterFrames = PENALTY_FRAMES;
		} else {
			if (outboundData.size() >= MAX_AHEAD) {
				puts("Game thread: Server is way behind, going to sleep until we hear something");
				asleep = 1;
				while (globalRunning && finalizedFrames <= 1) {
					mtx_wait(netCond, netMutex);
				}
				asleep = 0;
				puts("Game thread: Waking up");
				destNanos = nowNanos();
			}
			// We still need to make a copy since the rendering stuff is probably working with
			// the current phantomState.
			newPhantom(phantomState);
		}

		if (fasterFrames) {
			fasterFrames--;
			destNanos += FASTER_NANOS;
		} else {
			destNanos += STEP_NANOS;
		}

		mtx_unlock(netMutex);

		clock_gettime(CLOCK_MONOTONIC_RAW, &t4);
		{
			inputs_nanos = BILLION * (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec);
			update_nanos = BILLION * (t3.tv_sec - t2.tv_sec) + (t3.tv_nsec - t2.tv_nsec);
			follow_nanos = BILLION * (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec);
			if (performanceFrames) {
				performanceFrames--;
				performanceTotal += follow_nanos;
				if (!performanceFrames) {
					printf("perf: %ld\n", performanceTotal);
					performanceTotal = 0;
					performanceIters--;
					if (performanceIters) performanceFrames = PERF_FRAMES_MAX;
					else puts("perf done");
				}
			}
		}
	}
	playerDatas.destroy();

	return NULL;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	// Most keypresses don't do their usual function if we're
	// typing. Key releases are fine though! This approach is
	// simpler than clearing all the keys when typing starts.
	if (typingLen >= 0 && action != GLFW_RELEASE) {
		// Some keys gain a function when typing, though.
		// Should I permit GLFW_REPEAT here as well (mostly for backspace)?
		if (action == GLFW_PRESS) {
			if (key == GLFW_KEY_ESCAPE) {
				typingLen = -1;
			} else if (key == GLFW_KEY_BACKSPACE && typingLen) {
				textBuffer[typingLen] = '\0';
				textBuffer[typingLen-1] = '_';
				typingLen--;
			} else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
				textBuffer[typingLen] = '\0';
				textSendInd = 1;
				typingLen = -1;
			}
		}
		return;
	}

	// player inputs etc
	// `action` is one of GLFW_PRESS, GLFW_RELEASE, GLFW_REPEAT
	handleKey(key, action);
}

static void character_callback(GLFWwindow* window, unsigned int c) {
	if (typingLen < 0) {
		// Maybe we start typing, but nothing else
		if (!textSendInd) {
			char *const t = textBuffer;
			if (c == 't') {
				t[1] = '\0';
				t[0] = '_';
				typingLen = 0;
			} else if (c == '/') {
				t[2] = '\0';
				t[1] = '_';
				t[0] = '/';
				typingLen = 1;
			}
		}
		return;
	}

	char *const t = textBuffer;
	// `c` is the unicode codepoint
	if (c >= 0x20 && c <= 0xFE && typingLen+2 < TEXT_BUF_LEN) {
		t[typingLen+2] = '\0';
		t[typingLen+1] = '_';
		t[typingLen] = c;
		typingLen++;
	}
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	// Usually I avoid locking mutexes directly in callbacks,
	// since they could be called a lot,
	// but this one's quite rare
	mtx_lock(renderMutex);
	screenSize.width = width;
	screenSize.height = height;
	screenSize.changed = 1;
	mtx_unlock(renderMutex);
}

static void checkRenderData() {
	mtx_lock(renderMutex);
	if (renderData.pickup) {
		renderData.dropoff = renderedState;
		renderedState = renderData.pickup;
		renderData.pickup = NULL;
		renderStartNanos = renderData.nanos;
	}
	updateResolution();
	mtx_unlock(renderMutex);
}

static void* renderThreadFunc(void *_arg) {
	glfwMakeContextCurrent(display);
	// Explicitly request v-sync;
	// otherwise GLFW leaves it up to the driver default
	glfwSwapInterval(1);
	long drawingNanos = 0;
	long totalNanos = 0;
	long time0 = nowNanos();
	while (globalRunning) {
		checkRenderData();

		// Todo would this be better with STEP_NANOS, FASTER_NANOS, or something in between (like the idealized server frame nanos)?
		float interpRatio = (float)(time0 - renderStartNanos) / STEP_NANOS;
		if (interpRatio > 1.1) interpRatio = 1.1; // Ideally it would be somewhere in (0, 1]

		draw(renderedState, interpRatio, drawingNanos, totalNanos);
		long time1 = nowNanos();

		glfwSwapBuffers(display);
		if (manualGlFinish) {
			glFinish();
		}
		long time2 = nowNanos();

		drawingNanos = time1-time0;
		totalNanos = time2-time0;
		time0 = time2;
	}
	return NULL;
}

static void* inputThreadFunc(void *_arg) {
	glfwSetKeyCallback(display, key_callback);
	glfwSetCharCallback(display, character_callback);
	glfwSetCursorPosCallback(display, cursor_position_callback);
	glfwSetMouseButtonCallback(display, mouse_button_callback);
	glfwSetScrollCallback(display, scroll_callback);
	glfwSetWindowFocusCallback(display, window_focus_callback);
	glfwSetFramebufferSizeCallback(display, framebuffer_size_callback);

	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 10'000'000;

	while (!glfwWindowShouldClose(display)) {

		// glfwWaitEvents() should be exactly what we want, but in practice it would occasionally be
		// way too slow for unknown reasons (looks like dropped frames, but it's not a graphics issue;
		// the mouse data just doesn't update for few frames or so).
		// Haven't investigated further, but for now we just poll at 100 Hz,
		// which should be fast enough for a human and slow enough for a computer.
		glfwPollEvents();
		nanosleep(&t, NULL);

		shareInputs();
	}
	return NULL;
}

static char waitForThread(pthread_t thread) {
	timespec t;
	if (clock_gettime(CLOCK_REALTIME, &t) == -1) {
		printf("clock_gettime has errno %d, so can't wait.\n", errno);
		return 0;
	}
	// 1 second should be enough time here, we're not doing anything
	// really intense in these threads. Most likely reason we'd miss
	// this window would be if we're blocked on a socket operation,
	// which is rare enough I'm fine just killing the thread in that
	// case.
	t.tv_sec += 1;
	int ret = pthread_timedjoin_np(thread, NULL, &t);
	if (!ret) {
		return 1;
	}
	if (ret == ETIMEDOUT) {
		puts("Waited too long.");
	} else {
		printf("pthread_timedjoin_np returned error %d.\n", ret);
	}
	return 0;
}

static void cleanupThread(pthread_t thread, char const * const descr) {
	printf("Waiting for %s thread...\n", descr);
	if (waitForThread(thread)) {
		puts("Done.");
		return;
	}

	printf("Killing %s thread instead.\n", descr);
	pthread_cancel(thread);
	puts("Waiting for killed thread to complete...");
	if (waitForThread(thread)) {
		puts("Done.");
	} else {
		puts("Guess we're going to just move on then.");
	}
}

int main(int argc, char **argv) {
	puts("init GLFW...");
	if (!glfwInit()) {
		fputs("Couldn't init GLFW\n", stderr);
		return 1;
	}
	puts("init window...");
	display = glfwCreateWindow(1000, 700, WINDOW_TITLE, NULL, NULL);
	if (!display) {
		fputs("Couldn't create our display\n", stderr);
		return 1;
	}

	{
		// Framebuffer size is not guaranteed to be equal to window size
		int fbWidth, fbHeight;
		glfwGetFramebufferSize(display, &fbWidth, &fbHeight);
		setDisplaySize(fbWidth, fbHeight);
	}

	puts("init GL + custom stuff...");
	glfwMakeContextCurrent(display);
	// Expected to do bunches of init,
	// including GL stuff (since the context is bound to the thread here).
	game_init();
	glfwMakeContextCurrent(NULL); // Give up control so other thread can take it
	puts("GL + custom setup complete.");

	outboundTextQueue.init();
	outboundData.init();
	syncData.init();
	file_init();
	config_init(); // "config" should be after "file"

	// More config stuff; we need a viable state by this point.
	// Compared to `game_init()`, we don't have the GL context any more,
	// but we do have config loaded (and working dir is `data/` now).
	puts("init more custom...");
	rootState = game_init2();
	puts("more custom complete.");

	char const *host, *hostSrc, *port, *portSrc;
	if (argc > 3) {
		printf("At most 2 args expected, got %d\n", argc-1);
		return 1;
	}
	char const *configHost = config_getHost(), *configPort = config_getPort();
	if (argc > 1) {
		host = argv[1];
		hostSrc = "program argument";
		if (argc > 2) {
			port = argv[2];
			portSrc = "program argument";
		} else {
			port = "15000";
			portSrc = "default";
		}
	} else {
		char okay = 1;
		if (!*configHost) {
			puts("Without arguments, config must include a host, but none was found!");
			okay = 0;
		}
		if (!*configPort) {
			puts("Without arguments, config must include a port, but none was found!");
			okay = 0;
		}
		if (!okay) return 1;
		host = configHost;
		port = configPort;
		hostSrc = portSrc = "from config";
	}

	// Other general game setup, including networking
	printf("Using host '%s' (%s) and port '%s' (%s)\n", host, hostSrc, port, portSrc);
	puts("Connecting to host...");
	if (initSocket(host, port)) return 1;
	puts("Done.");
	puts("Awaiting setup info...");
	char initNetData[7];
	if (readData(initNetData, 7)) {
		puts("Error, aborting!");
		return 1;
	}
	if (initNetData[0] != (char)MAGIC_FIRST_BYTE) {
		printf("Bad initial byte 0x%hhX, aborting!\n", initNetData[0]);
		return 1;
	}
	myPlayer = initNetData[1];
	int numPlayers = initNetData[2];
	int32_t startFrame = ntohl(*(int32_t*)(initNetData+3));
	printf("Done, I am client #%d out of %d\n", myPlayer, numPlayers);
	setupPlayers(rootState, numPlayers);
	isLoader = (numPlayers == 1);
	// Connection was at least mostly successful,
	// record the `host` and `port` that we used.
	config_setHost(host);
	config_setPort(port);

	net2_init(numPlayers, startFrame);
	watch_init();
	mypoll_init(); // Uses fd's from "net" and "watch", so has to wait for them to init

	// init phantomState
	newPhantom(rootState);
	renderedState = dup(rootState); // Give us something to render, so we can skip null checks

	// Setup text buffers.
	// We make it so the player automatically sends the "syncme" command on their first frame
	textBuffer[TEXT_BUF_LEN-1] = '\0';
	strcpy(outboundTextQueue.add().items, "/syncme");
	if (isLoader) ensurePrefsSent();

	chatBuffer[0] = chatBuffer[TEXT_BUF_LEN-1] = '\0';
	loopbackCommandBuffer[0] = '\0';

	// set up timing stuff
	{
		timespec now;
		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		startSec = now.tv_sec;
	}

	//Main loop
	pthread_t gameThread;
	pthread_t pollThread;
	pthread_t renderThread;
	{
		int ret = pthread_create(&gameThread, NULL, gameThreadFunc, &startFrame);
		if (ret) {
			printf("pthread_create returned %d for gameThread\n", ret);
			return 1;
		}
		ret = pthread_create(&pollThread, NULL, mypoll_threadFunc, &startFrame);
		if (ret) {
			printf("pthread_create returned %d for pollThread\n", ret);
			pthread_cancel(gameThread); // No idea if this works, this is a failure case anyway
			return 1;
		}
		ret = pthread_create(&renderThread, NULL, renderThreadFunc, NULL);
		if (ret) {
			printf("pthread_create returned %d for renderThread\n", ret);
			pthread_cancel(gameThread); // No idea if this works, this is a failure case anyway
			pthread_cancel(pollThread);
			return 1;
		}
	}
	// Main thread lives in here until the program exits
	inputThreadFunc(NULL);

	// Generally signal that there's a shutdown in progress
	// (if the inputThread isn't the main thread we'd want to set the GLFW "should exit" flag and push an empty event through)
	globalRunning = 0;
	// Game thread could potentially be waiting on this condition
	mtx_lock(netMutex);
	mtx_signal(netCond);
	mtx_unlock(netMutex);

	puts("Writing config file...");
	config_write();
	puts("Done.");
	puts("Beginning cleanup.");
	cleanupThread(pollThread, "poll");
	cleanupThread(gameThread, "game");
	cleanupThread(renderThread, "render");
	closeSocket();
	puts("Cleaning up game objects...");
	doCleanup(rootState);
	free(rootState);
	doCleanup(phantomState);
	free(phantomState);
	if (renderData.pickup) renderData.dropoff = renderData.pickup;
	if (renderData.dropoff) {
		doCleanup(renderData.dropoff);
		free(renderData.dropoff);
	}
	puts("Done.");
	puts("Cleaning up simple interal components...");
	mypoll_destroy();
	watch_destroy();
	net2_destroy();
	game_destroy2(); // Mirror to game_init2
	config_destroy();
	file_destroy();
	syncData.destroy();
	outboundData.destroy();
	outboundTextQueue.destroy();
	game_destroy(); // Mirror to game_init
	puts("Done.");
	puts("Cleaning up GLFW...");
	glfwTerminate();
	puts("Done.");
	puts("All done!");
	return 0;
}
