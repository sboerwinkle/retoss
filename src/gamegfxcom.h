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
};

struct ggc_msg {
	int type;
	// Idea was to make this a union,
	// but for now it only needs one thing.
	dyntex_holder *data;

	std::atomic<struct ggc_msg*> next;
};

extern void addGgcMsg(int type, dyntex_holder *data);

extern ggc_msg *game_msg_tail;
