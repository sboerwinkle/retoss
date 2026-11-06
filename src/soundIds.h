#define SND_JUMP 0
#define SND_OOF_A 1
#define SND_POP 4
#define SND_TAP 5
#define SND_WHOOSH_A 6

// The first byte currently can be: 0xFF - player sounds; 0xFE - projectile sounds
// For projectiles, I'm currently using:
// 0x00FF'0000 - projectile type (but rockets have all of 80-8F, they make a lot of sounds each)
// 0x0000'FF00 - originating player
// 0x0000'003F - shoot count (wraps at 64, high 2 bits unused)
#define SND_ID_ROCKET  0xFE80'0000
#define SND_ID_GRENADE 0xFE90'0000
