#pragma once
#include "dcue/types-native.h"
using namespace native;

#include "dc/matrix.h"

struct directional_light_t {
    static constexpr component_type_t componentType = ct_directional_light;

    game_object_t* gameObject;

    V3d direction;
    RGBAf color;
    float intensity;
};

struct point_light_t {
    static constexpr component_type_t componentType = ct_point_light;

    game_object_t* gameObject;

    V3d position;
    RGBAf color;
    float intensity;
    float Range;
};

extern directional_light_t* directional_lights[];
extern point_light_t* point_lights[];