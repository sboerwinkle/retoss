
#define RIFLE_RANGE 100'000

struct toolRifle : toolInst {
	int32_t cooldown;
};

extern void toolRifle_create(toolInst **_data);
extern void toolRifle_define(toolDefn *defn);
