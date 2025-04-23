#include <tuple>
#include <vector>
#include <cstddef>
#include <cstdint>

#include "components.h"

namespace native {
    struct game_object_t;
}

enum animation_property_key
{
    AudioSource_Enabled,
    AudioSource_Volume,

    Camera_FOV,

    GameObject_IsActive,

    Image_Color_b,
    Image_Color_g,
    Image_Color_r,

    Light_Color_b,
    Light_Color_g,
    Light_Color_r,
    Light_Intensity,

    MeshRenderer_Enabled,
    MeshRenderer_material_Color_a,
    MeshRenderer_material_Color_b,
    MeshRenderer_material_Color_g,
    MeshRenderer_material_Color_r,
    MeshRenderer_material_Glossiness,
    MeshRenderer_material_Metallic,
    MeshRenderer_material_Mode,

    Transform_localEulerAnglesRaw_x,
    Transform_localEulerAnglesRaw_y,
    Transform_localEulerAnglesRaw_z,
    Transform_m_LocalPosition_x,
    Transform_m_LocalPosition_y,
    Transform_m_LocalPosition_z,
    Transform_m_LocalScale_x,
    Transform_m_LocalScale_y,
    Transform_m_LocalScale_z,
};

struct animation_track_t {
    float* times;
    float* values;
    size_t num_keys;
    animation_property_key property_key;
};

struct animation_t {
    animation_track_t* tracks;
    size_t num_tracks;
    float maxTime;
};

struct bound_animation_t {
    animation_t* animation;
    size_t* bindings;
    unsigned* currentFrames;
    float currentTime;
};

struct animator_t {
    static constexpr component_type_t componentType = ct_animator;
    native::game_object_t* gameObject;

    bound_animation_t* bound_animations;
    size_t num_bound_animations;
    size_t gameObjectIndex;

    void update(float deltaTime);
};

extern std::vector<animator_t*> animators;