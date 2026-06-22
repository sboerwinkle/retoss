#include <AL/al.h>
#include <AL/alc.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "matrix.h"
#include "list.h"

#include "game.h"

#include "sound.h"

// https://www.openal.org/documentation/OpenAL_Programmers_Guide.pdf

struct source {
	int32_t start;
	offset pos;
	offset vel;
	int posType;
	ALuint alSrc;
};

struct soundId {
	int32_t time;
	uint32_t id;
};

static list<source> activeSources;
static list<soundId> soundIds;
static int soundIdWindowFrames = 3;

list<offset_t> sound_playerPositions;

static ALuint alBuffers[NUM_SOUNDS];
static int64_t refDists[NUM_SOUNDS];

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
	rangeconst(i, soundIds.num) {
		soundId &s = soundIds[i];
		if (
			s.id == r->id
			&& abs(s.time - r->time) <= soundIdWindowFrames
		) {
			return;
		}
	}

	{
		soundId &s = soundIds.add();
		s.id = r->id;
		s.time = r->time;
	}

	source &s = activeSources.add();
	s.start = r->time;
	memcpy(s.pos, r->pos, sizeof(offset));
	memcpy(s.vel, r->vel, sizeof(offset));
	s.posType = r->posType;
	alGenSources(1, &s.alSrc);

	int sound = r->sound;
	if (sound < 0 || sound >= NUM_SOUNDS) {
		printf("WARN: Invalid sound # %d\n", sound);
		sound = 0;
	}
	alSourcei(s.alSrc, AL_BUFFER, alBuffers[sound]);
	alSourcef(s.alSrc, AL_REFERENCE_DISTANCE, 700);

	// With the exponential rolloff model,
	// this 2 means `1/distance^2`
	alSourcef(s.alSrc, AL_ROLLOFF_FACTOR, 2);
	//alSourcef(s.alSrc, AL_MIN_GAIN, 0.8);

	//alSourcei(singleSource.alSrc, AL_LOOPING, 1);
	//alSourcePlay(s.alSrc);
}

static void housekeepSoundIds(int32_t finishedTime, int32_t recentTime) {
	// The next frame that should be submitting any requests is finishedTime+1,
	// but keep some older frames for comparison as well.
	int32_t firstTime = finishedTime + 1 - soundIdWindowFrames;
	// Want to handle time wrapping correctly, so we use relative offsets
	int32_t duration = recentTime - firstTime;
	range(i, soundIds.num) {
		int32_t delta = soundIds[i].time - firstTime;
		// `delta > duration` shouldn't usually happen,
		// but we should expect some occasional time
		// jumps (like from loading a level).
		if (delta < 0 || delta > duration) {
			soundIds.quickRmAt(i);
			i--;
		}
	}
}

static void doOrientation() {
	float output[6], input[3] = {0, 0, 0};
	float *at = output;
	float *up = output+3;
	input[1] = 0.25;
	quat_apply(at, quatCamRotation, input);
	input[1] = 0;
	input[2] = 0.25;
	quat_apply(up, quatCamRotation, input);
	alListenerfv(AL_ORIENTATION, output);

}

void sound_frame(offset p1, offset p2, int32_t time, float interp, int32_t finishedTime) {
	housekeepSoundIds(finishedTime, time);
	doOrientation();
	offset v;
	range(i, 3) v[i] = p2[i] - p1[i];
	range(i, activeSources.num) {
		source &s = activeSources[i];

		ALint state;
		alGetSourcei(s.alSrc, AL_SOURCE_STATE, &state);
		if (state == AL_STOPPED) {
			// I previously had all the `state` checks in one place (at the end),
			// but it's probably worth skipping the position math the final time.
			alDeleteSources(1, &s.alSrc);
			activeSources.quickRmAt(i);
			i--;
			continue;
		}

		ALint pos[3];
		if (s.posType == SND_POS_COORDS) {
			range(j, 3) {
				pos[j] = (s.pos[j] + s.vel[j]*(time-1-s.start) - p1[j]) + (s.vel[j] - v[j])*interp;
			}
		} else if (s.posType == SND_POS_PLAYER) {
			int playerIx = s.pos[0];
			if (playerIx < 0 || playerIx >= sound_playerPositions.num) {
				range(j, 3) pos[j] = 0;
			} else {
				range(j, 3) pos[j] = sound_playerPositions[playerIx].o[j];
			}
		}
		// Could be 3f, fv, 3i, iv.
		alSourceiv(s.alSrc, AL_POSITION, pos);
		// TODO: If I want some sounds to anchor to moving things (like players),
		//       I'll have to get clever with that somehow!
		// Could also set AL_VELOCITY for doppler effect if I cared enough.

		// Since we're interpolating between (time-1) and (time),
		// we have to wait (worst case 1 frame?) until `time > s.start`.
		if (state == AL_INITIAL && time > s.start) {
			alSourcePlay(s.alSrc);
		}
	}
}

// TODO should probably check for memory leaks
void sound_init() {
	sound_playerPositions.init();
	soundIds.init();
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
	alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);

	alGenBuffers(NUM_SOUNDS, alBuffers);
	ALenum error;
	if ((error = alGetError()) != AL_NO_ERROR) {
		printf("Some OpenAL error happened at some point: %s", alGetString(error));
		return;
	}

	loadFile("assets/sounds/sproing.mp3", alBuffers[0]);
	refDists[0] = 700;
	loadFile("assets/sounds/meat.mp3", alBuffers[1]);
	refDists[1] = 700;
	// Placeholder
	loadFile("assets/sounds/meat.mp3", alBuffers[2]);
	refDists[2] = 700;
}

void sound_destroy() {
	// TODO: Do we need to have the context be current here?
	//       Previously we'd only release it after this call.
	auto device = alcGetContextsDevice(context);

	alcDestroyContext(context);
	alcCloseDevice(device);

	activeSources.destroy();
	soundIds.destroy();
	sound_playerPositions.destroy();
}
