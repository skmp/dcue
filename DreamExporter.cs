using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;

using System.Collections.Generic;
using System.Collections;
using System.Text;
using UnityEngine.Rendering;
using UnityEngine.Windows;

public class DreamExporter : MonoBehaviour
{
    class DreamScene
    {
        public HashSet<Material> materials = new HashSet<Material>();
        public HashSet<Texture> textures = new HashSet<Texture>();
        public HashSet<Mesh> meshes = new HashSet<Mesh>();

        public List<GameObject> gameObjects = new List<GameObject>();
    }

    static string StringId(Object obj)
    {
        return obj.GetType().ToString().Replace(".", "_") + "_" + (uint)obj.GetInstanceID();
    }

    static string ToCPP(bool param)
    {
        return param ? "true" : "false";
    }

    [MenuItem("Dreamcast/Export Scene")]
    static void ExportScene()
    {
        Debug.Log("Collecting ...");

        var ds = new DreamScene();

        var scene = SceneManager.GetActiveScene();
        if (scene != null)
        {
            var rootObjects = scene.GetRootGameObjects();

            foreach (var rootObject in rootObjects)
            {
                CollectObject(ds, rootObject);
            }
        }

        Debug.Log("Exporting ...");
        var sb = new StringBuilder();

        sb.AppendLine("// Textures");
        foreach (var texture in ds.textures)
        {
            sb.AppendLine("// " + texture.name);
            sb.AppendLine("auto " + StringId(texture) + " = texture_t(\"" + AssetDatabase.GetAssetPath(texture) + "\", " + texture.width + ", " + texture.height + ");");
        }

        sb.AppendLine("// Materials");
        foreach (var material in ds.materials)
        {
            sb.AppendLine("// " + material.name);
            sb.AppendLine("auto " + StringId(material) + " = material_t(" + material.color.a + ", " + material.color.r + ", " + material.color.g + ", " + material.color.b + ", " + (material.mainTexture == null ? "nullptr" : "&" + StringId(material.mainTexture)) + ");");
        }

        sb.AppendLine("// Meshes");
        foreach (var mesh in ds.meshes)
        {
            var vertices = mesh.vertices;

            sb.Append("float " + StringId(mesh) + "_vtx[] = { ");
            foreach (var vertex in vertices)
            {
                sb.Append(vertex.x + ", " + vertex.y + ", " + vertex.z + ", ");
            }
            sb.AppendLine(" };");

            var uvs = mesh.uv;

            if (uvs.Length > 0)
            {
                sb.Append("float " + StringId(mesh) + "_uv[] = { ");
                foreach (var uv in uvs)
                {
                    sb.Append(uv.x + ", " + uv.y + ", ");
                }
                sb.AppendLine(" };");
            }
            else
            {
                sb.AppendLine("float* " + StringId(mesh) + "_uv = nullptr;");
            }

            var colors = mesh.colors32;

            if (colors.Length > 0)
            {
                sb.Append("uint8_t " + StringId(mesh) + "_col[] = { ");
                foreach (var color in colors)
                {
                    sb.Append(color.a + ", " + color.r + ", " + color.g + ", " + color.b + ", ");
                }
                sb.AppendLine(" };");
            }
            else
            {
                sb.AppendLine("uint8_t* " + StringId(mesh) + "_col = nullptr;");
            }

            var normals = mesh.normals;

            if (normals.Length > 0)
            {
                sb.Append("float " + StringId(mesh) + "_normal[] = { ");
                foreach (var normal in normals)
                {
                    sb.Append(normal.x + ", " + normal.y + ", " + normal.z + ", ");
                }
                sb.AppendLine(" };");
            }
            else
            {
                sb.AppendLine("float* " + StringId(mesh) + "_normal = nullptr;");
            }

            if (mesh.subMeshCount == 0)
            {
                throw new System.Exception("mesh.subMeshCount != 0");
            }
            var indicies = mesh.GetIndices(0);

            if (indicies.Length > 65535)
            {
                throw new System.Exception("indicies.Length > 65535");
            }

            sb.Append("uint16_t " + StringId(mesh) + "_indicies[] = { ");
            foreach (var ind in indicies)
            {
                sb.Append(ind + ", ");
            }
            sb.AppendLine("};");

            sb.AppendLine("// " + mesh.name);
            sb.AppendLine("auto " + StringId(mesh) + " = mesh_t(" + indicies.Length + ", " + StringId(mesh) + "_indicies, " + vertices.Length + ", " + StringId(mesh) + "_vtx, " + StringId(mesh) + "_uv, " + StringId(mesh) + "_col, " + StringId(mesh) + "_normal);");
        }

        sb.AppendLine("// Scene (GameObjects)");


        foreach (var go in ds.gameObjects)
        {
            var mtx = go.transform.localToWorldMatrix;
            Mesh mesh = null;
            Material material = null;
            bool meshEnabled = false;

            if (go.GetComponent<MeshFilter>() != null && go.GetComponent<MeshRenderer>() != null)
            {
                mesh = go.GetComponent<MeshFilter>().sharedMesh;
                material = go.GetComponent<MeshRenderer>().sharedMaterial;
                meshEnabled = go.GetComponent<MeshRenderer>().enabled;
            }

            sb.AppendLine("matrix_t " + StringId(go) + "_ltw = { " + mtx.m00 + ", " + mtx.m01 + ", " + mtx.m02 + ", " + mtx.m03 + ",\n" +
                                                                   + mtx.m10 + ", " + mtx.m11 + ", " + mtx.m12 + ", " + mtx.m13 + ",\n" +
                                                                   + mtx.m20 + ", " + mtx.m21 + ", " + mtx.m22 + ", " + mtx.m23 + ",\n" +
                                                                   + mtx.m30 + ", " + mtx.m31 + ", " + mtx.m32 + ", " + mtx.m33 + " };" );

            sb.AppendLine("// " + go.name);
            sb.AppendLine("auto " + StringId(go) + " = game_object_t(" + ToCPP(go.activeInHierarchy) + ", &" + StringId(go) + "_ltw, " + ToCPP(meshEnabled) + ", " + (mesh == null ? "nullptr" : "&" + StringId(mesh)) + ", " + (material == null ? "nullptr" : "&" + StringId(material)) + ");");
        }

        sb.Append("game_object_t* gameObjects[] = { ");
        foreach (var go in ds.gameObjects)
        {
            sb.Append("&" + StringId(go) + ", ");
        }
        sb.AppendLine("};");

        File.WriteAllBytes("dream.cpp", UTF8Encoding.UTF8.GetBytes(sb.ToString()));

        Debug.Log("Done!");
    }

    static void CollectObject(DreamScene ds, GameObject gameObject)
    {
        ds.gameObjects.Add(gameObject);
        
        for (int i = 0; i < gameObject.GetComponentCount(); i++)
        {
            var component = gameObject.GetComponentAtIndex(i);
            //Debug.Log("\t COMP " + i + " type: " + component.GetType().Name);

            if (component.GetType() == typeof(MeshFilter))
            {
                var meshFilter = (MeshFilter)component;
                var mesh = meshFilter.sharedMesh;
                ds.meshes.Add(mesh);

                var meshRenderer = gameObject.GetComponent<MeshRenderer>();
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
