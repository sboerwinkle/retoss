#pragma once
#include <atomic>

// Kind of a mess for now, I'm tired.
// (Update: still tired)
// Might sort this all our later.
// ggc = gamegfxcom = game-thread to graphics-thread communication
// It seemed like too much bloat to add to gamestate.h, idk man.
// Probably I'm just going to assume that gamestate.h is already
// included for the relevant types.

#include "sound.h"

// For `ggc_msg.type`
enum {
	GGC_DYNTEX_NEW,
	GGC_DYNTEX_OLD,
	GGC_SND,
};

struct ggc_msg {
	int type;
	union {
		dyntex_holder *texHolder;
		snd_request snd;
	} data;
};

extern void ggcDestroy(ggc_msg *msg);
extern void addGgcMsg(int type, dyntex_holder *data);
