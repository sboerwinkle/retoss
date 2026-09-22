// Todo:
// This file is stuff declared in `game.cpp` that's used by
// game objects. This is why it became `game_gamestate.h`.
// But in practice, this is all sound stuff... so it feels
// like it should go in `sound.h`. But the functions don't
// really fit in `sound.cpp`, because they touch on
// inter-thread communication stuff which that file isn't
// aware of.
// So yeah IDK

#define SND_JUMP 0
#define SND_OOF_A 1
#define SND_POP 4
#define SND_TAP 5
#define SND_WHOOSH_A 6

#define SND_ID_TOOL_RL(pl) (0xFE80'0000 + (pl)*0x0100)
#define SND_ID_GRENADE      0xFE81'0000

extern void addSound(int32_t time, offset pos, offset vel, uint32_t id, int sound);
extern void addPlayerSound(int32_t time, int who, uint32_t id, int sound);
