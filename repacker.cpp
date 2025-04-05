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
#include <sstream>
#include <iomanip>
#include <cassert>

#include "dcue/types.h"
#include <sys/stat.h>

#include "vendor/crypto/sha256.h"
#include "vendor/dca3/vq.h"
#include "vendor/dca3/tex-util.h"

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
void loadDreamScene(const char *scene) {
    std::ifstream in(scene, std::ios::binary);
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
        size_t size = width * height * 4;
        void* texdata = malloc(size);
        in.read(reinterpret_cast<char*>(texdata), size);
        texture_t* tex = new texture_t(fileStr, width, height, texdata);
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

int main(int argc, const char** argv) {
    if (argc != 2) {
        std::cout << argv[0] << " <scene.dat>" << std::endl;
        return 1;
    }

    loadDreamScene(argv[1]);

    mkdir("repack-data", 0755);

    for (auto& tex: textures) {
        hash_sha256 hash;
        hash.sha256_init();
        hash.sha256_update((const uint8_t*)tex->data, tex->height * tex->width * 4);
        sha256_type hash_result = hash.sha256_final();

        std::stringstream ss;
        ss << "repack-data/tex_";
        for (int h = 0; h < hash_result.size(); h++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash_result[h];
        }
        ss << ".tga";

        auto imageData = createImageFromData_ARGB8888((const uint8_t*)tex->data, tex->width, tex->height, tex->width * 4);
        writeTGA(ss.str().c_str(), imageData, tex->width, tex->height, 32);
    }
}