// Todo:
// This file is stuff declared in `game.cpp` that's used by
// game objects. This is why it became `game_gamestate.h`.
// But in practice, this is all sound stuff... so it feels
// like it should go in `sound.h`. But the functions don't
// really fit in `sound.cpp`, because they touch on
// inter-thread communication stuff which that file isn't
// aware of.
// So yeah IDK

#include "soundIds.h"

extern void addSound(int32_t time, offset pos, offset vel, uint32_t id, int sound);
extern void addPlayerSound(int32_t time, int who, uint32_t id, int sound);

extern void redoSkin(player *p);
