#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <cassert>

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

    void load(void* texdata)
    {
        assert(id == 0);
        
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texdata);
        
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

// Global vectors to store the scene data
std::vector<texture_t*> textures;
std::vector<material_t*> materials;
std::vector<mesh_t*> meshes;
std::vector<game_object_t*> gameObjects;

// Helper to read a 7-bit encoded int (as written by .NET's BinaryWriter)
int read7BitEncodedInt(std::ifstream &in) {
    int result = 0;
    int bitsRead = 0;
    while (true) {
        uint8_t byte = 0;
        in.read(reinterpret_cast<char*>(&byte), 1);
        result |= (byte & 0x7F) << bitsRead;
        if ((byte & 0x80) == 0)
            break;
        bitsRead += 7;
    }
    return result;
}

// Helper to read a length-prefixed string
std::string readString(std::ifstream &in) {
    int length = read7BitEncodedInt(in);
    std::string str(length, '\0');
    in.read(&str[0], length);
    return str;
}

// Loads the scene from "dream.dat"
void loadDreamScene() {
    std::ifstream in("dream.dat", std::ios::binary);
    if (!in) {
        printf("Failed to open dream.dat\n");
        exit(1);
    }

    // Read and verify header (8 bytes)
    char header[8];
    in.read(header, 8);
    if (std::string(header, 8) != "DCUE0000") {
        printf("Invalid file header\n");
        exit(1);
    }

    // --- Textures Section ---
    int textureCount = 0;
    in.read(reinterpret_cast<char*>(&textureCount), sizeof(int));
    for (int i = 0; i < textureCount; i++) {
        std::string assetPath = readString(in);
        int width = 0, height = 0;
        in.read(reinterpret_cast<char*>(&width), sizeof(int));
        in.read(reinterpret_cast<char*>(&height), sizeof(int));

        // Copy assetPath into a new char array for texture_t
        char* fileStr = new char[assetPath.size() + 1];
        strcpy(fileStr, assetPath.c_str());
        texture_t* tex = new texture_t(fileStr, width, height);
        size_t size = width * height * 4;
        void* texdata = malloc(size);
        in.read(reinterpret_cast<char*>(texdata), size);
        tex->load(texdata);
        free(texdata);
        textures.push_back(tex);
    }

    // --- Materials Section ---
    int materialCount = 0;
    in.read(reinterpret_cast<char*>(&materialCount), sizeof(int));
    for (int i = 0; i < materialCount; i++) {
        float a, r, g, b;
        in.read(reinterpret_cast<char*>(&a), sizeof(float));
        in.read(reinterpret_cast<char*>(&r), sizeof(float));
        in.read(reinterpret_cast<char*>(&g), sizeof(float));
        in.read(reinterpret_cast<char*>(&b), sizeof(float));

        // Read hasTexture flag (bool stored as one byte)
        char hasTextureChar = 0;
        in.read(&hasTextureChar, 1);
        bool hasTexture = (hasTextureChar != 0);
        texture_t* tex = nullptr;
        if (hasTexture) {
            int texIndex = 0;
            in.read(reinterpret_cast<char*>(&texIndex), sizeof(int));
            if (texIndex >= 0 && texIndex < static_cast<int>(textures.size()))
                tex = textures[texIndex];
        }
        material_t* mat = new material_t(a, r, g, b, tex);
        materials.push_back(mat);
    }

    // --- Meshes Section ---
    int meshCount = 0;
    in.read(reinterpret_cast<char*>(&meshCount), sizeof(int));
    for (int i = 0; i < meshCount; i++) {
        // Vertices
        int vertexCount = 0;
        in.read(reinterpret_cast<char*>(&vertexCount), sizeof(int));
        float* vertices = new float[vertexCount * 3];
        in.read(reinterpret_cast<char*>(vertices), vertexCount * 3 * sizeof(float));

        // UVs
        int uvCount = 0;
        in.read(reinterpret_cast<char*>(&uvCount), sizeof(int));
        float* uvs = nullptr;
        if (uvCount > 0) {
            uvs = new float[uvCount * 2];
            in.read(reinterpret_cast<char*>(uvs), uvCount * 2 * sizeof(float));
        }

        // Colors (Color32: 4 bytes per element)
        int colorCount = 0;
        in.read(reinterpret_cast<char*>(&colorCount), sizeof(int));
        uint8_t* colors = nullptr;
        if (colorCount > 0) {
            colors = new uint8_t[colorCount * 4];
            in.read(reinterpret_cast<char*>(colors), colorCount * 4 * sizeof(uint8_t));
        }

        // Normals
        int normalCount = 0;
        in.read(reinterpret_cast<char*>(&normalCount), sizeof(int));
        float* normals = nullptr;
        if (normalCount > 0) {
            normals = new float[normalCount * 3];
            in.read(reinterpret_cast<char*>(normals), normalCount * 3 * sizeof(float));
        }

        // Indices
        int indexCount = 0;
        in.read(reinterpret_cast<char*>(&indexCount), sizeof(int));
        uint16_t* indices = new uint16_t[indexCount];
        in.read(reinterpret_cast<char*>(indices), indexCount * sizeof(uint16_t));

        mesh_t* mesh = new mesh_t(indexCount, indices, vertexCount, vertices, uvs, colors, normals);
        meshes.push_back(mesh);
    }

    // --- Game Objects Section ---
    int gameObjectCount = 0;
    in.read(reinterpret_cast<char*>(&gameObjectCount), sizeof(int));
    for (int i = 0; i < gameObjectCount; i++) {
        // Active state
        char activeChar = 0;
        in.read(&activeChar, 1);
        bool active = (activeChar != 0);

        // Transform matrix (16 floats)
        matrix_t* transform = new matrix_t();
        in.read(reinterpret_cast<char*>(&transform->m00), 16 * sizeof(float));

        // Mesh enabled
        char meshEnabledChar = 0;
        in.read(&meshEnabledChar, 1);
        bool meshEnabled = (meshEnabledChar != 0);

        // Mesh reference
        char hasMeshChar = 0;
        in.read(&hasMeshChar, 1);
        bool hasMesh = (hasMeshChar != 0);
        mesh_t* mesh = nullptr;
        if (hasMesh) {
            int meshIndex = 0;
            in.read(reinterpret_cast<char*>(&meshIndex), sizeof(int));
            if (meshIndex >= 0 && meshIndex < static_cast<int>(meshes.size()))
                mesh = meshes[meshIndex];
        }

        // Material reference
        char hasMaterialChar = 0;
        in.read(&hasMaterialChar, 1);
        bool hasMaterial = (hasMaterialChar != 0);
        material_t* material = nullptr;
        if (hasMaterial) {
            int materialIndex = 0;
            in.read(reinterpret_cast<char*>(&materialIndex), sizeof(int));
            if (materialIndex >= 0 && materialIndex < static_cast<int>(materials.size()))
                material = materials[materialIndex];
        }

        game_object_t* go = new game_object_t(active, transform, meshEnabled, mesh, material);
        gameObjects.push_back(go);
    }

    in.close();
    printf("Loaded %d textures, %d materials, %d meshes, and %d game objects.\n",
           static_cast<int>(textures.size()), static_cast<int>(materials.size()),
           static_cast<int>(meshes.size()), static_cast<int>(gameObjects.size()));
}

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
            glBindTexture(GL_TEXTURE_2D, obj->material->texture->id);
            glEnable(GL_TEXTURE_2D);
        }
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

        if (mesh->normals) {
            glNormal3f(mesh->normals[idx0 * 3], mesh->normals[idx0 * 3 + 1], mesh->normals[idx0 * 3 + 2]);
        }
        if (mesh->uv) {
            glTexCoord2f(mesh->uv[idx0 * 2], mesh->uv[idx0 * 2 + 1]);
        }
        if (mesh->col) {
            glColor4f(r * mesh->col[idx0 * 4] / 255.0f, g * mesh->col[idx0 * 4 + 1] / 255.0f,
                      b * mesh->col[idx0 * 4 + 2] / 255.0f, a * mesh->col[idx0 * 4 + 3] / 255.0f);
        }
        glVertex3f(mesh->vertices[idx0 * 3], mesh->vertices[idx0 * 3 + 1], mesh->vertices[idx0 * 3 + 2]);

        if (mesh->normals) {
            glNormal3f(mesh->normals[idx1 * 3], mesh->normals[idx1 * 3 + 1], mesh->normals[idx1 * 3 + 2]);
        }
        if (mesh->uv) {
            glTexCoord2f(mesh->uv[idx1 * 2], mesh->uv[idx1 * 2 + 1]);
        }
        if (mesh->col) {
            glColor4f(r * mesh->col[idx1 * 4] / 255.0f, g * mesh->col[idx1 * 4 + 1] / 255.0f,
                      b * mesh->col[idx1 * 4 + 2] / 255.0f, a * mesh->col[idx1 * 4 + 3] / 255.0f);
        }
        glVertex3f(mesh->vertices[idx1 * 3], mesh->vertices[idx1 * 3 + 1], mesh->vertices[idx1 * 3 + 2]);

        if (mesh->normals) {
            glNormal3f(mesh->normals[idx2 * 3], mesh->normals[idx2 * 3 + 1], mesh->normals[idx2 * 3 + 2]);
        }
        if (mesh->uv) {
            glTexCoord2f(mesh->uv[idx2 * 2], mesh->uv[idx2 * 2 + 1]);
        }
        if (mesh->col) {
            glColor4f(r * mesh->col[idx2 * 4] / 255.0f, g * mesh->col[idx2 * 4 + 1] / 255.0f,
                      b * mesh->col[idx2 * 4 + 2] / 255.0f, a * mesh->col[idx2 * 4 + 3] / 255.0f);
        }
        glVertex3f(mesh->vertices[idx2 * 3], mesh->vertices[idx2 * 3 + 1], mesh->vertices[idx2 * 3 + 2]);
    }
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

int main(int argc, char* argv[]) {

    // Initialize SDL with video support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return -1;
    }
    
    // Request an OpenGL 2.1 context
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
    
    // Enable depth testing and backface culling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Lighting setup
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat ambientLight[]  = {0.4f, 0.4f, 0.4f, 1.0f};
    GLfloat diffuseLight[]  = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat specularLight[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
    
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Capture the mouse for relative movement
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    // Load the scene from the binary file
    loadDreamScene();
    
    // Camera variables
    float camX = 0.0f, camY = 0.0f, camZ = 5.0f;
    float yaw = 0.0f, pitch = 0.0f;
    const float moveSpeed = 0.15f;
    const float mouseSensitivity = 0.1f;
    
    // --- Main loop ---
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
            else if (event.type == SDL_MOUSEMOTION) {
                yaw   -= event.motion.xrel * mouseSensitivity;
                pitch -= event.motion.yrel * mouseSensitivity;
                if (pitch > 89.0f)  pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;
            }
        }
        
        const Uint8* state = SDL_GetKeyboardState(NULL);
        float radYaw = -yaw * (M_PI / 180.0f);
        float radPitch = pitch * (M_PI / 180.0f);
        float forwardX = cos(radPitch) * sin(radYaw);
        float forwardY = sin(radPitch);
        float forwardZ = -cos(radPitch) * cos(radYaw);
        float rightX = cos(radYaw);
        float rightZ = sin(radYaw);

        if (state[SDL_SCANCODE_W]) {
            camX += forwardX * moveSpeed;
            camY += forwardY * moveSpeed;
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
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        
        glRotatef(-pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-yaw,   0.0f, 1.0f, 0.0f);
        glTranslatef(-camX, -camY, -camZ);
        
        // Render game objects; here we first render opaque then transparent objects
        for (auto gameObject : gameObjects)
            if (gameObject->active && gameObject->mesh_enabled && gameObject->material && gameObject->material->a == 1.0f)
                renderGameObject(gameObject);

        for (auto gameObject : gameObjects)
            if (gameObject->active && gameObject->mesh_enabled && gameObject->material && gameObject->material->a != 1.0f)
                renderGameObject(gameObject);
        
        SDL_GL_SwapWindow(window);
    }
    
    // Cleanup SDL and OpenGL context
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
