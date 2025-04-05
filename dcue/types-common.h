#pragma once

struct V3d {
    float x, y, z;
};

struct TexCoords {
    float u, v;
};

struct Sphere {
    V3d center;
    float radius;
};

// v3d functions
inline V3d makeV3d(float x, float y, float z) { V3d v = { x, y, z }; return v; }
inline bool equal(const V3d &v1, const V3d &v2) { return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z; }
inline V3d neg(const V3d &a) { return makeV3d(-a.x, -a.y, -a.z); }
inline V3d add(const V3d &a, const V3d &b) { return makeV3d(a.x+b.x, a.y+b.y, a.z+b.z); }
inline V3d sub(const V3d &a, const V3d &b) { return makeV3d(a.x-b.x, a.y-b.y, a.z-b.z); }
inline V3d scale(const V3d &a, float r) { return makeV3d(a.x*r, a.y*r, a.z*r); }
inline float length(const V3d &v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
inline V3d normalize(const V3d &v) { return scale(v, 1.0f/length(v)); }
inline V3d setlength(const V3d &v, float l) { return scale(v, l/length(v)); }
inline V3d cross(const V3d &a, const V3d &b) {
	return makeV3d(a.y*b.z - a.z*b.y,
	               a.z*b.x - a.x*b.z,
	               a.x*b.y - a.y*b.x);
}
inline float dot(const V3d &a, const V3d &b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline V3d lerp(const V3d &a, const V3d &b, float r) {
	return makeV3d(a.x + r*(b.x - a.x),
	               a.y + r*(b.y - a.y),
	               a.z + r*(b.z - a.z));
};

// RGBA
struct RGBA
{
	uint8 red;
	uint8 green;
	uint8 blue;
	uint8 alpha;
};
inline RGBA makeRGBA(uint8 r, uint8 g, uint8 b, uint8 a) { RGBA c = { r, g, b, a }; return c; }
inline bool equal(const RGBA &c1, const RGBA &c2) { return c1.red == c2.red && c1.green == c2.green && c1.blue == c2.blue && c1.alpha == c2.alpha; }
#define RWRGBAINT(r, g, b, a) ((uint32)((((a)&0xff)<<24)|(((b)&0xff)<<16)|(((g)&0xff)<<8)|((r)&0xff)))