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

#include <map>
#include <set>
#include <list>

#include <filesystem>

#include "dcue/types-import.h"
#include "dcue/types-native.h"

#include <sys/stat.h>

#include "vendor/crypto/sha256.h"
#include "vendor/dca3/vq.h"
#include "vendor/dca3/tex-util.h"
#include "vendor/TriStripper/include/tri_stripper.h"

#include "vendor/dca3/float16.h"

#define texconvf(...) printf(__VA_ARGS__)

using namespace import;

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


bool isDegenerate(const V3d& v1, const V3d& v2, const V3d& v3) {
    V3d u = {v2.x - v1.x, v2.y - v1.y, v2.z - v1.z};
    V3d v = {v3.x - v1.x, v3.y - v1.y, v3.z - v1.z};
	V3d crs = cross(u, v);
	if (length(crs) < 0.0000001f) {
		return true;
	} else {
		return false;
	}
}

bool isDegenerateByIndex(uint16_t idx1, uint16_t idx2, uint16_t idx3) {
	return idx1 == idx2 || idx1 == idx3 || idx2 == idx3;
}
static int8_t packFloat8(float f) {
	auto rounded =  lroundf(f * 127);

	assert(rounded >= -127 && rounded <= 127);

	return static_cast<int8_t>(rounded);
}

static int16_t packFloat16(float f) {
	auto rounded =  lroundf(f * 32767);

	assert(rounded >= -32767 && rounded <= 32767);

	return static_cast<int16_t>(rounded);
}
struct write_vector: std::vector<uint8_t> {
	template<typename T>
	void write(const T& val) {
		const uint8_t* p = (const uint8_t*)&val;
		insert(end(), p, p + sizeof(T));
	}

	template<typename T>
	void rewrite(size_t offset, const T& val) {
		const uint8_t* p = (const uint8_t*)&val;
		std::copy(p, p + sizeof(T), begin() + offset);
	}

	void packVertex(Sphere* volume, V3d* vertex, TexCoords* texcoord, V3d* normal, RGBA* color, bool big_vertex, bool pad_xyz, bool big_uv) {
		if (big_vertex) {
			write<float>(vertex->x);
			write<float>(vertex->y);
			write<float>(vertex->z);
		} else {
			write<int16_t>(packFloat16((vertex->x - volume->center.x) / volume->radius));
			write<int16_t>(packFloat16((vertex->y - volume->center.y) / volume->radius));
			write<int16_t>(packFloat16((vertex->z - volume->center.z) / volume->radius));
			
			if (pad_xyz) {
				write<int16_t>(0);
			}
		}

		if (texcoord) {
			if (big_uv) {
				float16 u = texcoord->u;
				float16 v = texcoord->v;
				write<uint32_t>((u.raw << 16) | v.raw);
			} else {
				write<int8_t>(lroundf(texcoord->u * 127));
				write<int8_t>(lroundf(texcoord->v * 127));
			}
		}

		if (normal) {
			auto normaly = normalize(*normal);
			if (length(*normal) < 0.0001) {
				normaly.x = 1;
				normaly.y = 0;
				normaly.z = 0;
				printf("*WARNING* invalid normal\n");
			}
			
			int8_t nxi = lroundf(normaly.x * 127);
			int8_t nyi = lroundf(normaly.y * 127);
			int8_t nzi = lroundf(normaly.z * 127);
			
			V3d normal2 = { static_cast<float>(nxi), static_cast<float>(nyi), static_cast<float>(nzi) };

			assert(fabs(normaly.x-normal2.x/127) < 0.05f);
			assert(fabs(normaly.y-normal2.y/127) < 0.05f);
			assert(fabs(normaly.z-normal2.z/127) < 0.05f);

			write<int8_t>(nxi);
			write<int8_t>(nyi);
			write<int8_t>(nzi);
			write<int8_t>(0);
		}
		
		if (color) {
			write<uint32_t>((color->blue ^ 128) | ((color->green ^ 128) << 8) | ((color->red ^ 128) << 16) | ((color->alpha ^ 128) << 24));
		}
	}
};

bool isBigVertex(const V3d& vtx) {
	return vtx.x > 127 || vtx.y > 127 || vtx.z > 127 || vtx.x < -127 || vtx.y < -127 || vtx.z < -127;
}

bool isBigUV(const TexCoords& uv) {
	return uv.u > 1 || uv.v > 1 || uv.u < -1 || uv.v < -1;
}

void adjustFlagsForAlingment(bool textured, bool colored, bool& big_vertex, bool& big_uv, bool& pad_xyz) {
	pad_xyz = false;

	if (textured) {
		if (big_vertex && !big_uv) {
			big_uv = true;
		}

		if (!big_vertex && big_uv) {
			pad_xyz = true;
		}
	}
}

unsigned caluclateVertexAlignment(bool textured, bool normaled, bool colored, bool big_vertex, bool big_uv, bool pad_xyz) {
	if (big_vertex) {
		return 4;
	} else if (textured && big_uv) {
		return 4;
	} else {
		return 2;
	}
}
unsigned caluclateVertexSize(bool textured, bool normaled, bool colored, bool big_vertex, bool big_uv, bool pad_xyz) {
	uint32_t vertexBytes = 0; //xyz

	if (big_vertex) {
		vertexBytes += 4 * 3;
	} else {
		vertexBytes += 2 * 3;

		if (pad_xyz) {
			vertexBytes += 2;
		}
	}

	if (textured) {
		if (big_uv) {
			vertexBytes += 4;
		} else {
			vertexBytes += 2;
		}
	}

	if (normaled) {
		vertexBytes += 4;
	}

	if (colored) {
		vertexBytes += 4;
	}

	return vertexBytes;
}

Sphere calculateBoundingSphere(V3d* vertexData, size_t count) {
	Sphere sphere;
	sphere.center = {0, 0, 0};
	sphere.radius = 0;

	for (size_t i = 0; i < count; i++) {
		sphere.center = add(sphere.center, vertexData[i]);
	}

	sphere.center.x /= count;
	sphere.center.y /= count;
	sphere.center.z /= count;

	for (size_t i = 0; i < count; i++) {
		sphere.radius = std::max(sphere.radius, length(sub(vertexData[i], sphere.center)));
	}

	return sphere;
}
struct meshlet {
	std::set<uint16_t> vertices;
	std::map<uint16_t, uint8_t> vertexToLocalIndex;
	std::vector<triangle_stripper::primitive_group*> strips;
	size_t vertexDataOffset;
	size_t indexDataOffset;
	size_t skinIndexDataOffset;
	size_t skinWeightDataOffset;
	size_t rewriteOffsetVDO;
	size_t rewriteOffsetIDO;
	size_t rewriteOffsetSIDO;
	size_t rewriteOffsetSWDO;

	bool isOfBigVertex(V3d* vertexData, Sphere* volume) {
		for (auto v : vertices) {
			if (isBigVertex(sub(vertexData[v], volume->center))) {
				return true;
			}
		}
		return false;
	}

	bool isOfBigUV(TexCoords* uvData) {
		for (auto v : vertices) {
			if (isBigUV(uvData[v])) {
				return true;
			}
		}
		return false;
	}

	Sphere calculateBoundingSphere(V3d* vertexData) {
		Sphere sphere;
		sphere.center = {0, 0, 0};
		sphere.radius = 0;

		for (auto v : vertices) {
			sphere.center = add(sphere.center, vertexData[v]);
		}

		sphere.center.x /= vertices.size();
		sphere.center.y /= vertices.size();
		sphere.center.z /= vertices.size();

		for (auto v : vertices) {
			sphere.radius = std::max(sphere.radius, length(sub(vertexData[v], sphere.center)));
		}

		return sphere;
	}
};
struct compressed_mesh_t {
	Sphere bounding_sphere;
	std::vector<uint8_t> data;
};

compressed_mesh_t process_mesh(mesh_t *mesh) {
	using namespace triangle_stripper;
	
	int32 n = 1;
	std::vector<primitive_vector> pvecs(n);
	std::vector<std::vector<meshlet>> meshMeshlets(n);

	size_t totalIndices = 0, strips = 0,  totalTrilist = 0;

	bool colored = mesh->col != nullptr;
	bool normaled = mesh->normals != nullptr;
	bool texcoorded = mesh->uv != nullptr;
	
	Sphere boundingSphere = calculateBoundingSphere((V3d*)mesh->vertices, mesh->vertex_count);

	auto vertices = (V3d*)mesh->vertices;
	auto normals = (V3d*)mesh->normals;
	auto texcoords = (TexCoords*)mesh->uv;
	auto colors = (RGBA*)mesh->col;

	std::vector<size_t> canonicalIdx(mesh->vertex_count, SIZE_MAX);
	for (size_t i = 0; i < mesh->vertex_count; i++) {
		for (size_t j = i+1; j < mesh->vertex_count; j++) {
			bool duplicate = true;
			if (vertices[i].x != vertices[j].x || vertices[i].y != vertices[j].y || vertices[i].z != vertices[j].z) {
				duplicate = false;
			}

			if (texcoorded && (texcoords[i].u != texcoords[j].u || texcoords[i].v != texcoords[j].v)) {
				duplicate = false;
			}
			if (normaled && (normals[i].x != normals[j].x || normals[i].y != normals[j].y || normals[i].z != normals[j].z)) {
				duplicate = false;
			}
			if (colored && (colors[i].red != colors[j].red || colors[i].green != colors[j].green || colors[i].blue != colors[j].blue || colors[i].alpha != colors[j].alpha)) {
				duplicate = false;
			}
			if (duplicate) {
				// texconvf("Vertex %zu: %.2f %.2f %.2f deemed dup with %zu: %.2f %.2f %.2f\n", i, vertices[i].x, vertices[i].y, vertices[i].z, j, vertices[j].x, vertices[j].y, vertices[j].z);
				if(canonicalIdx[i] == SIZE_MAX) {
					if (canonicalIdx[j] != SIZE_MAX) {
						canonicalIdx[i] = canonicalIdx[j];
						// texconvf("Warning: Duplicate Chain vertex %zu and %zu\n", i, j);
					} else {
						canonicalIdx[i] = i;
						canonicalIdx[j] = i;
						// texconvf("Warning: Duplicate vertex %zu and %zu\n", i, j);
					}
				} else {
					canonicalIdx[j] = canonicalIdx[i];
					// texconvf("Warning: Duplicate Double vertex %zu and %zu\n", i, j);
				}
			}
		}
		if(canonicalIdx[i] == SIZE_MAX) {
			canonicalIdx[i] = i;
		}
	}

	size_t dups = 0;
	for (size_t i = 0; i < mesh->vertex_count; i++) {
		if (canonicalIdx[i] != i) {
			dups++;
		}
	}
	texconvf("Found %zu vertex duplicates, %.2f%%\n", dups, (float)dups/mesh->vertex_count*100);
	{
        size_t meshNum = 0;
		totalTrilist += mesh->index_count;

		for (size_t i = 0; i < mesh->index_count; i++) {
			mesh->indices[i] = canonicalIdx[mesh->indices[i]];
		}

		{
			indices Indices(mesh->indices, mesh->indices + mesh->index_count);

			tri_stripper TriStripper(Indices);

			TriStripper.SetMinStripSize(0);
			TriStripper.SetCacheSize(0);
			TriStripper.SetBackwardSearch(true);

			TriStripper.Strip(&pvecs[meshNum]);
		}

		for (auto &&strip: pvecs[meshNum]) {
			totalIndices += strip.Indices.size();
			if (strip.Type == TRIANGLES) {
				assert(strip.Indices.size()%3==0);
				strips += strip.Indices.size()/3;
			} else {
				strips ++;
			}
		}
	}
	#undef printf
	texconvf("Down to %ld strips (%.2f), %ld indices from %ld (vtx: %ld)\n", strips, (float)totalIndices/strips, totalIndices, totalTrilist, mesh->vertex_count);

	// construct meshlets
	
	size_t meshIndexesCount = 0;
	size_t meshVerticesCount = 0;
	size_t meshletIndexesCount = 0;
	size_t meshletVerticesCount = 0;
	for (int pvn = 0; pvn < pvecs.size(); pvn++) {
		auto &&prims = pvecs[pvn];

		std::set<uint16_t> meshletVertices;
		std::vector<primitive_group*> meshletStrips;

		std::list<primitive_group*> strips;
		for (auto &&strip: prims) {
			strips.push_back(&strip);
		}
		#undef printf

		while(strips.size()) {
			for(;;) {
				// pluck strip with fewest new indices

				primitive_group* bestStrip = nullptr;

				size_t remainingVertices = 128 - meshletVertices.size();
				size_t bestSharedVertices = 0;

				for (auto &&strip_ptr: strips) {
					auto &&strip = *strip_ptr;
					std::set<uint16_t> newVertices;
					size_t sharedVertices = 0;
					for (auto &&idx: strip.Indices) {
						if (meshletVertices.find(idx) == meshletVertices.end()) {
							newVertices.insert(idx);
						} else {
							sharedVertices++;
						}
					}
					if (newVertices.size() == 0) {
						bestStrip = strip_ptr;
						break;
					}
					if (newVertices.size() <= remainingVertices && sharedVertices >= bestSharedVertices) {
						bestStrip = strip_ptr;
						bestSharedVertices = sharedVertices;
					}
				}

				if (bestStrip == nullptr) {
					break;
				}

				// add strip to meshlet
				meshletStrips.push_back(bestStrip);
				for (auto &&idx: bestStrip->Indices) {
					meshletVertices.insert(idx);
				}
				strips.remove(bestStrip);
			}

			assert(meshletStrips.size() != 0);

			// printf("Meshlet constructed, %ld strips, %zu vertices\n", meshletStrips.size(), meshletVertices.size());
			for (auto &&strip: meshletStrips) {
				meshletIndexesCount += strip->Indices.size();
			}
			meshletVerticesCount += meshletVertices.size();

			meshMeshlets[pvn].push_back(meshlet{meshletVertices, {}, meshletStrips, 0, 0});

			uint8_t localIndex = 0;
			for (auto &&idx: meshletVertices) {
				meshMeshlets[pvn].back().vertexToLocalIndex[idx] = localIndex++;
			}

			assert(localIndex <= 128);

			meshletStrips.clear();
			meshletVertices.clear();
		}

		std::set<uint16_t> meshVertices;
		for (auto &&strip: prims) {
			meshIndexesCount += strip.Indices.size();
			for (auto &&idx: strip.Indices) {
				meshVertices.insert(idx);
			}
		}
		meshVerticesCount += meshVertices.size();
	}
	texconvf("%zu; %.2f; Meshlets complete %zu vertices %zu indexes from %zu vertices %zu indexes\n", meshletVerticesCount - meshVerticesCount, (float)(meshletVerticesCount - meshVerticesCount)/meshVerticesCount, meshletVerticesCount, meshletIndexesCount, meshVerticesCount, meshIndexesCount);

	write_vector meshData;
	write_vector meshletData;
	write_vector vertexData;
	write_vector indexData;
	write_vector skinningIndexData;
	write_vector skinningWeightData;

	for (size_t i = 0; i < meshMeshlets.size(); i++) {
		auto &&mesh = meshMeshlets[i];
		
		assert(mesh.size() <= 32767);
		meshData.write<int16_t>(mesh.size());

		assert((meshletData.size() + meshMeshlets.size() * 4) <= 32767);
		meshData.write<int16_t>(meshletData.size() + meshMeshlets.size() * 4);

		for (auto && meshlet: mesh) {
			auto boundingSphere = meshlet.calculateBoundingSphere(vertices);

			uint32_t totalIndexes = 0;
			for(auto&& strip: meshlet.strips) {
				totalIndexes += strip->Indices.size();
			}

			// write out vertex data

			bool big_vertex = meshlet.isOfBigVertex(vertices, &boundingSphere);
			bool big_uv = texcoorded && meshlet.isOfBigUV(texcoords);
			bool pad_xyz;
			adjustFlagsForAlingment(texcoorded, colored, big_vertex, big_uv, pad_xyz);
			uint8_t vertexSize = caluclateVertexSize(texcoorded, normaled, colored, big_vertex, big_uv, pad_xyz);
			uint8_t vertexAlignment = caluclateVertexAlignment(texcoorded, normaled, colored, big_vertex, big_uv, pad_xyz);

			assert(vertexSize % vertexAlignment == 0);
			assert(vertexData.size() % vertexAlignment == 0);

			meshlet.vertexDataOffset = vertexData.size();

			for (auto &&idx: meshlet.vertices) {
				vertexData.packVertex(&boundingSphere, &vertices[idx], texcoorded ? &texcoords[idx] : nullptr, normaled ? &normals[idx] : nullptr, colored ? &colors[idx] : nullptr, big_vertex, pad_xyz, big_uv);
			}

			// write out index data
			meshlet.indexDataOffset = indexData.size();
			
			for(auto&& strip: meshlet.strips) {
				if (strip->Type == TRIANGLES) {
					for (size_t i = 0; i < strip->Indices.size(); i+=3) {
						indexData.write<uint8_t>(meshlet.vertexToLocalIndex[strip->Indices[i]]);
						indexData.write<uint8_t>(meshlet.vertexToLocalIndex[strip->Indices[i+1]]);
						indexData.write<uint8_t>(meshlet.vertexToLocalIndex[strip->Indices[i+2]] | 128);
					}
				} else {
					for (size_t i = 0; i < strip->Indices.size(); i++) {
						indexData.write<uint8_t>(meshlet.vertexToLocalIndex[strip->Indices[i]] | ((i + 1) == strip->Indices.size() ? 128 : 0));
					}
				}
			}

            meshlet.skinIndexDataOffset = SIZE_MAX;
            meshlet.skinWeightDataOffset = SIZE_MAX;

			// write out meshlet data
			meshletData.write(boundingSphere);
			//isTextured, isNormaled, isColored, small_xyz, pad_xyz, small_uv
			uint16_t flags = texcoorded | (normaled << 1) | (colored << 2) | (!big_vertex << 3) | (pad_xyz << 4) | (!big_uv << 5);
			meshletData.write<uint16_t>(flags);
			meshletData.write<uint8_t>(0);
			//bool textured, bool normaled, bool colored, bool big_vertex, bool big_uv, bool pad_xyz
			meshletData.write<uint8_t>(vertexSize);
			assert(meshlet.vertices.size() <= 65535);
			meshletData.write<uint16_t>(meshlet.vertices.size());
			assert(totalIndexes <= 65535);
			meshletData.write<uint16_t>(totalIndexes);
			meshlet.rewriteOffsetVDO = meshletData.size();
			meshletData.write<uint32_t>(meshlet.vertexDataOffset); // will be patched
			meshlet.rewriteOffsetIDO = meshletData.size();
			meshletData.write<uint32_t>(meshlet.indexDataOffset); // will be patched
		}
	}

	assert(skinningIndexData.size() % 2 == 0);

	bool isIdx8 = mesh->vertex_count < 256;

	auto dataSize = meshData.size() + meshletData.size() + vertexData.size() + skinningIndexData.size() + skinningWeightData.size() + indexData.size();

	auto vertexBase = meshData.size() + meshletData.size();
	auto skinIndexBase = vertexBase + vertexData.size();
	auto skinWeightBase = skinIndexBase + skinningIndexData.size();
	assert(skinWeightBase % 2 == 0);
	auto indexBase = skinWeightBase + skinningWeightData.size();

	for (auto&& mesh: meshMeshlets) {
		for (auto&& meshlet: mesh) {
			assert(vertexBase % 4 == 0);
			meshletData.rewrite<uint32_t>(meshlet.rewriteOffsetVDO, meshlet.vertexDataOffset + vertexBase);
			meshletData.rewrite<uint32_t>(meshlet.rewriteOffsetIDO, meshlet.indexDataOffset + indexBase);
		}
	}

    std::vector<uint8_t> meshlets(dataSize);

	uint8_t* dataPtr = meshlets.data();
	memcpy(dataPtr, meshData.data(), meshData.size());
	dataPtr += meshData.size();
	memcpy(dataPtr, meshletData.data(), meshletData.size());
	dataPtr += meshletData.size();
	memcpy(dataPtr, vertexData.data(), vertexData.size());
	dataPtr += vertexData.size();

	memcpy(dataPtr, indexData.data(), indexData.size());
	dataPtr += indexData.size();

	assert(dataPtr - meshlets.data() == dataSize);

    return { boundingSphere, meshlets };
}

int main(int argc, const char** argv) {
    if (argc != 3) {
        std::cout << argv[0] << " <scene.dat> <scene.ndt>" << std::endl;
        return 1;
    }

    loadDreamScene(argv[1]);

    mkdir("repack-data", 0755);
    mkdir("repack-data/tex-tga", 0755);
    mkdir("repack-data/tex", 0755);
    mkdir("repack-data/mesh", 0755);

    std::vector<pvr_tex> native_textures;
	std::map<sha256_type, size_t> native_textures_map;
    std::map<texture_t*, size_t> native_textures_index;

    for (auto& tex: textures) {
        hash_sha256 hash;
        hash.sha256_init();
        hash.sha256_update((const uint8_t*)tex->data, tex->height * tex->width * 4);
        sha256_type hash_result = hash.sha256_final();

        std::stringstream ss;
        ss << "repack-data/tex-tga/";
        for (int h = 0; h < hash_result.size(); h++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash_result[h];
        }
        ss << ".tga";

        auto tga_filename = ss.str();

        std::stringstream ss2;
        ss2 << "repack-data/tex/";
        for (int h = 0; h < hash_result.size(); h++) {
            ss2 << std::hex << std::setw(2) << std::setfill('0') << (int)hash_result[h];
        }
        ss2 << ".pvr";

        auto pvr_filename = ss2.str();

        std::cout << "Processing texture: " << pvr_filename << std::endl;

        if (!std::filesystem::exists(pvr_filename)) {
            auto imageData = createImageFromData_ARGB8888((const uint8_t*)tex->data, tex->width, tex->height, tex->width * 4);
            auto nw = std::min(256, tex->width/2);
            auto nh = std::min(256, tex->height/2);
            assert(nw >= 8);
            assert(nh >= 8);
    
            imageData = downscaleImage(imageData, tex->width, tex->height, nw, nh);
            writeTGA(tga_filename.c_str(), imageData, nw, nh, 32);
    
            std::stringstream encodeCmd;
            encodeCmd << "../vendor/pvrtex/pvrtex -i " << tga_filename << " -o " << pvr_filename << " -c small -d";
    
            std::cout << "Runing: " << encodeCmd.str() << std::endl;
            auto res = system(encodeCmd.str().c_str());

            assert(res == 0);
        }

		if (native_textures_map.find(hash_result) == native_textures_map.end()) {
			native_textures_map[hash_result] = native_textures.size();
			native_textures.push_back(loadPVR(pvr_filename.c_str()));
		}
        native_textures_index[tex] = native_textures_map[hash_result];
    }

	std::vector<compressed_mesh_t> native_meshes;
	std::map<sha256_type, size_t> native_meshes_map;
    std::map<mesh_t*, size_t> native_meshes_index;

    for (auto& mesh: meshes) {
        hash_sha256 hash;
        hash.sha256_init();
        hash.sha256_update((uint8_t*)mesh->vertices, mesh->vertex_count * 3 * sizeof(float));
        hash.sha256_update((uint8_t*)mesh->indices, mesh->index_count * sizeof(uint16_t));
        if (mesh->col) {
            hash.sha256_update((uint8_t*)mesh->col, mesh->vertex_count * 4);
        }
        if (mesh->uv) {
            hash.sha256_update((uint8_t*)mesh->uv, mesh->vertex_count * 2 * sizeof(float));
        }
        if (mesh->normals) {
            hash.sha256_update((uint8_t*)mesh->normals, mesh->vertex_count * 3 * sizeof(float));
        }
        sha256_type hash_result = hash.sha256_final();

        std::stringstream ss;
        ss << "repack-data/mesh/";
        for (int h = 0; h < hash_result.size(); h++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash_result[h];
        }
        ss << ".native";

        auto mesh_filename = ss.str();

        std::cout << "Processing mesh: " << mesh_filename << std::endl;

        if (!std::filesystem::exists(mesh_filename)) {
            auto compressed_mesh = process_mesh(mesh);
            auto mesh_file = std::ofstream(mesh_filename);

            mesh_file.write("DCUENM00", 8);
            mesh_file.write((const char*)&compressed_mesh.bounding_sphere, sizeof(compressed_mesh.bounding_sphere));
            mesh_file.write((const char*)compressed_mesh.data.data(), compressed_mesh.data.size());
        }

		if (native_meshes_map.find(hash_result) == native_meshes_map.end()) {
			auto mesh_file = std::ifstream(mesh_filename);
			char tag[9] = { 0 };
			mesh_file.read(tag, 8);
			if (memcmp(tag, "DCUENM00", 8) != 0) {
				std::cout << "Unexpeted mesh tag " << tag << std::endl;
				return 1;
			}
			compressed_mesh_t mesh;

			mesh_file.seekg(0, std::ios::end);
			auto mesh_size = (size_t)mesh_file.tellg() - 8;
			mesh_file.seekg(8, std::ios::beg);

			mesh.data.resize(mesh_size - sizeof(mesh.bounding_sphere));

			mesh_file.read((char*)&mesh.bounding_sphere, sizeof(mesh.bounding_sphere));
			mesh_file.read((char*)mesh.data.data(), mesh.data.size());

			native_meshes_map[hash_result] = native_meshes.size();
			native_meshes.push_back(mesh);
		}
        native_meshes_index[mesh] = native_meshes_map[hash_result];
    }

	auto outfile = std::ofstream(argv[2]);

    outfile.write("DCUENS00", 8);
    
	uint32_t tmp;

	tmp = (uint32_t)native_textures.size();
	outfile.write((char*)&tmp, sizeof(tmp));
	for(auto& native_tex: native_textures) {
		tmp = (uint32_t)native_tex.data.size();
		outfile.write((char*)&tmp, sizeof(tmp));

		outfile.write((char*)&native_tex.flags, sizeof(native_tex.flags));
		outfile.write((char*)&native_tex.offs, sizeof(native_tex.offs));
		outfile.write((char*)&native_tex.lw, sizeof(native_tex.lw));
		outfile.write((char*)&native_tex.lh, sizeof(native_tex.lh));

		outfile.write((char*)native_tex.data.data(), native_tex.data.size());
	}

	std::map<material_t*, size_t> native_materials_index;
	tmp = (uint32_t)materials.size();
	outfile.write((char*)&tmp, sizeof(tmp));
	for (auto& material: materials) {
		outfile.write((char*)&material->a, sizeof(material->a));
		outfile.write((char*)&material->r, sizeof(material->r));
		outfile.write((char*)&material->g, sizeof(material->g));
		outfile.write((char*)&material->b, sizeof(material->b));
		if (material->texture) {
			tmp = native_textures_index[material->texture];
		} else {
			tmp = UINT32_MAX;
		}
		outfile.write((char*)&tmp, sizeof(tmp));
		native_materials_index[material] = native_materials_index.size();
	}

	tmp = (uint32_t)native_meshes.size();
	outfile.write((char*)&tmp, sizeof(tmp));
	for (auto& native_mesh: native_meshes) {
		tmp = native_mesh.data.size();
		outfile.write((char*)&tmp, sizeof(tmp));
		outfile.write((char*)&native_mesh.bounding_sphere, sizeof(native_mesh.bounding_sphere));
		outfile.write((char*)native_mesh.data.data(), native_mesh.data.size());
	}

	tmp = (uint32_t)gameObjects.size();
	outfile.write((char*)&tmp, sizeof(tmp));
	for (auto& gameObject: gameObjects) {
		outfile.write((char*)&gameObject->active, sizeof(gameObject->active));
		native::r_matrix_t ltw = {
			gameObject->transform->m00, gameObject->transform->m10, gameObject->transform->m20, gameObject->transform->m30,
			gameObject->transform->m01, gameObject->transform->m11, gameObject->transform->m21, gameObject->transform->m31,
			gameObject->transform->m02, gameObject->transform->m12, gameObject->transform->m22, gameObject->transform->m32,
			gameObject->transform->m03, gameObject->transform->m13, gameObject->transform->m23, gameObject->transform->m33
		};
		outfile.write((char*)&ltw, sizeof(ltw));
		outfile.write((char*)&gameObject->mesh_enabled, sizeof(gameObject->mesh_enabled));
		if (gameObject->mesh) {
			tmp = native_meshes_index[gameObject->mesh];
		} else {
			tmp = UINT32_MAX;
		}
		outfile.write((char*)&tmp, sizeof(tmp));
		if (gameObject->material) {
			tmp = native_materials_index[gameObject->material];
		} else {
			tmp = UINT32_MAX;
		}
		outfile.write((char*)&tmp, sizeof(tmp));
	}

	return 0;


}