
#define SND_JUMP 0
#define SND_OOF_A 1
#define SND_POP 4
#define SND_TAP 5
#define SND_WHOOSH_A 6

#define SND_ID_TOOL_RL(pl) (0xFF00'1000 + (pl)*0x1'0000)

extern void addSound(int32_t time, offset pos, offset vel, uint32_t id, int sound);
extern void addPlayerSound(int32_t time, int who, uint32_t id, int sound);
