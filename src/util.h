#pragma once
#include <stdint.h>

#define range(var, lim) for(int var = 0; var < lim; var++)
#define rangeconst(var, lim) for(int var = 0, int var##_count = lim; var < var##_count; var++)

// Will need to keep an eye on this file, I think I see a spot of rust
typedef unsigned char u8;

extern char getNum(const char **c, int32_t *out);
