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
#include <algorithm>

#include <map>
#include <unordered_map>
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

#include "vendor/hmm/heightmap.h"
#include "vendor/hmm/triangulator.h"

#include <meshoptimizer.h>

#define texconvf(...) printf(__VA_ARGS__)

using namespace import;

//----------------------------------------------------------
// Internal geometry types

struct Vec3 {
    float x, y, z;
    Vec3 operator-(const Vec3 &other) const {
        return { x - other.x, y - other.y, z - other.z };
    }
};

struct Triangle {
    int i0, i1, i2;
};

struct Quad {
    int i0, i1, i2, i3;
	unsigned submesh;
};

// An edge defined by two vertex indices (stored in sorted order)
struct Edge {
    int a, b;
    Edge(int v0, int v1) {
        if (v0 < v1) { a = v0; b = v1; }
        else { a = v1; b = v0; }
    }
    bool operator==(const Edge &other) const {
        return a == other.a && b == other.b;
    }
};

struct EdgeHash {
    std::size_t operator()(const Edge &e) const {
        return std::hash<int>()(e.a) ^ std::hash<int>()(e.b);
    }
};

// Helper type for 2D points used in the projected plane.
struct Point2 {
    float x, y;
    int idx; // original vertex index; -1 indicates a new vertex
};

//----------------------------------------------------------
// Basic vector math and geometry utilities

Vec3 Cross(const Vec3 &a, const Vec3 &b) {
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

float Dot(const Vec3 &a, const Vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(const Vec3 &v) {
    return std::sqrt(Dot(v, v));
}

Vec3 Normalize(const Vec3 &v) {
    float len = Length(v);
    return { v.x / len, v.y / len, v.z / len };
}

Vec3 ComputeTriangleNormal(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2) {
    return Normalize(Cross(v1 - v0, v2 - v0));
}

bool AreNormalsSimilar(const Vec3 &n1, const Vec3 &n2, float threshold = 0.95f) {
    return Dot(n1, n2) >= threshold;
}

bool IsConvexQuad(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2, const Vec3 &v3) {
    Vec3 normal = Normalize(Cross(v1 - v0, v2 - v0));
    // Compute cross products for successive edges.
    Vec3 cp0 = Cross(v1 - v0, v2 - v0);
    Vec3 cp1 = Cross(v2 - v1, v3 - v1);
    Vec3 cp2 = Cross(v3 - v2, v0 - v2);
    Vec3 cp3 = Cross(v0 - v3, v1 - v3);
    if (Dot(cp0, normal) < 0 || Dot(cp1, normal) < 0 ||
        Dot(cp2, normal) < 0 || Dot(cp3, normal) < 0)
        return false;
    return true;
}

//----------------------------------------------------------
// 2D Convex Hull using a variant of the Graham Scan

std::vector<Point2> ComputeConvexHull(const std::vector<Point2>& points) {
    if (points.size() < 3)
        return points;
    float cx = 0.f, cy = 0.f;
    for (const auto &pt : points) {
        cx += pt.x; cy += pt.y;
    }
    cx /= points.size(); cy /= points.size();
    std::vector<Point2> sorted = points;
    std::sort(sorted.begin(), sorted.end(), [cx, cy](const Point2 &a, const Point2 &b) {
        float angleA = std::atan2(a.y - cy, a.x - cx);
        float angleB = std::atan2(b.y - cy, b.x - cx);
        return angleA < angleB;
    });
    std::vector<Point2> hull;
    for (const auto &pt : sorted) {
        while (hull.size() >= 2) {
            const Point2 &a = hull[hull.size()-2];
            const Point2 &b = hull[hull.size()-1];
            float crossVal = (b.x - a.x) * (pt.y - b.y) - (b.y - a.y) * (pt.x - b.x);
            if (crossVal <= 0)
                hull.pop_back();
            else
                break;
        }
        hull.push_back(pt);
    }
    return hull;
}

//----------------------------------------------------------
// New: Compute an inscribed quad from a convex polygon in 2D
// using a simple heuristic by shooting rays at 0, 90, 180, and 270 degrees.
std::vector<Point2> ComputeInscribedQuadFromHull(const std::vector<Point2>& hull) {
    std::vector<Point2> result;
    if (hull.empty())
        return result;
    float sumX = 0.f, sumY = 0.f;
    for (const auto &pt : hull) { sumX += pt.x; sumY += pt.y; }
    float cx = sumX / hull.size(), cy = sumY / hull.size();
    std::vector<float> angles = {0.f, 1.5708f, 3.14159f, 4.71239f};
    for (float theta : angles) {
        float dx = std::cos(theta), dy = std::sin(theta);
        float tMin = 1e9; Point2 best; bool found = false;
        for (size_t i = 0; i < hull.size(); i++) {
            const Point2 &A = hull[i], &B = hull[(i+1)%hull.size()];
            float Ax = A.x, Ay = A.y, Bx = B.x, By = B.y;
            float Ex = Bx - Ax, Ey = By - Ay;
            float det = dx * Ey - dy * Ex;
            if (std::abs(det) < 1e-6f)
                continue;
            float t = ((Ax - cx) * Ey - (Ay - cy) * Ex) / det;
            float s = (std::abs(Ex) > std::abs(Ey)) ? (cx + t * dx - Ax) / Ex : (cy + t * dy - Ay) / Ey;
            if (t >= 0 && s >= 0 && s <= 1) {
                if (t < tMin) {
                    tMin = t; best.x = cx + t*dx; best.y = cy + t*dy; best.idx = -1; found = true;
                }
            }
        }
        if (found)
            result.push_back(best);
    }
    return result;
}

//----------------------------------------------------------
// Helper: Compute quad area (by splitting into two triangles)
float QuadArea(const Quad &quad, const std::vector<Vec3> &vertices) {
    Vec3 v0 = vertices[quad.i0], v1 = vertices[quad.i1],
         v2 = vertices[quad.i2], v3 = vertices[quad.i3];
    float area1 = 0.5f * Length(Cross(v1 - v0, v2 - v0));
    float area2 = 0.5f * Length(Cross(v2 - v0, v3 - v0));
    return area1 + area2;
}

Vec3 ComputeQuadNormal(const Quad &quad, const std::vector<Vec3> &vertices) {
    Vec3 v0 = vertices[quad.i0], v1 = vertices[quad.i1], v2 = vertices[quad.i2];
    return ComputeTriangleNormal(v0, v1, v2);
}

//----------------------------------------------------------
// Grouping by principal axis bucket: ±x, ±y, ±z.
enum PrincipalAxis { PLUS_X, MINUS_X, PLUS_Y, MINUS_Y, PLUS_Z, MINUS_Z };

PrincipalAxis GetPrincipalAxis(const Vec3 &n) {
    float ax = std::abs(n.x), ay = std::abs(n.y), az = std::abs(n.z);
    if (ax >= ay && ax >= az)
        return (n.x >= 0.f) ? PLUS_X : MINUS_X;
    else if (ay >= ax && ay >= az)
        return (n.y >= 0.f) ? PLUS_Y : MINUS_Y;
    else
        return (n.z >= 0.f) ? PLUS_Z : MINUS_Z;
}

//----------------------------------------------------------
// Updated MergeQuads: Attempt to merge two quads. This routine
// projects the union of vertices (from both quads) onto the plane of the first quad.
// If the convex hull does not yield exactly four 2D points, it computes an inscribed quad,
// adding new vertices as needed.
bool MergeQuads(const Quad &q1, const Quad &q2, std::vector<Vec3> &vertices, Quad &mergedQuad) {
    std::vector<int> q1Verts = { q1.i0, q1.i1, q1.i2, q1.i3 };
    std::vector<int> q2Verts = { q2.i0, q2.i1, q2.i2, q2.i3 };
    bool shareEdge = false;
    for (int i = 0; i < 4 && !shareEdge; i++) {
        int a = q1Verts[i], b = q1Verts[(i+1)%4];
        for (int j = 0; j < 4; j++) {
            int c = q2Verts[j], d = q2Verts[(j+1)%4];
            if ((a == c && b == d) || (a == d && b == c)) { shareEdge = true; break; }
        }
    }
    if (!shareEdge)
        return false;
    
    std::vector<int> allIndices = { q1.i0, q1.i1, q1.i2, q1.i3, q2.i0, q2.i1, q2.i2, q2.i3 };
    std::vector<int> uniqueIndices;
    for (int idx : allIndices) {
        if (std::find(uniqueIndices.begin(), uniqueIndices.end(), idx) == uniqueIndices.end())
            uniqueIndices.push_back(idx);
    }
    
    Vec3 origin = vertices[q1.i0];
    Vec3 p1 = vertices[q1.i1], p2 = vertices[q1.i2];
    Vec3 planeNormal = ComputeTriangleNormal(origin, p1, p2);
    Vec3 arbitrary = { 1.f, 0.f, 0.f };
    if (std::abs(Dot(planeNormal, arbitrary)) > 0.99f)
        arbitrary = { 0.f, 1.f, 0.f };
    Vec3 u = Normalize(Cross(planeNormal, arbitrary));
    Vec3 v = Normalize(Cross(planeNormal, u));
    
    std::vector<Point2> pts2D;
    for (int idx : uniqueIndices) {
        Vec3 p = vertices[idx];
        Vec3 diff = { p.x - origin.x, p.y - origin.y, p.z - origin.z };
        float x = Dot(diff, u), y = Dot(diff, v);
        pts2D.push_back({ x, y, idx });
    }
    
    std::vector<Point2> hull = ComputeConvexHull(pts2D);
    if (hull.size() == 4) {
        mergedQuad.i0 = hull[0].idx;
        mergedQuad.i1 = hull[1].idx;
        mergedQuad.i2 = hull[2].idx;
        mergedQuad.i3 = hull[3].idx;
    } else {
        std::vector<Point2> inscribed = ComputeInscribedQuadFromHull(hull);
        if (inscribed.size() != 4)
            return false;
        int idx0 = static_cast<int>(vertices.size());
        vertices.push_back({ origin.x + inscribed[0].x * u.x + inscribed[0].y * v.x,
                               origin.y + inscribed[0].x * u.y + inscribed[0].y * v.y,
                               origin.z + inscribed[0].x * u.z + inscribed[0].y * v.z });
        int idx1 = static_cast<int>(vertices.size());
        vertices.push_back({ origin.x + inscribed[1].x * u.x + inscribed[1].y * v.x,
                               origin.y + inscribed[1].x * u.y + inscribed[1].y * v.y,
                               origin.z + inscribed[1].x * u.z + inscribed[1].y * v.z });
        int idx2 = static_cast<int>(vertices.size());
        vertices.push_back({ origin.x + inscribed[2].x * u.x + inscribed[2].y * v.x,
                               origin.y + inscribed[2].x * u.y + inscribed[2].y * v.y,
                               origin.z + inscribed[2].x * u.z + inscribed[2].y * v.z });
        int idx3 = static_cast<int>(vertices.size());
        vertices.push_back({ origin.x + inscribed[3].x * u.x + inscribed[3].y * v.x,
                               origin.y + inscribed[3].x * u.y + inscribed[3].y * v.y,
                               origin.z + inscribed[3].x * u.z + inscribed[3].y * v.z });
        mergedQuad.i0 = idx0; mergedQuad.i1 = idx1; mergedQuad.i2 = idx2; mergedQuad.i3 = idx3;
    }
    
    Vec3 A = vertices[mergedQuad.i0],
         B = vertices[mergedQuad.i1],
         C = vertices[mergedQuad.i2],
         D = vertices[mergedQuad.i3];
    if (!IsConvexQuad(A, B, C, D))
        return false;
    Vec3 n = ComputeTriangleNormal(A, B, C);
    float d = -(n.x * A.x + n.y * A.y + n.z * A.z);
    const float tol = 1e-4f;
    for (int idx : { mergedQuad.i0, mergedQuad.i1, mergedQuad.i2, mergedQuad.i3 }) {
        float dist = std::abs(n.x * vertices[idx].x + n.y * vertices[idx].y + n.z * vertices[idx].z + d);
        if (dist > tol)
            return false;
    }
    return true;
}

//----------------------------------------------------------
// Structure to hold the final result: quads per submesh and the vertex list.
struct QuadMesh {
    std::vector<std::vector<Quad>> quadsPerSubmesh;
    std::vector<Vec3> vertices;
};

//----------------------------------------------------------
// Updated CompactVertices: Now we first collect only the vertex indices referenced
// by any quad and then reindex/compact only those vertices (merging nearly identical ones).
struct VertexKey {
    int x, y, z;
    bool operator==(const VertexKey &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey &key) const {
        return std::hash<int>()(key.x) ^ (std::hash<int>()(key.y) << 1) ^ (std::hash<int>()(key.z) << 2);
    }
};

void CompactVertices(QuadMesh &qm, float epsilon = 1e-6f) {
    // First, determine which vertices are referenced by any quad.
    std::set<int> usedIndices;
    for (const auto &subQuads : qm.quadsPerSubmesh) {
        for (const auto &q : subQuads) {
            usedIndices.insert(q.i0);
            usedIndices.insert(q.i1);
            usedIndices.insert(q.i2);
            usedIndices.insert(q.i3);
        }
    }
    
    // Build new vertex list from only these indices.
    std::unordered_map<VertexKey, int, VertexKeyHash> uniqueMap;
    std::unordered_map<int, int> newIndexMapping; // maps old index -> new index
    std::vector<Vec3> newVertices;
    
    // Iterate only over used indices.
    for (int i : usedIndices) {
        const Vec3 &p = qm.vertices[i];
        VertexKey key = { static_cast<int>(std::round(p.x / epsilon)),
                          static_cast<int>(std::round(p.y / epsilon)),
                          static_cast<int>(std::round(p.z / epsilon)) };
        auto it = uniqueMap.find(key);
        if (it != uniqueMap.end()) {
            newIndexMapping[i] = it->second;
        } else {
            int newIdx = static_cast<int>(newVertices.size());
            uniqueMap[key] = newIdx;
            newVertices.push_back(p);
            newIndexMapping[i] = newIdx;
        }
    }
    
    // Update quad indices.
    for (auto &subQuads : qm.quadsPerSubmesh) {
        for (auto &q : subQuads) {
            q.i0 = newIndexMapping[q.i0];
            q.i1 = newIndexMapping[q.i1];
            q.i2 = newIndexMapping[q.i2];
            q.i3 = newIndexMapping[q.i3];
        }
    }
    
    qm.vertices = newVertices;
}

//----------------------------------------------------------
// New: Filter the final quads so that the overall union of vertices is capped at 32.
// We select quads (largest first) so that the union of their vertices does not exceed 32.
void FilterMaxVertexCount(QuadMesh &qm, size_t maxVertexCount = 32) {
    std::vector<Quad> allQuads;
    for (const auto &sub : qm.quadsPerSubmesh) {
        for (const auto &q : sub)
            allQuads.push_back(q);
    }
    std::sort(allQuads.begin(), allQuads.end(), [&](const Quad &a, const Quad &b) {
         return QuadArea(a, qm.vertices) > QuadArea(b, qm.vertices);
    });
    std::vector<Quad> selected;
    std::set<int> unionIndices;
    for (const Quad &q : allQuads) {
         std::set<int> candidate = unionIndices;
         candidate.insert(q.i0);
         candidate.insert(q.i1);
         candidate.insert(q.i2);
         candidate.insert(q.i3);
         if (candidate.size() <= maxVertexCount) {
             selected.push_back(q);
             unionIndices = candidate;
         }
    }
    qm.quadsPerSubmesh.clear();
    qm.quadsPerSubmesh.push_back(selected);
    CompactVertices(qm);
}

//----------------------------------------------------------
// Main utility function:
// 1. Generate conservative quads from the input mesh_t (including merging and new vertex creation)
// 2. Group quads by principal axis buckets and keep top 10 per bucket.
// 3. Reindex (compact) the vertex list so only used, unique vertices remain.
// 4. Finally, if the overall vertex count exceeds 32, filter out (cut off) quads,
//    keeping only the largest ones until the union of vertices is ≤ 32.
QuadMesh GenerateConservativeQuads(const mesh_t* mesh) {
    QuadMesh result;
    result.vertices.resize(mesh->vertex_count);
    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        result.vertices[i] = { mesh->vertices[i*3],
                               mesh->vertices[i*3+1],
                               mesh->vertices[i*3+2] };
    }
    
    // Process each submesh.
    for (size_t s = 0; s < mesh->submesh_count; s++) {
        const submesh_t &sub = mesh->submeshes[s];
        std::vector<Triangle> triangles;
        for (size_t i = 0; i < sub.index_count; i += 3) {
            triangles.push_back({ 
                static_cast<int>(sub.indices[i]),
                static_cast<int>(sub.indices[i+1]),
                static_cast<int>(sub.indices[i+2])
            });
        }
        
        std::unordered_map<Edge, std::vector<int>, EdgeHash> edgeToTriangles;
        for (size_t i = 0; i < triangles.size(); i++) {
            const Triangle &tri = triangles[i];
            Edge edges[3] = { Edge(tri.i0, tri.i1), Edge(tri.i1, tri.i2), Edge(tri.i2, tri.i0) };
            for (int j = 0; j < 3; j++)
                edgeToTriangles[edges[j]].push_back(static_cast<int>(i));
        }
        
        std::vector<bool> usedTriangle(triangles.size(), false);
        std::vector<Quad> quads;
        for (const auto &entry : edgeToTriangles) {
            const Edge &edge = entry.first;
            const std::vector<int> &tris = entry.second;
            if (tris.size() != 2)
                continue;
            int t0 = tris[0], t1 = tris[1];
            if (usedTriangle[t0] || usedTriangle[t1])
                continue;
            const Triangle &tri0 = triangles[t0];
            const Triangle &tri1 = triangles[t1];
            int free0 = (tri0.i0 != edge.a && tri0.i0 != edge.b) ? tri0.i0 :
                        (tri0.i1 != edge.a && tri0.i1 != edge.b) ? tri0.i1 : tri0.i2;
            int free1 = (tri1.i0 != edge.a && tri1.i0 != edge.b) ? tri1.i0 :
                        (tri1.i1 != edge.a && tri1.i1 != edge.b) ? tri1.i1 : tri1.i2;
            Vec3 n0 = ComputeTriangleNormal(result.vertices[tri0.i0],
                                             result.vertices[tri0.i1],
                                             result.vertices[tri0.i2]);
            Vec3 n1 = ComputeTriangleNormal(result.vertices[tri1.i0],
                                             result.vertices[tri1.i1],
                                             result.vertices[tri1.i2]);
            if (!AreNormalsSimilar(n0, n1))
                continue;
            Quad candidate1 = { free0, edge.a, free1, edge.b };
            Quad candidate2 = { free0, edge.b, free1, edge.a };
            bool valid = false;
            Quad finalQuad;
            {
                Vec3 A = result.vertices[candidate1.i0],
                     B = result.vertices[candidate1.i1],
                     C = result.vertices[candidate1.i2],
                     D = result.vertices[candidate1.i3];
                if (IsConvexQuad(A, B, C, D)) {
                    finalQuad = candidate1;
                    valid = true;
                }
            }
            if (!valid) {
                Vec3 A = result.vertices[candidate2.i0],
                     B = result.vertices[candidate2.i1],
                     C = result.vertices[candidate2.i2],
                     D = result.vertices[candidate2.i3];
                if (IsConvexQuad(A, B, C, D)) {
                    finalQuad = candidate2;
                    valid = true;
                }
            }
            if (valid) {
                quads.push_back(finalQuad);
                usedTriangle[t0] = usedTriangle[t1] = true;
            }
        }
        
        bool mergedAny = true;
        while (mergedAny) {
            mergedAny = false;
            std::vector<Quad> newQuads;
            std::vector<bool> used(quads.size(), false);
            for (size_t i = 0; i < quads.size(); i++) {
                if (used[i])
                    continue;
                bool merged = false;
                for (size_t j = i+1; j < quads.size(); j++) {
                    if (used[j])
                        continue;
                    Quad candidate;
                    if (MergeQuads(quads[i], quads[j], result.vertices, candidate)) {
                        newQuads.push_back(candidate);
                        used[i] = used[j] = true;
                        merged = true;
                        mergedAny = true;
                        break;
                    }
                }
                if (!merged && !used[i])
                    newQuads.push_back(quads[i]);
            }
            quads = newQuads;
        }
        
        std::vector<Quad> filteredQuads;
        std::vector<std::pair<Quad, float>> buckets[6];
        for (const Quad &q : quads) {
            Vec3 qn = ComputeQuadNormal(q, result.vertices);
            PrincipalAxis pa = GetPrincipalAxis(qn);
            float area = QuadArea(q, result.vertices);
            buckets[pa].push_back({q, area});
        }
        for (int i = 0; i < 6; i++) {
            std::sort(buckets[i].begin(), buckets[i].end(), [](const std::pair<Quad, float>& a, const std::pair<Quad, float>& b) {
                return a.second > b.second;
            });
            size_t keepCount = std::min(buckets[i].size(), size_t(10));
            for (size_t j = 0; j < keepCount; j++) {
                filteredQuads.push_back(buckets[i][j].first);
            }
        }
        result.quadsPerSubmesh.push_back(filteredQuads);
    }
    
    CompactVertices(result);
    
    if (true /*result.vertices.size() > 32*/) {
        std::vector<Quad> allQuads;
		// submesh
        for (unsigned i = 0; i < result.quadsPerSubmesh.size(); i++) {
			const auto &sub = result.quadsPerSubmesh[i];
            for (const auto &q : sub) {
                allQuads.push_back(q);
				allQuads.back().submesh = i;
			}
        }
        std::sort(allQuads.begin(), allQuads.end(), [&](const Quad &a, const Quad &b) {
             return QuadArea(a, result.vertices) > QuadArea(b, result.vertices);
        });
        std::vector<Quad> selected;
        std::set<int> unionIndices;
        for (const Quad &q : allQuads) {
             std::set<int> candidate = unionIndices;
             candidate.insert(q.i0);
             candidate.insert(q.i1);
             candidate.insert(q.i2);
             candidate.insert(q.i3);
             if (candidate.size() <= 32) {
                 selected.push_back(q);
                 unionIndices = candidate;
             }
        }
        result.quadsPerSubmesh.clear();
        result.quadsPerSubmesh.push_back(selected);
		CompactVertices(result);
    }
    
    return result;
}


// Global vectors to store the scene data
std::vector<texture_t*> textures;
std::vector<material_t*> materials;
std::vector<mesh_t*> meshes;
std::vector<terrain_t*> terrains;
std::vector<game_object_t*> gameObjects;
size_t skybox[6];
RGBAf skyboxTint;

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
    if (std::string(header, 8) != "DCUE0006") {
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
		uint8_t mode;
		in.read(reinterpret_cast<char*>(&mode), sizeof(mode));

        float a, r, g, b;
        in.read(reinterpret_cast<char*>(&a), sizeof(float));
        in.read(reinterpret_cast<char*>(&r), sizeof(float));
        in.read(reinterpret_cast<char*>(&g), sizeof(float));
        in.read(reinterpret_cast<char*>(&b), sizeof(float));

		float ea, er, eg, eb;
		in.read(reinterpret_cast<char*>(&ea), sizeof(float));
        in.read(reinterpret_cast<char*>(&er), sizeof(float));
        in.read(reinterpret_cast<char*>(&eg), sizeof(float));
        in.read(reinterpret_cast<char*>(&eb), sizeof(float));

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
        material_t* mat = new material_t(mode, a, r, g, b, ea, er, eg, eb, tex);
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

		int subMeshCount = 0;
    	in.read(reinterpret_cast<char*>(&subMeshCount), sizeof(int));
		int logicalSubMeshCount = 0;
    	in.read(reinterpret_cast<char*>(&logicalSubMeshCount), sizeof(int));
		submesh_t* submeshes = new submesh_t[subMeshCount + logicalSubMeshCount];
		for (int subMeshNum = 0; subMeshNum < subMeshCount + logicalSubMeshCount; subMeshNum++) {
			// Indices
			int indexCount = 0;
			in.read(reinterpret_cast<char*>(&indexCount), sizeof(int));
			uint16_t* indices = new uint16_t[indexCount];
			in.read(reinterpret_cast<char*>(indices), indexCount * sizeof(uint16_t));

			submeshes[subMeshNum].index_count = indexCount;
			submeshes[subMeshNum].indices = indices;
		}
        mesh_t* mesh = new mesh_t(subMeshCount, logicalSubMeshCount, submeshes, vertexCount, vertices, uvs, colors, normals);
        meshes.push_back(mesh);
    }

	// --- Terrain Section ---
	int terrainCount = 0;
    in.read(reinterpret_cast<char*>(&terrainCount), sizeof(int));
	for (int i = 0; i < terrainCount; i++) {
		float sizex,sizey,sizez;

		in.read(reinterpret_cast<char*>(&sizex), sizeof(float));
		in.read(reinterpret_cast<char*>(&sizey), sizeof(float));
		in.read(reinterpret_cast<char*>(&sizez), sizeof(float));

		float tscalex, tscaley;
		in.read(reinterpret_cast<char*>(&tscalex), sizeof(float));
		in.read(reinterpret_cast<char*>(&tscaley), sizeof(float));

		float toffsetx, toffsety;
		in.read(reinterpret_cast<char*>(&toffsetx), sizeof(float));
		in.read(reinterpret_cast<char*>(&toffsety), sizeof(float));

		int width, height;
		in.read(reinterpret_cast<char*>(&width), sizeof(int));
		in.read(reinterpret_cast<char*>(&height), sizeof(int));

		float* tdata = new float[width*height];
		in.read(reinterpret_cast<char*>(tdata), width*height*sizeof(float));
		terrain_t* terrain = new terrain_t(sizex,sizey,sizez, tscalex, tscaley, toffsetx, toffsety, width, height, tdata);

		terrains.push_back(terrain);
	}

    // --- Game Objects Section ---
    int gameObjectCount = 0;
    in.read(reinterpret_cast<char*>(&gameObjectCount), sizeof(int));
    for (int i = 0; i < gameObjectCount; i++) {
        // Active state
        char activeChar = 0;
        in.read(&activeChar, 1);
        bool active = (activeChar != 0);

		char movableChar = 0;
        in.read(&movableChar, 1);
        bool movable = (movableChar != 0);

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
        int materialsLength = 0;
		in.read(reinterpret_cast<char*>(&materialsLength), sizeof(int));
        bool hasMaterial = (materialsLength != 0);
        material_t** materials_lst = nullptr;
        if (hasMaterial) {
			materials_lst = new material_t*[materialsLength];
			for (int materialNum = 0; materialNum < materialsLength; materialNum++) {
            	int materialIndex = 0;
				in.read(reinterpret_cast<char*>(&materialIndex), sizeof(int));
				if (materialIndex >= 0 && materialIndex < static_cast<int>(materials.size()))
					materials_lst[materialNum] = materials[materialIndex];
				else
					materials_lst[materialNum] = nullptr;
			}
        }

		terrain_t* terrain = nullptr;
		{
			int terrainIndex = 0;
			in.read(reinterpret_cast<char*>(&terrainIndex), sizeof(int));
			if (terrainIndex >= 0 && terrainIndex < static_cast<int>(terrains.size()))
				terrain = terrains[terrainIndex];
		}

		material_t* terrain_material = nullptr;
		{
			int materialIndex = 0;
			in.read(reinterpret_cast<char*>(&materialIndex), sizeof(int));
			if (materialIndex >= 0 && materialIndex < static_cast<int>(materials.size()))
				terrain_material = materials[materialIndex];
		}

		if (terrain) {
			assert(!materials_lst);
			assert(!mesh);

			auto hmm = std::make_shared<Heightmap>(terrain->width, terrain->height, std::vector<float>(terrain->tdata, terrain->tdata + terrain->width* terrain->height));
			Triangulator tri(hmm);

			tri.Run(0.02, 0, 0);
        	auto points = tri.Points(1);
        	auto triangles = tri.Triangles();
			

			int vertexCount = points.size();
			assert(vertexCount < 65535);
			float* vertices = new float[vertexCount * 3];

			float* uvs = new float[vertexCount * 2];
			for (int i = 0; i < vertexCount; i++) {
				uvs[i * 2 + 0] = points[i].x / terrain->width * terrain->tscalex + terrain->toffsetx;
				uvs[i * 2 + 1] = points[i].y / terrain->height * terrain->tscaley + terrain->toffsety;
			}

			for (int i = 0; i < vertexCount; i++) {
				float nx = points[i].x / terrain->width * terrain->sizex;
				float nz = (terrain->height-points[i].y) / terrain->height * terrain->sizez;
				float ny = points[i].z * terrain->sizey;
				vertices[i * 3 + 0] = points[i].x = nx;
				vertices[i * 3 + 1] =points[i].y = ny;
				vertices[i * 3 + 2] =points[i].z = nz;
			}

			std::vector<glm::vec3> normals(points.size(), glm::vec3(0.0f));

			// Calculate face normals and accumulate them for each vertex.
			for(const auto &tri : triangles) {
				// Get the vertices of the triangle.
				glm::vec3 v0 = points[tri.x];
				glm::vec3 v1 = points[tri.y];
				glm::vec3 v2 = points[tri.z];

				// Calculate edge vectors.
				glm::vec3 edge1 = v1 - v0;
				glm::vec3 edge2 = v2 - v0;

				// Compute the face normal using cross product and normalize it.
				glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

				// Accumulate the face normal into each vertex normal.
				normals[tri.x] += faceNormal;
				normals[tri.y] += faceNormal;
				normals[tri.z] += faceNormal;
			}

			// Normalize the accumulated vertex normals.
			for (auto &normal : normals) {
				normal = glm::normalize(normal);
			}

			float* normals_ptr = new float[vertexCount * 3];

			for (int i = 0; i < vertexCount; i++) {
				normals_ptr[i * 3 + 0] = normals[i].x;
				normals_ptr[i * 3 + 1] = normals[i].y;
				normals_ptr[i * 3 + 2] = normals[i].z;
			}


			int indexCount = triangles.size() * 3;
			uint16_t* indices = new uint16_t[indexCount];
			
			for (int i = 0; i < triangles.size(); i++) {
				indices[i * 3 + 0] = triangles[i].x;
				indices[i * 3 + 1] = triangles[i].y;
				indices[i * 3 + 2] = triangles[i].z;
			}
			submesh_t* submeshes = new submesh_t[1];
			submeshes[0].index_count = indexCount;
			submeshes[0].indices = indices;

			mesh = new mesh_t(1, 0, submeshes, vertexCount, vertices, uvs, nullptr, normals_ptr);
			meshes.push_back(mesh);

			materials_lst = new material_t*[1];
			materials_lst[0] = terrain_material;

			meshEnabled = true; // TODO: Fixme
		}

        game_object_t* go = new game_object_t(active, movable, transform, meshEnabled, mesh, materials_lst);
        gameObjects.push_back(go);
    }

	for (int i = 0; i < 6; i++) {
		int skyboxTextureNum = 0;
		in.read(reinterpret_cast<char*>(&skyboxTextureNum), sizeof(int));
		skybox[i] = skyboxTextureNum;
	}
	in.read(reinterpret_cast<char*>(&skyboxTint.alpha), sizeof(skyboxTint.alpha));
	in.read(reinterpret_cast<char*>(&skyboxTint.red), sizeof(skyboxTint.red));
	in.read(reinterpret_cast<char*>(&skyboxTint.green), sizeof(skyboxTint.green));
	in.read(reinterpret_cast<char*>(&skyboxTint.blue), sizeof(skyboxTint.blue));

	assert(!in.bad());
    in.close();
    printf("Loaded %d textures, %d materials, %d meshes, %d terrains, %d game objects.\n",
           static_cast<int>(textures.size()), static_cast<int>(materials.size()),
           static_cast<int>(meshes.size()), static_cast<int>(terrains.size()),
		   static_cast<int>(gameObjects.size()));
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
	std::vector<uint8_t> quadData;
	std::vector<uint8_t> data;
};

compressed_mesh_t process_mesh(mesh_t *mesh) {
	using namespace triangle_stripper;
	
	int32 n = mesh->submesh_count + mesh->logical_submesh_count;
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
	for (int submeshNum = 0; submeshNum < n; submeshNum++)
	{
		auto submesh = &mesh->submeshes[submeshNum];
		totalTrilist += submesh->index_count;

		for (size_t i = 0; i < submesh->index_count; i++) {
			submesh->indices[i] = canonicalIdx[submesh->indices[i]];
		}
#if 0
		if (submesh->index_count > 1500)
		{
			float target_error = 0.1f;
			float threshold = 1500.0f/submesh->index_count;
			auto& src = *submesh;
			size_t ic = src.index_count;

			// build input index vector
			std::vector<unsigned int> inIndices(ic);
			for (size_t j = 0; j < ic; ++j)
				inIndices[j] = src.indices[j];

			// prepare output buffer (max size = ic)
			std::vector<unsigned int> outIndices(ic);
			float lod_error = 0.f;

			// target index count = floor(ic * threshold)
			size_t target_ic = size_t(ic * threshold);
			if (target_ic < 3) target_ic = 3; // at least one triangle

			// run the simplifier
			size_t new_ic = meshopt_simplify(
				outIndices.data(),
				inIndices.data(),
				ic,
				mesh->vertices,
				mesh->vertex_count,
				sizeof(float) * 3,
				target_ic,
				target_error,
				&lod_error
			);

			// shrink to actual size
			outIndices.resize(new_ic);

			// allocate and copy back to uint16_t indices
			submesh->index_count = new_ic;
			free(submesh->indices);
			submesh->indices     = (uint16_t*)malloc(sizeof(uint16_t)*new_ic);
			for (size_t j = 0; j < new_ic; ++j)
				submesh->indices[j] = uint16_t(outIndices[j]);

			texconvf("Submesh %u: %zu→%zu indices (error=%f)\n", submeshNum, ic, new_ic, lod_error);
		}
		#endif
		{
			indices Indices(submesh->indices, submesh->indices + submesh->index_count);

			tri_stripper TriStripper(Indices);

			TriStripper.SetMinStripSize(0);
			TriStripper.SetCacheSize(0);
			TriStripper.SetBackwardSearch(true);

			TriStripper.Strip(&pvecs[submeshNum]);
		}

		for (auto &&strip: pvecs[submeshNum]) {
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

	QuadMesh qm = GenerateConservativeQuads(mesh);

	write_vector quadDataDesc;
	write_vector quadDataVertex;
	write_vector quadDataIndex;
	assert(qm.quadsPerSubmesh.size() == 1);
	auto quadDescSize = qm.quadsPerSubmesh.size() * 2 * 2;
	auto quadIndexSize = 0;
	for (auto& quads: qm.quadsPerSubmesh) {
		quadIndexSize += quads.size() * 5 * 2;
	}
	if (quadIndexSize & 3) {
		quadIndexSize += 2;
	}

	auto quadVertexOffset = quadIndexSize + quadDescSize;
	
	// this is broken and only works for one global quad group, not per submesh
	assert(qm.quadsPerSubmesh.size() == 1);
	for (auto& quads: qm.quadsPerSubmesh) {
		uint32_t offset = quadDescSize + quadDataIndex.size();
		assert(offset <= INT16_MAX);
		uint32_t size = quads.size();
		assert(size <= INT8_MAX);

		uint32_t vertexCount = qm.vertices.size();
		assert(vertexCount <= INT8_MAX);

		quadDataDesc.write<int16_t>(offset);
		quadDataDesc.write<int8_t>(size);
		quadDataDesc.write<int8_t>(vertexCount);

		for (auto& quad: quads) {
			uint32_t offset0 = quad.i0 * 3 * 4;
			uint32_t offset1 = quad.i1 * 3 * 4;
			uint32_t offset2 = quad.i2 * 3 * 4;
			uint32_t offset3 = quad.i3 * 3 * 4;

			assert(offset0 <= INT16_MAX);
			assert(offset1 <= INT16_MAX);
			assert(offset2 <= INT16_MAX);
			assert(offset3 <= INT16_MAX);
			quadDataIndex.write<int16_t>(offset0);
			quadDataIndex.write<int16_t>(offset1);
			quadDataIndex.write<int16_t>(offset2);
			quadDataIndex.write<int16_t>(offset3);

			assert(quad.submesh <= INT16_MAX);
			quadDataIndex.write<int16_t>(quad.submesh);
		}
	}
	if (quadDataIndex.size() & 3) {
		quadDataIndex.write<int16_t>(0); // padding
	}

	for (auto& vtx: qm.vertices) {
		quadDataVertex.write<float>(vtx.x);
		quadDataVertex.write<float>(vtx.y);
		quadDataVertex.write<float>(vtx.z);
	}

	assert(quadDataDesc.size() == quadDescSize);
	assert(quadDataIndex.size() == quadIndexSize);
	assert((quadDescSize & 3) == 0);
	assert((quadIndexSize & 3) == 0);

	std::vector<uint8_t> quadData(quadDataDesc.size() + quadDataIndex.size() + quadDataVertex.size());
	memcpy(quadData.data(), quadDataDesc.data(), quadDataDesc.size());
	memcpy(quadData.data() + quadDataDesc.size(), quadDataVertex.data(), quadDataVertex.size());
	memcpy(quadData.data() + quadDataDesc.size() + quadDataVertex.size(), quadDataIndex.data(), quadDataIndex.size());
	texconvf("Quad Data Size: %lu\n", quadData.size());
    return { boundingSphere, quadData, meshlets };
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

        std::cout << "Processing texture: " << pvr_filename << " " << tex->file << std::endl;

        if (!std::filesystem::exists(pvr_filename)) {
            auto imageData = createImageFromData_ARGB8888((const uint8_t*)tex->data, tex->width, tex->height, tex->width * 4);
            auto nw = std::min(128, tex->width/2);
            if (tex->width < 16) {
                nw = tex->width;
			}
            auto nh = std::min(128, tex->height/2);
            if (tex->height < 16) {
                nh = tex->height;
            }
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
		for (int submeshNum = 0; submeshNum < mesh->submesh_count + mesh->logical_submesh_count; submeshNum++) {
        	hash.sha256_update((uint8_t*)mesh->submeshes[submeshNum].indices, mesh->submeshes[submeshNum].index_count * sizeof(uint16_t));
		}
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

            mesh_file.write("DCUENM02", 8);
            mesh_file.write((const char*)&compressed_mesh.bounding_sphere, sizeof(compressed_mesh.bounding_sphere));
			uint32_t tmp = compressed_mesh.quadData.size();
			mesh_file.write((const char*)&tmp, sizeof(tmp));
			mesh_file.write((const char*)compressed_mesh.quadData.data(), compressed_mesh.quadData.size());
            mesh_file.write((const char*)compressed_mesh.data.data(), compressed_mesh.data.size());
        }

		if (native_meshes_map.find(hash_result) == native_meshes_map.end()) {
			auto mesh_file = std::ifstream(mesh_filename);
			char tag[9] = { 0 };
			mesh_file.read(tag, 8);
			if (memcmp(tag, "DCUENM02", 8) != 0) {
				std::cout << "Unexpeted mesh tag " << tag << std::endl;
				return 1;
			}
			compressed_mesh_t mesh;

			mesh_file.seekg(0, std::ios::end);
			auto mesh_size = (size_t)mesh_file.tellg() - 8;
			mesh_file.seekg(8, std::ios::beg);

			
			mesh_file.read((char*)&mesh.bounding_sphere, sizeof(mesh.bounding_sphere));
			uint32_t quadDataSize;
			mesh_file.read((char*)&quadDataSize, sizeof(quadDataSize));
			mesh.quadData.resize(quadDataSize);
			mesh_file.read((char*)mesh.quadData.data(), mesh.quadData.size());
			
			mesh.data.resize(mesh_size - sizeof(mesh.bounding_sphere) - quadDataSize);
			mesh_file.read((char*)mesh.data.data(), mesh.data.size());

			native_meshes_map[hash_result] = native_meshes.size();
			native_meshes.push_back(mesh);
		}
        native_meshes_index[mesh] = native_meshes_map[hash_result];
    }

	std::cout << "Writting out scene " << argv[2] << std::endl;

	auto outfile = std::ofstream(argv[2]);

    outfile.write("DCUENS05", 8);
    
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
		outfile.write((char*)&material->mode, sizeof(material->mode));
		outfile.write((char*)&material->a, sizeof(material->a));
		outfile.write((char*)&material->r, sizeof(material->r));
		outfile.write((char*)&material->g, sizeof(material->g));
		outfile.write((char*)&material->b, sizeof(material->b));
		outfile.write((char*)&material->ea, sizeof(material->ea));
		outfile.write((char*)&material->er, sizeof(material->er));
		outfile.write((char*)&material->eg, sizeof(material->eg));
		outfile.write((char*)&material->eb, sizeof(material->eb));
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
		tmp = native_mesh.quadData.size();
		outfile.write((char*)&tmp, sizeof(tmp));
		tmp = native_mesh.data.size();
		outfile.write((char*)&tmp, sizeof(tmp));
		outfile.write((char*)&native_mesh.bounding_sphere, sizeof(native_mesh.bounding_sphere));
		outfile.write((char*)native_mesh.quadData.data(), native_mesh.quadData.size());
		outfile.write((char*)native_mesh.data.data(), native_mesh.data.size());
	}

	tmp = (uint32_t)gameObjects.size();
	outfile.write((char*)&tmp, sizeof(tmp));
	for (auto& gameObject: gameObjects) {
		outfile.write((char*)&gameObject->active, sizeof(gameObject->active));
		outfile.write((char*)&gameObject->movable, sizeof(gameObject->movable));
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

		tmp = gameObject->mesh ? gameObject->mesh->submesh_count : 0;
		outfile.write((char*)&tmp, sizeof(tmp));
		if (gameObject->mesh) {
			for (int materialNum = 0; materialNum < gameObject->mesh->submesh_count; materialNum++) {
				if (gameObject->materials[materialNum]) {
					tmp = native_materials_index[gameObject->materials[materialNum]];
				} else {
					tmp = UINT32_MAX;
				}
				outfile.write((char*)&tmp, sizeof(tmp));
			}
		}
	}

	for (int i = 0; i < 6; i++) {
		tmp = native_textures_index[textures[skybox[i]]];
		outfile.write((char*)&tmp, sizeof(tmp));
	}
	outfile.write(reinterpret_cast<char*>(&skyboxTint.alpha), sizeof(skyboxTint.alpha));
	outfile.write(reinterpret_cast<char*>(&skyboxTint.red), sizeof(skyboxTint.red));
	outfile.write(reinterpret_cast<char*>(&skyboxTint.green), sizeof(skyboxTint.green));
	outfile.write(reinterpret_cast<char*>(&skyboxTint.blue), sizeof(skyboxTint.blue));

	return 0;


}
