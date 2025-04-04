#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_opengl.h>
#include <OpenGL/glu.h>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <vector>

#include <unistd.h>

// Provided structs
struct texture_t {
    const char* file;
    int width;
    int height;
    GLuint id;

    texture_t(const char* file, int width, int height)
    {
        this->file = file;
        this->width = width;
        this->height = height;
        this->id = 0;
    }

    void load()
    {
        if (this->id != 0) {
            return;
        }
        SDL_Surface* loadedSurface = IMG_Load(file);
        if (!loadedSurface) {
            printf("Failed to load texture %s: %s\n", file, IMG_GetError());
            return;
        }

        // Convert to a known format (e.g., SDL_PIXELFORMAT_RGBA32)
        SDL_Surface* formattedSurface = SDL_ConvertSurfaceFormat(loadedSurface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(loadedSurface);
        if (!formattedSurface) {
            printf("Failed to convert surface format: %s\n", SDL_GetError());
            return;
        }

        // Now use the formatted surface for texture creation
        glGenTextures(1, &this->id);
        glBindTexture(GL_TEXTURE_2D, this->id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, formattedSurface->w, formattedSurface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, formattedSurface->pixels);
        SDL_FreeSurface(formattedSurface);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
};

struct material_t {
    float a, r, g, b;
    texture_t* texture;
    material_t(float a, float r, float g, float b, texture_t* texture)
    {
        this->a = a;
        this->r = r;
        this->g = g;
        this->b = b;
        this->texture = texture;
    }
};

struct mesh_t {
    uint16_t* indices;
    float* vertices;
    float* uv;
    uint8_t* col;
    float* normals;
    size_t vertex_count;
    size_t index_count;

    mesh_t(size_t index_count, uint16_t* indices, size_t vertex_count, float* vertices, float* uv, uint8_t* col, float* normals)
    {
        this->index_count = index_count;
        this->indices = indices;
        this->vertex_count = vertex_count;
        this->vertices = vertices;
        this->uv = uv;
        this->col = col;
        this->normals = normals;
    }
};

struct matrix_t {
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    float m30, m31, m32, m33;
};

struct game_object_t {
    matrix_t* transform;
    mesh_t* mesh;
    material_t* material;
    bool active;
    bool mesh_enabled;

    game_object_t(bool active, matrix_t* transform, bool mesh_enabled, mesh_t* mesh, material_t* material)
    {
        this->active = active;
        this->transform = transform;
        this->mesh_enabled = mesh_enabled;
        this->mesh = mesh;
        this->material = material;
    }
};

// defines gameObjects[]
#include "dream.cpp"

// Render a game object using OpenGL immediate mode
void renderGameObject(game_object_t* obj) {
    if (!obj || !obj->mesh)
        return;
    
    glPushMatrix();
    // Apply the object's transform
    if (obj->transform) {
        float m[16] = {
            obj->transform->m00, obj->transform->m10, obj->transform->m20, obj->transform->m30,
            obj->transform->m01, obj->transform->m11, obj->transform->m21, obj->transform->m31,
            obj->transform->m02, obj->transform->m12, obj->transform->m22, obj->transform->m32,
            obj->transform->m03, obj->transform->m13, obj->transform->m23, obj->transform->m33
        };
        glMultMatrixf(m);
    }
    
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    if (obj->material) {
        r = obj->material->r;
        g = obj->material->g;
        b = obj->material->b;
        a = obj->material->a;
    }

    // Set the material color (ignoring texture for simplicity)
    if (obj->material) {
        if (obj->material->texture) {
            obj->material->texture->load();
            glBindTexture(GL_TEXTURE_2D, obj->material->texture->id);
            glEnable(GL_TEXTURE_2D);
        }
        // Set the color
        glColor4f(obj->material->r, obj->material->g, obj->material->b, obj->material->a);
    }
    else
        glColor4f(1, 1, 1, 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    mesh_t* mesh = obj->mesh;
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < mesh->index_count; i += 3) {
        uint16_t idx0 = mesh->indices[i];
        uint16_t idx1 = mesh->indices[i+1];
        uint16_t idx2 = mesh->indices[i+2];

        // If normals are provided, set them for each vertex
        if (mesh->normals) {
            glNormal3f(mesh->normals[idx0 * 3], mesh->normals[idx0 * 3 + 1], mesh->normals[idx0 * 3 + 2]);
        }
        if (mesh->uv) {
            glTexCoord2f(mesh->uv[idx0 * 2], mesh->uv[idx0 * 2 + 1]);
        }
        if (mesh->col) {
            glColor4f(r * mesh->col[idx0 * 4] / 255.0f, g * mesh->col[idx0 * 4 + 1] / 255.0f, b * mesh->col[idx0 * 4 + 2] / 255.0f, a * mesh->col[idx0 * 4 + 3] / 255.0f);
        }
        glVertex3f(mesh->vertices[idx0 * 3], mesh->vertices[idx0 * 3 + 1], mesh->vertices[idx0 * 3 + 2]);

        if (mesh->normals) {
            glNormal3f(mesh->normals[idx1 * 3], mesh->normals[idx1 * 3 + 1], mesh->normals[idx1 * 3 + 2]);
        }
        if (mesh->uv) {
            glTexCoord2f(mesh->uv[idx1 * 2], mesh->uv[idx1 * 2 + 1]);
        }
        if (mesh->col) {
            glColor4f(r * mesh->col[idx1 * 4] / 255.0f, g * mesh->col[idx1 * 4 + 1] / 255.0f, b * mesh->col[idx1 * 4 + 2] / 255.0f, a * mesh->col[idx1 * 4 + 3] / 255.0f);
        }
        glVertex3f(mesh->vertices[idx1 * 3], mesh->vertices[idx1 * 3 + 1], mesh->vertices[idx1 * 3 + 2]);

        if (mesh->normals) {
            glNormal3f(mesh->normals[idx2 * 3], mesh->normals[idx2 * 3 + 1], mesh->normals[idx2 * 3 + 2]);
        }
        if (mesh->uv) {
            glTexCoord2f(mesh->uv[idx2 * 2], mesh->uv[idx2 * 2 + 1]);
        }
        if (mesh->col) {
            glColor4f(r * mesh->col[idx2 * 4] / 255.0f, g * mesh->col[idx2 * 4 + 1] / 255.0f, b * mesh->col[idx2 * 4 + 2] / 255.0f, a * mesh->col[idx2 * 4 + 3] / 255.0f);
        }
        glVertex3f(mesh->vertices[idx2 * 3], mesh->vertices[idx2 * 3 + 1], mesh->vertices[idx2 * 3 + 2]);
    }
    glEnd();
    
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
}

int main(int argc, char* argv[]) {

    chdir("/Users/skmp/projects/DreamExportTest");


    // Initialize SDL with video support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return -1;
    }
    
    // Request an OpenGL 2.1 context (sufficient for simple rendering)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    
    // Create the SDL window with an OpenGL context
    SDL_Window* window = SDL_CreateWindow("SDL Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        printf("OpenGL context creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // Enable VSync
    SDL_GL_SetSwapInterval(1);
    
    // Set up the OpenGL viewport and projection
    glViewport(0, 0, 800, 600);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0 / 600.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);

    // Enable backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Enable lighting and configure the directional light (GL_LIGHT0)
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat ambientLight[]  = {0.4f, 0.4f, 0.4f, 1.0f};
    GLfloat diffuseLight[]  = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat specularLight[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
    
    // Enable color material so glColor calls affect the material properties
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Capture the mouse for relative movement
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    // Camera variables
    float camX = 0.0f, camY = 0.0f, camZ = 5.0f;
    float yaw = 0.0f, pitch = 0.0f;
    const float moveSpeed = 0.15f;
    const float mouseSensitivity = 0.1f;
    
    // --- Main loop ---
    bool running = true;
    while (running) {
        SDL_Event event;
        // Process SDL events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
            else if (event.type == SDL_MOUSEMOTION) {
                // Update camera orientation based on relative mouse movement
                yaw   -= event.motion.xrel * mouseSensitivity;
                pitch -= event.motion.yrel * mouseSensitivity;
                // Clamp pitch to avoid flipping
                if (pitch > 89.0f)  pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;
            }
        }
        
        // Process keyboard state for WASD movement
        const Uint8* state = SDL_GetKeyboardState(NULL);
        // Calculate forward vector from yaw (in radians)
        // Convert yaw and pitch from degrees to radians.
        float radYaw = -yaw * (M_PI / 180.0f);
        float radPitch = pitch * (M_PI / 180.0f);

        // Calculate the forward vector with pitch included.
        float forwardX = cos(radPitch) * sin(radYaw);
        float forwardY = sin(radPitch);
        float forwardZ = -cos(radPitch) * cos(radYaw);

        // Calculate the right vector (only horizontal movement).
        float rightX = cos(radYaw);
        float rightZ = sin(radYaw);

        if (state[SDL_SCANCODE_W]) {
            camX += forwardX * moveSpeed;
            camY += forwardY * moveSpeed; // Move vertically based on pitch.
            camZ += forwardZ * moveSpeed;
        }
        if (state[SDL_SCANCODE_S]) {
            camX -= forwardX * moveSpeed;
            camY -= forwardY * moveSpeed;
            camZ -= forwardZ * moveSpeed;
        }
        if (state[SDL_SCANCODE_A]) {
            camX -= rightX * moveSpeed;
            camZ -= rightZ * moveSpeed;
        }
        if (state[SDL_SCANCODE_D]) {
            camX += rightX * moveSpeed;
            camZ += rightZ * moveSpeed;
        }


        
        // Clear the screen and depth buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        
        // Set the directional light position based on the light game object.
        // We assume the light's forward vector is stored in the third column of its transform.
        // Invert it so that the light comes from the object's facing direction.
        // matrix_t* lightTransform = UnityEngine_GameObject_48640.transform;
        // GLfloat lightDir[4] = { -lightTransform->m02, -lightTransform->m12, -lightTransform->m22, 0.0f };
        // glLightfv(GL_LIGHT0, GL_POSITION, lightDir);
        
        // Apply camera transformations: first rotate then translate
        glRotatef(-pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-yaw,   0.0f, 1.0f, 0.0f);
        glTranslatef(-camX, -camY, -camZ);
        
        // Render all game objects
        for (auto gameObject : gameObjects)
            if (gameObject->active && gameObject->mesh_enabled  && gameObject->material->a == 1.0f)
                renderGameObject(gameObject);

        // Render all game objects
        for (auto gameObject : gameObjects)
        if (gameObject->active && gameObject->mesh_enabled  && gameObject->material->a != 1.0f)
            renderGameObject(gameObject);
        
        // Swap the OpenGL buffers
        SDL_GL_SwapWindow(window);
    }
    
    // Cleanup SDL and OpenGL context
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}