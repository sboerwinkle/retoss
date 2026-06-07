#include <AL/al.h>
#include <AL/alc.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "matrix.h"

#include "sound.h"

// https://www.openal.org/documentation/OpenAL_Programmers_Guide.pdf

struct source {
	offset pos;
	offset vel;
	int32_t start;
	ALuint al_src;
};

static ALuint alBuffers[NUM_SOUNDS];

static ALCcontext *context;

void sound_grab() {
	alcMakeContextCurrent(context);
}

void sound_ungrab() {
	alcMakeContextCurrent(NULL);
}

static void _loadFile(char const *filename, SNDFILE *sfHandle, SF_INFO *info, ALuint alBuf) {
	if (sf_error(sfHandle)) {
		printf("Failure while reading sound '%s': %s\n", filename, sf_strerror(sfHandle));
		return;
	}
	if (info->channels != 1) {
		// We're going to be positioning this in a game world,
		// so stereo audio doesn't make sense. Also skips some
		// math with frames vs samples.
		printf(
			"Failure while reading sound '%s': Expected mono audio, but got %d channels\n",
			filename,
			info->channels
		);
		return;
	}
	int frames = info->frames;
	size_t size = sizeof(int16_t)*frames;
	int16_t *buffer = (int16_t*)malloc(size);
	sf_read_short(sfHandle, buffer, frames);
	alBufferData(alBuf, AL_FORMAT_MONO16, buffer, size, info->samplerate);
	free(buffer);
}

static void loadFile(char const *filename, ALuint alBuf) {
	SF_INFO info = {0};
	SNDFILE *sfHandle = sf_open(filename, SFM_READ, &info);
	_loadFile(filename, sfHandle, &info, alBuf);
	sf_close(sfHandle);
	// Could check error status of sf_close, but I don't
	// care enough. Would need to use `sf_error_number`.
}

void sound_init() {
	auto device = alcOpenDevice(NULL);
	if (!device) {
		puts("OpenAL device creation failed");
		// TODO is this fatal? (+ context failure case)
		return;
	}
	context = alcCreateContext(device, NULL);
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

	loadFile("assets/sounds/laugh.mp3", alBuffers[0]);

	source singleSource;
	alGenSources(1, &singleSource.al_src);
	alSourcei(singleSource.al_src, AL_LOOPING, 1);
	alSourcei(singleSource.al_src, AL_BUFFER, alBuffers[0]);
	alSourcePlay(singleSource.al_src);

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
	// TODO: Do we need to have the context be current here?
	//       Previously we'd only release it after this call.
	auto device = alcGetContextsDevice(context);

	alcDestroyContext(context);
	alcCloseDevice(device);
}
