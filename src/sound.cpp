#include <AL/al.h>
#include <AL/alc.h>
#include <stdio.h>

#include "util.h"

#include "sound.h"

// https://www.openal.org/documentation/OpenAL_Programmers_Guide.pdf

static ALuint alBuffers[NUM_SOUNDS];

void sound_init() {
	auto device = alcOpenDevice(NULL);
	if (!device) {
		puts("OpenAL device creation failed");
		// TODO is this fatal? (+ context failure case)
		return;
	}
	auto context = alcCreateContext(device, NULL);
	if (!context) {
		puts("OpenAL context creation failed");
		return;
	}
	alcMakeContextCurrent(context);

	alGenBuffers(NUM_SOUNDS, alBuffers);
	ALenum error;
	if ((error = alGetError()) != AL_NO_ERROR) {
		printf("Some OpenAL error happened at some point: %s", alGetString(error));
		return;
	}

	// alGenSources
	// Have to figure out a few things about moving sources.
	// A person going "oof" should probably move, or it'll sound weird.
	// A bullet impact noise, on the other hand, doesn't have anything to attach to,
	// 	(attaching it to the wall is asinine)
	// but presumably we still need to update its position (moving frame of reference)
	// and also reclaim the source when it's done.

	// Note that according to OpenAL docs, position and velocity units are completely independent,
	// and they also say you can turn off doppler effects by leaving all velocities at 0.
	// So this tells me positions aren't interpolated using velocities,
	// I'd have to do that myself.
	// This means manual bookkeeping of basically all sources (since presumably they can all move),
	// so the good news is tracking "finished" sources on top of that shouldn't be bad.
	//
	// Do I have to worry about race conditions if everything's moving fast and I don't update positions
	// at the same time?
	// If so, then (like GL but for different reasons) I'll have to do everything with relative distances
	// anyway. And maybe for the same reason as GL too, I bet everything is floating-point bullshit :|
}

void sound_destroy() {
	auto context = alcGetCurrentContext();
	auto device = alcGetContextsDevice(context);

	alcMakeContextCurrent(NULL);

	alcDestroyContext(context);
	alcCloseDevice(device);
}
