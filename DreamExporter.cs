using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;

public class DreamExporter : MonoBehaviour
{
    static bool ArraysEqual<T>(T[] a1, T[] a2)
    {
        if (ReferenceEquals(a1, a2))
            return true;

        if (a1 == null || a2 == null)
            return false;

        if (a1.Length != a2.Length)
            return false;

        var comparer = EqualityComparer<T>.Default;
        for (int i = 0; i < a1.Length; i++)
        {
            if (!comparer.Equals(a1[i], a2[i])) return false;
        }
        return true;
    }

    class DreamTexScaleOffset
    {
        public Vector2 scale;
        public Vector2 offset;
        public bool valid;

        public DreamTexScaleOffset(bool valid, Vector2 scale, Vector2 offset)
        {
            this.valid = valid;
            this.scale = scale;
            this.offset = offset;
        }

        public override int GetHashCode()
        {
            return offset.GetHashCode() ^ scale.GetHashCode() ^ valid.GetHashCode();
        }
        public override bool Equals(object obj)
        {
            var other = obj as DreamTexScaleOffset;

            if (other != null)
            {
                return valid == other.valid && offset == other.offset && scale == other.scale;
            }

            return false;
        }

        // Overload == and != for consistency
        public static bool operator ==(DreamTexScaleOffset left, DreamTexScaleOffset right)
        {
            if (left is null) return right is null;

            return left.Equals(right);
        }

        public static bool operator !=(DreamTexScaleOffset left, DreamTexScaleOffset right)
        {
            return !(left == right);
        }
    }
    class DreamMesh
    {
        public Mesh mesh;
        public DreamTexScaleOffset[] tso = new DreamTexScaleOffset[0];
        public List<DreamTexScaleOffset> tsoIndexLinear = new List<DreamTexScaleOffset>();
        public Dictionary<DreamTexScaleOffset, List<int>> tsoIndexMateralNum = new Dictionary<DreamTexScaleOffset, List<int>>();

        public DreamMesh(Mesh mesh, Material[] materials)
        {
            if (mesh.subMeshCount != materials.Length)
            {
                throw new Exception("Submeshes != Materials?");
            }

            if (mesh.subMeshCount == 0)
            {
                throw new Exception("mesh.subMeshCount == 0 ?!");
            }

            this.mesh = mesh;
            tso = new DreamTexScaleOffset[materials.Length];

            for (int materialNum = 0; materialNum < materials.Length; materialNum++)
            {
                var material = materials[materialNum];

                if (material == null)
                {
                    tso[materialNum] = new DreamTexScaleOffset(false, Vector2.zero, Vector2.zero);
                    if (!tsoIndexMateralNum.ContainsKey(tso[materialNum]))
                    {
                        tsoIndexLinear.Add(tso[materialNum]);
                        tsoIndexMateralNum[tso[materialNum]] = new List<int>();
                    }
                    tsoIndexMateralNum[tso[materialNum]].Add(materialNum);
                    continue;
                }

                tso[materialNum] = new DreamTexScaleOffset(true, material.mainTextureScale, material.mainTextureOffset);

                if (!tsoIndexMateralNum.ContainsKey(tso[materialNum]))
                {
                    tsoIndexLinear.Add(tso[materialNum]);
                    tsoIndexMateralNum[tso[materialNum]] = new List<int>();
                }
                tsoIndexMateralNum[tso[materialNum]].Add(materialNum);
            }
        }

        // Override GetHashCode to match Equals behavior
        public override int GetHashCode()
        {
            return mesh.GetHashCode();
        }

        public override bool Equals(object obj)
        {
            var other = obj as DreamMesh;

            if (other != null)
            {
                return mesh == other.mesh && ArraysEqual(tso, other.tso);
            }

            return false;
        }

        // Overload == and != for consistency
        public static bool operator ==(DreamMesh left, DreamMesh right)
        {
            if (left is null) return right is null;

            return left.Equals(right);
        }

        public static bool operator !=(DreamMesh left, DreamMesh right)
        {
            return !(left == right);
        }
    }
    class DreamScene
    {
        public HashSet<Material> materials = new HashSet<Material>();
        public HashSet<Texture> textures = new HashSet<Texture>();
        public HashSet<DreamMesh> meshes = new HashSet<DreamMesh>();
        public List<GameObject> gameObjects = new List<GameObject>();
    }

    static Texture2D GetReadableTexture(Texture source)
    {
        // Create a temporary RenderTexture of the same size as the source
        RenderTexture tmp = RenderTexture.GetTemporary(source.width, source.height, 0, RenderTextureFormat.Default, RenderTextureReadWrite.Linear);
        Graphics.Blit(source, tmp);

        // Backup the currently active RenderTexture
        RenderTexture previous = RenderTexture.active;
        RenderTexture.active = tmp;

        // Create a new Texture2D and read the RenderTexture contents into it
        Texture2D readableTexture = new Texture2D(source.width, source.height, TextureFormat.RGBA32, false);
        readableTexture.ReadPixels(new Rect(0, 0, source.width, source.height), 0, 0);
        readableTexture.Apply();

        // Restore previously active RenderTexture and release the temporary one
        RenderTexture.active = previous;
        RenderTexture.ReleaseTemporary(tmp);

        return readableTexture;
    }


    [MenuItem("Dreamcast/Export Scene")]
    static void ExportScene()
    {
        Debug.Log("Collecting ...");
        DreamScene ds = new DreamScene();
        Scene scene = SceneManager.GetActiveScene();
        if (scene != null)
        {
            GameObject[] rootObjects = scene.GetRootGameObjects();
            foreach (GameObject go in rootObjects)
            {
                CollectObject(ds, go);
            }
        }

        // Create lists (for consistent ordering) and dictionaries (for lookups)
        List<Texture> textureList = new List<Texture>(ds.textures);
        List<Material> materialList = new List<Material>(ds.materials);

        List<DreamMesh> meshList = new List<DreamMesh>(ds.meshes);

        Dictionary<Texture, int> textureIndex = new Dictionary<Texture, int>();
        for (int i = 0; i < textureList.Count; i++)
        {
            textureIndex[textureList[i]] = i;
        }
        Dictionary<Material, int> materialIndex = new Dictionary<Material, int>();
        for (int i = 0; i < materialList.Count; i++)
        {
            materialIndex[materialList[i]] = i;
        }
        Dictionary<DreamMesh, int> meshIndex = new Dictionary<DreamMesh, int>();
        for (int i = 0; i < meshList.Count; i++)
        {
            meshIndex[meshList[i]] = i;
        }

        Debug.Log("Exporting ...");
        // Write binary file with header "DCUE0001"
        using (FileStream fs = new FileStream("dream.dat", FileMode.Create))
        using (BinaryWriter writer = new BinaryWriter(fs, Encoding.UTF8))
        {
            // Write header (8 bytes)
            writer.Write(Encoding.ASCII.GetBytes("DCUE0001"));

            // --------------------
            // Write Textures Section
            // --------------------
            writer.Write(textureList.Count);
            foreach (Texture tex in textureList)
            {
                // Write asset path as a length-prefixed string
                string assetPath = AssetDatabase.GetAssetPath(tex);
                writer.Write(assetPath);
                writer.Write(tex.width);
                writer.Write(tex.height);

                // If the texture is a Texture2D, try to get its pixel data.
                Texture2D tex2D = tex as Texture2D;
                if (tex2D != null)
                {
                    // If texture is not readable, create a readable copy.
                    if (!tex2D.isReadable)
                    {
                        tex2D = GetReadableTexture(tex2D);
                    }

                    byte[] rawData = tex2D.GetRawTextureData();
                    if (rawData.Length != tex2D.width * tex2D.height * 4)
                    {
                        throw new Exception("Texture data size mismatch");
                    }
                    writer.Write(rawData);          // Write the pixel data.
                }
                else
                {
                    throw new Exception("Texture is not a Texture2D " + tex.GetType().Name);
                }
            }

            // --------------------
            // Write Materials Section
            // --------------------
            writer.Write(materialList.Count);
            foreach (Material mat in materialList)
            {
                // Write material color components (as floats)
                Color col = mat.color;
                writer.Write(col.a);
                writer.Write(col.r);
                writer.Write(col.g);
                writer.Write(col.b);

                // Write whether the material has a main texture.
                // If so, write the index of that texture.
                bool hasTexture = (mat.mainTexture != null) && textureIndex.ContainsKey(mat.mainTexture);
                writer.Write(hasTexture);
                if (hasTexture)
                {
                    writer.Write(textureIndex[mat.mainTexture]);
                }
            }

            // --------------------
            // Write Meshes Section
            // --------------------
            writer.Write(meshList.Count);
            foreach (DreamMesh dmesh in meshList)
            {
                var mesh = dmesh.mesh;

                Dictionary<int, int>[] base_replicas_decode = new Dictionary<int, int>[dmesh.tso.Length];

                for (int i = 0; i < base_replicas_decode.Length; i++)
                {
                    base_replicas_decode[i] = new Dictionary<int, int>();
                }

                var tso_index_list = dmesh.tsoIndexLinear;

                int replica_base = 0;
                List<Vector3> all_verts = new List<Vector3>();
                List<Vector2> all_uvs = new List<Vector2>();
                List<Color32> all_cols = new List<Color32>();
                List<Vector3> all_normals = new List<Vector3>();

                for (int current_replica = 0; current_replica < tso_index_list.Count; current_replica++)
                {
                    var current_tso = tso_index_list[current_replica];

                    Dictionary<int, int> index_lump = new Dictionary<int, int>();
                    List<int> index_lump_linear = new List<int>();

                    foreach (var submeshNum in dmesh.tsoIndexMateralNum[current_tso])
                    {
                        int[] indices = mesh.GetIndices(submeshNum);
                        foreach (var index in indices)
                        {
                            if (!index_lump.ContainsKey(index))
                            {
                                index_lump_linear.Add(index);
                                index_lump.Add(index, index_lump.Count); // NB this will modify index_lump.Count
                            }

                            if (!base_replicas_decode[submeshNum].ContainsKey(index))
                            {
                                base_replicas_decode[submeshNum].Add(index, replica_base + index_lump[index]);
                            }
                            else
                            {
                                if (base_replicas_decode[submeshNum][index] != replica_base + index_lump[index])
                                {
                                    throw new Exception("Index missmatch");
                                }
                            }
                        }
                    }

                    Debug.Log("Replica: " + current_replica + " replica_base " + replica_base);

                    replica_base += index_lump_linear.Count;

                    // Vertices: write count then each vertex (x, y, z)
                    Vector3[] vertices = mesh.vertices;

                    if (index_lump_linear.Count == 0)
                    {
                        throw new Exception("index_lump_linear.Count == 0");
                    }

                    foreach (var index in index_lump_linear)
                    {
                        var v = vertices[index];
                        all_verts.Add(v);
                    }

                    // TODO: Fix up

                    // UVs: write count then each uv (x, y)
                    Vector2[] uvs = mesh.uv;
                    if (uvs.Length > 0)
                    {
                        foreach (var index in index_lump_linear)
                        {
                            var uv = uvs[index] * current_tso.scale + current_tso.offset;
                            uv.x = -uv.x;
                            all_uvs.Add(uv);
                        }
                    }

                    // Colors: write count then each color (a, r, g, b as bytes)
                    Color32[] colors = mesh.colors32;
                    if (colors.Length > 0)
                    {
                        foreach (var index in index_lump_linear)
                        {
                            var c = colors[index];
                            all_cols.Add(c);
                        }
                    }

                    // Normals: write count then each normal (x, y, z)
                    Vector3[] normals = mesh.normals;
                    if (normals.Length > 0)
                    {
                        foreach (var index in index_lump_linear)
                        {
                            var n = normals[index];
                            all_normals.Add(n);
                        }
                    }

                }

                writer.Write(all_verts.Count);
                foreach (var v in all_verts)
                {
                    writer.Write(v.x);
                    writer.Write(v.y);
                    writer.Write(v.z);
                }
                writer.Write(all_uvs.Count);
                foreach (var uv in all_uvs)
                {
                    writer.Write(uv.x);
                    writer.Write(uv.y);
                }
                writer.Write(all_cols.Count);
                foreach (var c in all_cols)
                {
                    writer.Write(c.a);
                    writer.Write(c.r);
                    writer.Write(c.g);
                    writer.Write(c.b);
                }
                writer.Write(all_normals.Count);
                foreach (var n in all_normals)
                {
                    writer.Write(n.x);
                    writer.Write(n.y);
                    writer.Write(n.z);
                }

                Debug.Log("Replicas: " + base_replicas_decode.Length + " replicated vtx: " + (replica_base - mesh.vertices.Length));

                if (replica_base > 65535)
                {
                    throw new Exception("Mesh vertices count > 65535, replica_base = " + replica_base + " mesh.vertices.Length = " + mesh.vertices.Length);
                }

                writer.Write((int)mesh.subMeshCount);

                var meshvers = mesh.vertices;

                for (int i = 0; i < mesh.subMeshCount; i++)
                {
                    // Indices: write count then each index as a ushort.
                    int[] indices = mesh.GetIndices(i);

                    writer.Write((int)indices.Length);
                    foreach (int index in indices)
                    {
                        if (!base_replicas_decode[i].ContainsKey(index))
                        {
                            throw new Exception("!base_replicas_decode[i].ContainsKey(index)");
                        }
                        var index_dereplicated = base_replicas_decode[i][index];

                        if (meshvers[index] != all_verts[index_dereplicated])
                        {
                            throw new Exception("Vertex missmatch");
                        }
                        writer.Write((ushort)index_dereplicated);
                    }
                }
            }

            // --------------------
            // Write GameObjects Section
            // --------------------
            writer.Write(ds.gameObjects.Count);
            foreach (GameObject go in ds.gameObjects)
            {
                // Write active state (bool)
                writer.Write(go.activeInHierarchy);

                // Write localToWorldMatrix (16 floats)
                Matrix4x4 mtx = go.transform.localToWorldMatrix;
                writer.Write(mtx.m00); writer.Write(mtx.m01);
                writer.Write(mtx.m02); writer.Write(mtx.m03);
                writer.Write(mtx.m10); writer.Write(mtx.m11);
                writer.Write(mtx.m12); writer.Write(mtx.m13);
                writer.Write(mtx.m20); writer.Write(mtx.m21);
                writer.Write(mtx.m22); writer.Write(mtx.m23);
                writer.Write(mtx.m30); writer.Write(mtx.m31);
                writer.Write(mtx.m32); writer.Write(mtx.m33);

                // Check for an attached mesh and material.
                DreamMesh mesh = null;
                Material[] materials = new Material[0];
                bool meshEnabled = false;
                MeshFilter mf = go.GetComponent<MeshFilter>();
                MeshRenderer mr = go.GetComponent<MeshRenderer>();
                if (mf != null && mr != null && mf.sharedMesh != null && mr.sharedMaterials != null)
                {
                    mesh = new DreamMesh(mf.sharedMesh, mr.sharedMaterials);
                    materials = mr.sharedMaterials;
                    meshEnabled = mr.enabled;
                }
                writer.Write(meshEnabled);
                // Write mesh index if mesh exists, else flag false.
                bool hasMesh = (mesh != null) && meshIndex.ContainsKey(mesh);
                writer.Write(hasMesh);
                if (hasMesh)
                {
                    writer.Write(meshIndex[mesh]);
                }
                // Write material index if material exists.
                writer.Write((int)materials.Length);
                foreach (Material material in materials)
                {
                    if (material != null)
                    {
                        writer.Write(materialIndex[material]);
                    }
                    else
                    {
                        writer.Write((int)-1);
                    }
                }
            }
        }

        Debug.Log("Done!");
    }

    static void CollectObject(DreamScene ds, GameObject gameObject)
    {
        ds.gameObjects.Add(gameObject);

        //int compCount = gameObject.GetComponents()
        //for (int i = 0; i < compCount; i++)
        {
            var component = gameObject.GetComponent<MeshFilter>();
            if (component != null && component.GetType() == typeof(MeshFilter))
            {
                MeshFilter meshFilter = (MeshFilter)component;
                Mesh mesh = meshFilter.sharedMesh;
                MeshRenderer meshRenderer = gameObject.GetComponent<MeshRenderer>();
                if (mesh != null && meshRenderer != null)
                {
                    ds.meshes.Add(new DreamMesh(mesh, meshRenderer.sharedMaterials));


                    foreach (Material material in meshRenderer.sharedMaterials)
                    {
                        if (material != null)
                        {
                            ds.materials.Add(material);
                            if (material.mainTexture != null)
                            {
                                if (material.mainTexture is Texture2D)
                                {
                                    ds.textures.Add(material.mainTexture);
                                }
                            }
                        }
                    }
                }
            }
        }

        for (int i = 0; i < gameObject.transform.childCount; i++)
        {
            CollectObject(ds, gameObject.transform.GetChild(i).gameObject);
        }
    }
}
