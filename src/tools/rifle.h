
#define RIFLE_RANGE 100'000

struct toolRifle : toolInst {
	int32_t cooldown;
	char octopus;
};

extern void toolRifle_create(toolInst **_data, char octopus);
extern void toolRifle_define(toolDefn *defn);
