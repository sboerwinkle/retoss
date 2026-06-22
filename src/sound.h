#pragma once

#define NUM_SOUNDS 3

struct snd_request {
	int32_t time;
	uint32_t id;

	offset pos;
	offset vel;
	int posType; // SND_POS_...
	int sound;
};

enum {
	SND_POS_COORDS,
	SND_POS_PLAYER,
};

extern list<offset_t> sound_playerPositions;

extern void sound_grab();
extern void sound_ungrab();

extern void sound_add(snd_request *r);
extern void sound_frame(offset p1, offset p2, int32_t time, float interp, int32_t finishedTime);

extern void sound_init();
extern void sound_destroy();
