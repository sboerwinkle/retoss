#include <stdint.h>

// Not sure where this originates from, but I got it from this StackOverflow answer:
// https://stackoverflow.com/questions/17035441/looking-for-decent-quality-prng-with-only-32-bits-of-state/52056161#52056161
// I think it may actually be pretty common.
uint32_t splitmix32(uint32_t *state) {
    uint32_t z = (*state += 0x9e3779b9);
    z ^= z >> 16; z *= 0x21f0aaad;
    z ^= z >> 15; z *= 0x735a2d97;
    z ^= z >> 15;
    return z;
}
