#pragma once
#include <cstdint>

struct float16 {
    uint16_t raw = 0;
    
    float16() {
        raw = 0;
    }
    
    float16(float v) {
        raw = ((uint32_t&)v)>>16;
    }

    operator float() const {
        /* Required workaround for GCC14.2.0 -Wuninitialized compiler warning bug. */
        union {
            uint32_t fraw;
            float fp;
        };

        fraw = (raw << 16);

        return fp;
    }

    float16& operator+=(float other) {
        *this = *this + other;
        return *this;
    }

    float16& operator-=(float other) {
        *this = *this - other;
        return *this;
    }
    float16& operator*=(float other) {
        *this = *this * other;
        return *this;
    }
    float16& operator/=(float other) {
        *this = *this / other;
        return *this;
    }

    float16 operator-() const  {
       return - (float)*this;
    }

    float operator+(float other) const {
        return (float)*this  + other;
    }
    float operator-(float other) const {
        return (float)*this  - other;
    }
    float operator*(float other) const {
        return (float)*this  * other;
    }

    float operator/(float other) const {
        return (float)*this  / other;
    }

    bool operator==(float16 other) const {
        return raw == other.raw;
    }
};