#pragma once
#include <stdint.h>

#define range(var, lim) for(int var = 0; var < lim; var++)
#define rangeconst(var, lim) for(int var = 0, var##_count = lim; var < var##_count; var++)

// "32;2" is "green and dim". I like how this looks, but if it's too dim,
// you can remove the ";2". Apparently it's less widely supported anyway,
// so some people may just get the "green" part. Even so, it should still
// be easier to pick out the non-quiet messages.
#define QUIET(x) "\033[32;2m" x "\033[0m"

// Will need to keep an eye on this file, I think I see a spot of rust
typedef unsigned char u8;

extern char getNum(const char **c, int32_t *out);

#ifdef WINDOWS

// mingw toolchain doesn't have these defined,
// but we don't need them for Windows anyway
// since I'm assuming we won't have to `fork`
// to get the browser open (maybe, hopefully).
#define O_CLOEXEC 0
#define FD_CLOEXEC 0
#define SOCK_CLOEXEC 0

// Another function that doesn't exist.
// For now I just want this to compile.
#define strerrorname_np(x) "[NO DESCRIPTION, FIX ME]"

// Who needs file permissions anyway
#define mkdir(x, y) mkdir(x)

// The `_RAW` variant is Linux-specific, and only kind of matters anyway.
// IDK how Windows handles NTP updates, which is one of the differences.
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC

#endif
