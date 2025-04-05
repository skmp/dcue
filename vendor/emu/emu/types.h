#pragma once

#include <cassert>
#include <cstdint>

typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#define verify assert

#define die(...) assert(false && __VA_ARGS__)