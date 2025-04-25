#pragma once
#include "dcue/types-native.h"
using namespace native;

#include "dc/matrix.h"

struct camera_t {
    static constexpr component_type_t componentType = ct_camera;

    game_object_t* gameObject;

    float fov;
    float nearPlane;
    float farPlaneUnity;
    
    // internal state
    matrix_t devViewProjScreen;
    matrix_t devViewProjScreenSkybox;
    Plane frustumPlanes[6];
    
    V2d viewOffset;
    V2d viewWindow;
    V3d frustumCorners[8];
    float farPlane;

    void buildPlanes()
    {
        V3d *c = frustumCorners;
        Plane *p = frustumPlanes;
        V3d v51 = sub(c[1], c[5]);
        V3d v73 = sub(c[3], c[7]);

        /* Far plane */
        p[0].normal = gameObject->ltw.at;
        p[0].distance = dot(p[0].normal, c[4]);

        /* Near plane */
        p[1].normal = neg(p[0].normal);
        p[1].distance = dot(p[1].normal, c[0]);

        /* Right plane */
        p[2].normal = normalize(cross(v51,
                                            sub(c[6], c[5])));
        p[2].distance = dot(p[2].normal, c[1]);

        /* Top plane */
        p[3].normal = normalize(cross(sub(c[4], c[5]),
                                            v51));
        p[3].distance = dot(p[3].normal, c[1]);

        /* Left plane */
        p[4].normal = normalize(cross(v73,
                                            sub(c[4], c[7])));
        p[4].distance = dot(p[4].normal, c[3]);

        /* Bottom plane */
        p[5].normal = normalize(cross(sub(c[6], c[7]),
                                            v73));
        p[5].distance = dot(p[5].normal, c[3]);
    }

    void buildClipPersp()
    {
        /* First we calculate the 4 points on the view window. */
        V3d up = scale(gameObject->ltw.up, viewWindow.y);
        V3d left = scale(gameObject->ltw.right, viewWindow.x);
        V3d *c = frustumCorners;
        c[0] = add(add(gameObject->ltw.at, up), left);	// top left
        c[1] = sub(add(gameObject->ltw.at, up), left);	// top right
        c[2] = sub(sub(gameObject->ltw.at, up), left);	// bottom right
        c[3] = add(sub(gameObject->ltw.at, up), left);	// bottom left

        /* Now Calculate near and far corners. */
        V3d off = sub(scale(gameObject->ltw.up, viewOffset.y),
                    scale(gameObject->ltw.right, viewOffset.x));
        for(int32 i = 0; i < 4; i++){
            V3d corner = sub(frustumCorners[i], off);
            V3d pos = add(gameObject->ltw.pos, off);
            c[i] = add(scale(corner, nearPlane), pos);
            c[i+4] = add(scale(corner, farPlane), pos);
        }

		buildPlanes();
    }

    void setFOV(float hfov, float ratio)
    {
        float w, h;

        w = (float)640;
        h = (float)480;
        if(w < 1 || h < 1){
            w = 1;
            h = 1;
        }
        hfov = hfov*3.14159f/360.0f;	// deg to rad and halved

        float ar1 = 4.0f/3.0f;
        float ar2 = w/h;
        float vfov = atanf(tanf(hfov/2) / ar1) *2;
        hfov = atanf(tanf(vfov/2) * ar2) *2;

        float a = tanf(hfov);
        viewWindow = { a, a/ratio };
        viewOffset = { 0.0f, 0.0f };
    }

    void beforeRender(float aspect) {
        farPlane = farPlaneUnity / 4;
        setFOV(fov, aspect);
        buildClipPersp();
        
        // calculate devViewProjScreen
		alignas(8) float view[16], proj[16];

        // View Matrix
        r_matrix_t inv;
        float det;
        invertGeneral(&inv, &det, &gameObject->ltw);
        // Since we're looking into positive Z,
        // flip X to ge a left handed view space.
        view[0]  = -inv.right.x;
        view[1]  =  -inv.right.y;
        view[2]  =  inv.right.z;
        view[3]  =  0.0f;
        view[4]  = -inv.up.x;
        view[5]  =  -inv.up.y;
        view[6]  =  inv.up.z;
        view[7]  =  0.0f;
        view[8]  =  -inv.at.x;
        view[9]  =   -inv.at.y;
        view[10] =  inv.at.z;
        view[11] =  0.0f;
        view[12] = -inv.pos.x;
        view[13] =  -inv.pos.y;
        view[14] =  inv.pos.z;
        view[15] =  1.0f;
    
        // Projection Matrix
        float invwx = 1.0f/viewWindow.x;
        float invwy = 1.0f/viewWindow.y;
        float invz = 1.0f/(farPlane-nearPlane);
    
        proj[0] = invwx;
        proj[1] = 0.0f;
        proj[2] = 0.0f;
        proj[3] = 0.0f;
    
        proj[4] = 0.0f;
        proj[5] = invwy;
        proj[6] = 0.0f;
        proj[7] = 0.0f;
    
        proj[8] = viewOffset.x*invwx;
        proj[9] = viewOffset.y*invwy;
        proj[12] = -proj[8];
        proj[13] = -proj[9];
        if(true /*projection == Camera::PERSPECTIVE*/){
            proj[10] = farPlane*invz;
            proj[11] = 1.0f;
    
            proj[15] = 0.0f;
        }else{
            proj[10] = invz;
            proj[11] = 0.0f;
    
            proj[15] = 1.0f;
        }
        proj[14] = -nearPlane*proj[10];
        
        static matrix_t DCE_MAT_SCREENVIEW = {
            { 1, 0, 0, 0},
            { 0, 1, 0, 0},
            { 0, 0, 1, 0},
            { 0, 0, 0, 1}
        };

        // DCE_MatrixViewport(0, 0, 640, 480);
        {
            float width = 640;
            float height = 480;
            float x = 0;
            float y = 0;
            DCE_MAT_SCREENVIEW[0][0] = -width * 0.5f;
            DCE_MAT_SCREENVIEW[1][1] = height * 0.5f;
            DCE_MAT_SCREENVIEW[2][2] = 1;
            DCE_MAT_SCREENVIEW[3][0] = -DCE_MAT_SCREENVIEW[0][0] + x;
            DCE_MAT_SCREENVIEW[3][1] = height - (DCE_MAT_SCREENVIEW[1][1] + y);
        }
        mat_load((matrix_t*)&DCE_MAT_SCREENVIEW);
        mat_apply((matrix_t*)&proj[0]);
        mat_apply((matrix_t*)&view[0]);
        mat_store((matrix_t*)&devViewProjScreen);

        view[12] = 0;
        view[13] = 0;
        view[14] = 0;
        mat_load((matrix_t*)&DCE_MAT_SCREENVIEW);
        mat_apply((matrix_t*)&proj[0]);
        mat_apply((matrix_t*)&view[0]);
        mat_store((matrix_t*)&devViewProjScreenSkybox);
    }
    
    enum { SPHEREOUTSIDE, SPHEREBOUNDARY, SPHEREINSIDE, SPHEREBOUNDARY_NEAR /* frustumTestSphereEx only */};

    int frustumTestSphere(const Sphere *s) const
    {
        int res = SPHEREINSIDE;
        const Plane *p = this->frustumPlanes;
        for(int32 i = 0; i < 6; i++){
            float dist = dot(p->normal, s->center) - p->distance;
            if(s->radius < dist)
                return SPHEREOUTSIDE;
            if(s->radius > -dist)
                res = SPHEREBOUNDARY;
            p++;
        }
        return res;
    }

    int frustumTestSphereNear(const Sphere *s) const
    {
        int res = SPHEREINSIDE;
        const Plane *p = this->frustumPlanes;
    
        // far
        float dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        // near
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        if(s->radius > -dist)
            res = SPHEREBOUNDARY_NEAR;
        p++;
    
        // others
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        return res;
    }
};

extern camera_t* cameras[];