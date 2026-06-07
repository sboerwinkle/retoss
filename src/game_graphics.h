#pragma once
#include <atomic>

// Kind of a mess for now, I'm tired.
// Might sort this all our later.
// ggc = gamegfxcom = game-thread to graphics-thread communication
// It seemed like too much bloat to add to gamestate.h, idk man.
// Probably I'm just going to assume that gamestate.h is already
// included for the relevant types.

// For `ggc_msg.type`
enum {
	GGC_DYNTEX_NEW,
	GGC_DYNTEX_OLD,
	GGC_SND,
};

// TODO this name makes it really hard to guess which file this is in,
//      need to clean this up somehow.
struct snd_request {
	int32_t time;
	int32_t id;

	offset pos;
	offset vel;
	int sound;
};

struct ggc_msg {
	int type;
	union {
		dyntex_holder *texHolder;
		snd_request snd;
	} data;
};

extern void addGgcMsg(int type, dyntex_holder *data);
