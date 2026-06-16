#include <AL/al.h>
#include <AL/alc.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "matrix.h"
#include "list.h"

#include "sound.h"

// https://www.openal.org/documentation/OpenAL_Programmers_Guide.pdf

struct source {
	offset pos;
	offset vel;
	int32_t start;
	ALuint alSrc;
};

static list<source> activeSources;

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

void sound_add(snd_request *r) {
	source &s = activeSources.add();
	memcpy(s.pos, r->pos, sizeof(offset));
	memcpy(s.vel, r->vel, sizeof(offset));
	s.start = r->time;
	alGenSources(1, &s.alSrc);

	// TODO: Use `r->sound` instead of always taking `alBuffers[0]`.
	alSourcei(s.alSrc, AL_BUFFER, alBuffers[0]);
	//alSourcei(singleSource.alSrc, AL_LOOPING, 1);
	alSourcePlay(s.alSrc);
}

void sound_frame(offset p1, offset p2, int32_t time, float interp) {
	offset v;
	range(i, 3) v[i] = p2[i] - p1[i];
	range(i, activeSources.num) {
		source &s = activeSources[i];
		ALint val;
		alGetSourcei(s.alSrc, AL_SOURCE_STATE, &val);
		if (val == AL_STOPPED) {
			alDeleteSources(1, &s.alSrc);
			activeSources.quickRmAt(i);
			i--;
			continue;
		}

		/* TODO: enable
		ALint pos[3];
		range(i, 3) {
			pos[i] = (s.pos[i] + s.vel[i]*(time-s.start) - p1[i]) + (s.vel[i] - v[i])*interp;
		}
		// Could be 3f, fv, 3i, iv.
		alSourceiv(s.alSrc, AL_POSITION, pos);
		*/
		// TODO: If I want some sounds to anchor to moving things (like players),
		//       I'll have to get clever with that somehow!
		// Could also set AL_VELOCITY for doppler effect if I cared enough.
	}
}

// TODO should probably check for memory leaks
void sound_init() {
	activeSources.init();

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

	loadFile("assets/sounds/sproing.mp3", alBuffers[0]);
}

void sound_destroy() {
	// TODO: Do we need to have the context be current here?
	//       Previously we'd only release it after this call.
	auto device = alcGetContextsDevice(context);

	alcDestroyContext(context);
	alcCloseDevice(device);

	activeSources.destroy();
}
