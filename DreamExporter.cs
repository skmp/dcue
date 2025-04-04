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
    class DreamScene
    {
        public HashSet<Material> materials = new HashSet<Material>();
        public HashSet<Texture> textures = new HashSet<Texture>();
        public HashSet<Mesh> meshes = new HashSet<Mesh>();
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
        List<Mesh> meshList = new List<Mesh>(ds.meshes);

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
        Dictionary<Mesh, int> meshIndex = new Dictionary<Mesh, int>();
        for (int i = 0; i < meshList.Count; i++)
        {
            meshIndex[meshList[i]] = i;
        }

        Debug.Log("Exporting ...");
        // Write binary file with header "DCUE0000"
        using (FileStream fs = new FileStream("dream.dat", FileMode.Create))
        using (BinaryWriter writer = new BinaryWriter(fs, Encoding.UTF8))
        {
            // Write header (8 bytes)
            writer.Write(Encoding.ASCII.GetBytes("DCUE0000"));

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
                    thow new Exception("Texture is not a Texture2D");
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
            foreach (Mesh mesh in meshList)
            {
                // Vertices: write count then each vertex (x, y, z)
                Vector3[] vertices = mesh.vertices;
                writer.Write(vertices.Length);
                foreach (Vector3 v in vertices)
                {
                    writer.Write(v.x);
                    writer.Write(v.y);
                    writer.Write(v.z);
                }

                // UVs: write count then each uv (x, y)
                Vector2[] uvs = mesh.uv;
                writer.Write(uvs.Length);
                if (uvs.Length > 0)
                {
                    foreach (Vector2 uv in uvs)
                    {
                        writer.Write(uv.x);
                        writer.Write(uv.y);
                    }
                }

                // Colors: write count then each color (a, r, g, b as bytes)
                Color32[] colors = mesh.colors32;
                writer.Write(colors.Length);
                if (colors.Length > 0)
                {
                    foreach (Color32 c in colors)
                    {
                        writer.Write(c.a);
                        writer.Write(c.r);
                        writer.Write(c.g);
                        writer.Write(c.b);
                    }
                }

                // Normals: write count then each normal (x, y, z)
                Vector3[] normals = mesh.normals;
                writer.Write(normals.Length);
                if (normals.Length > 0)
                {
                    foreach (Vector3 n in normals)
                    {
                        writer.Write(n.x);
                        writer.Write(n.y);
                        writer.Write(n.z);
                    }
                }

                // Indices: write count then each index as a ushort.
                int[] indices = mesh.GetIndices(0);
                if (indices.Length > 65535)
                {
                    throw new Exception("Mesh indices count > 65535");
                }
                writer.Write(indices.Length);
                foreach (int index in indices)
                {
                    writer.Write((ushort)index);
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
                Mesh mesh = null;
                Material material = null;
                bool meshEnabled = false;
                MeshFilter mf = go.GetComponent<MeshFilter>();
                MeshRenderer mr = go.GetComponent<MeshRenderer>();
                if (mf != null && mr != null)
                {
                    mesh = mf.sharedMesh;
                    material = mr.sharedMaterial;
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
                bool hasMaterial = (material != null) && materialIndex.ContainsKey(material);
                writer.Write(hasMaterial);
                if (hasMaterial)
                {
                    writer.Write(materialIndex[material]);
                }
            }
        }

        Debug.Log("Done!");
    }

    static void CollectObject(DreamScene ds, GameObject gameObject)
    {
        ds.gameObjects.Add(gameObject);

        int compCount = gameObject.GetComponentCount();
        for (int i = 0; i < compCount; i++)
        {
            var component = gameObject.GetComponentAtIndex(i);
            if (component.GetType() == typeof(MeshFilter))
            {
                MeshFilter meshFilter = (MeshFilter)component;
                Mesh mesh = meshFilter.sharedMesh;
                ds.meshes.Add(mesh);

                MeshRenderer meshRenderer = gameObject.GetComponent<MeshRenderer>();
                if (meshRenderer != null && meshRenderer.sharedMaterial != null)
                {
                    ds.materials.Add(meshRenderer.sharedMaterial);
                    if (meshRenderer.sharedMaterial.mainTexture != null)
                    {
                        ds.textures.Add(meshRenderer.sharedMaterial.mainTexture);
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
