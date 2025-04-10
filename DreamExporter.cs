using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;
using UnityEditor.Animations;
using System.Linq;
using Ludiq;
using UnityEditor.ShaderGraph.Drawing;
using Bolt;

public class DreamExporter : MonoBehaviour
{
    enum AnimationPropertyType
    {
        Float,
        Boolean,
        Unsupported
    }

    enum AnimationPropertyKey
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
    }

    class PropertyKeyInfo
    {
        public AnimationPropertyType type;
        public AnimationPropertyKey key;
        public bool Ignored;

        public PropertyKeyInfo(AnimationPropertyType type, AnimationPropertyKey key, bool Ignored = false)
        {
            this.type = type;
            this.key = key;
            this.Ignored = Ignored;
        }
    }

    static Dictionary<string, PropertyKeyInfo> knownPropertyKeys = new Dictionary<string, PropertyKeyInfo>()
    {
        { "AudioSource::m_Enabled", new PropertyKeyInfo(AnimationPropertyType.Boolean, AnimationPropertyKey.AudioSource_Enabled, true) },
        { "AudioSource::m_Volume", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.AudioSource_Volume, true) },
        { "Camera::field of view", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Camera_FOV, true) },
        { "GameObject::m_IsActive", new PropertyKeyInfo(AnimationPropertyType.Boolean, AnimationPropertyKey.GameObject_IsActive, true) },
        { "Image::m_Color.b", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Image_Color_b, true) },
        { "Image::m_Color.g", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Image_Color_g, true) },
        { "Image::m_Color.r", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Image_Color_r, true) },
        { "Light::m_Color.b", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Light_Color_b, true) },
        { "Light::m_Color.g", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Light_Color_g, true) },
        { "Light::m_Color.r", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Light_Color_r, true) },
        { "Light::m_Intensity", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Light_Intensity, true) },
        { "MeshRenderer::m_Enabled", new PropertyKeyInfo(AnimationPropertyType.Boolean, AnimationPropertyKey.MeshRenderer_Enabled, true) },
        { "MeshRenderer::material._Color.a", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.MeshRenderer_material_Color_a, true) },
        { "MeshRenderer::material._Color.b", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.MeshRenderer_material_Color_b, true) },
        { "MeshRenderer::material._Color.g", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.MeshRenderer_material_Color_g, true) },
        { "MeshRenderer::material._Color.r", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.MeshRenderer_material_Color_r, true) },
        { "MeshRenderer::material._Glossiness", new PropertyKeyInfo(AnimationPropertyType.Unsupported, AnimationPropertyKey.MeshRenderer_material_Glossiness, true) },
        { "MeshRenderer::material._Metallic", new PropertyKeyInfo(AnimationPropertyType.Unsupported, AnimationPropertyKey.MeshRenderer_material_Metallic, true) },
        { "MeshRenderer::material._Mode", new PropertyKeyInfo(AnimationPropertyType.Unsupported, AnimationPropertyKey.MeshRenderer_material_Mode, true) },

        { "Transform::localEulerAnglesRaw.x", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_localEulerAnglesRaw_x) },
        { "Transform::localEulerAnglesRaw.y", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_localEulerAnglesRaw_y) },
        { "Transform::localEulerAnglesRaw.z", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_localEulerAnglesRaw_z) },
        { "Transform::m_LocalPosition.x", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_m_LocalPosition_x) },
        { "Transform::m_LocalPosition.y", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_m_LocalPosition_y) },
        { "Transform::m_LocalPosition.z", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_m_LocalPosition_z) },
        { "Transform::m_LocalScale.x", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_m_LocalScale_x) },
        { "Transform::m_LocalScale.y", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_m_LocalScale_y) },
        { "Transform::m_LocalScale.z", new PropertyKeyInfo(AnimationPropertyType.Float, AnimationPropertyKey.Transform_m_LocalScale_z) },
    };

    public static GameObject GameObjectFromPath(GameObject gameObject, string path)
    {
        var rv = gameObject;

        if (path.Length == 0)
        {
            return rv;
        }

        var parts = path.Split('/');

        foreach(var part in parts)
        {
            if (part == "")
            {
                throw new Exception("Invalid part " + part + " in " + path);
            }
            bool found = false;

            for (int childNum = 0; childNum < rv.transform.childCount; childNum++)
            {
                var child = rv.transform.GetChild(childNum);
                if (child.name == part)
                {
                    found = true;
                    rv = child.gameObject;
                    break;
                }
            }

            if (!found)
            {
                rv = null;
                break;
            }
        }

        return rv;
    }

    [MenuItem("Dreamcast/Export Hierarchy")]
    public static void ProcessHierarchy()
    {
        DreamScene ds = CollectScene();

        StringBuilder sb = new StringBuilder();
        sb.AppendLine("#include <vector>");
        sb.AppendLine("#include \"dcue/types-native.h\"");
        sb.AppendLine("using namespace native;");
        sb.AppendLine("void InitializeHierarchy(std::vector<game_object_t*> gameObjects) {");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];
            if (gameObject.transform.parent != null)
            {
                sb.AppendLine($" gameObjects[{gameObjectNum}]->parent = gameObjects[{ds.gameObjectIndex[gameObject.transform.parent.gameObject]}];");
            }
            var position = gameObject.transform.localPosition;
            var rotation = gameObject.transform.localEulerAngles;
            var scale = gameObject.transform.localScale;
            sb.AppendLine($" gameObjects[{gameObjectNum}]->position = r_vector3_t{{{position.x}, {position.y}, {position.z}}};");
            sb.AppendLine($" gameObjects[{gameObjectNum}]->rotation = r_vector3_t{{{rotation.x}, {rotation.y}, {rotation.z}}};");
            sb.AppendLine($" gameObjects[{gameObjectNum}]->scale = r_vector3_t{{{scale.x}, {scale.y}, {scale.z}}};");
        }
        sb.AppendLine("}");

        File.WriteAllText("hierarchy.cpp", sb.ToString());
    }

    [MenuItem("Dreamcast/Export Animations")]
    public static void ProcessAnimations()
    {
        DreamScene ds = CollectScene();

        StringBuilder sb = new StringBuilder();

        sb.AppendLine("#include \"anim.h\"");
        sb.AppendLine();

        var rgo = SceneManager.GetActiveScene().GetRootGameObjects();

        var animators = new List<Animator>();

        foreach (var go in rgo) {
            animators.AddRange(go.GetComponentsInChildren<Animator>(true));
        }

        var animationClips = new HashSet<AnimationClip>();
        var animatiorControllers = new HashSet<AnimatorController>();

        for (int animatorNum = 0; animatorNum < animators.Count; animatorNum++)
        {
            var animator = animators[animatorNum];

            var animatorController = animator.runtimeAnimatorController as AnimatorController;

            if (animatorController == null || animatorController.animationClips.Length == 0)
                continue;

            animatiorControllers.Add(animatorController);


            foreach (var animationClip in animatorController.animationClips)
            {
                animationClips.Add(animationClip);
            }
        }
        Debug.Log("animationClips: " + animationClips.Count);
        Debug.Log("animatiorControllers: " + animatiorControllers.Count);

        foreach (var animatorController in animatiorControllers)
        {
            if (animatorController.layers.Length > 1)
            {
                throw new Exception("Too many layers");
            }

            if (animatorController.layers.Length == 0)
                continue;

            var stateMachine = animatorController.layers[0].stateMachine;

            if (stateMachine.stateMachines.Length > 0)
            {
                throw new Exception("Nested state machines");
            }

            // stateMachine.defaultState;

            foreach (ChildAnimatorState childState in stateMachine.states)
            {
                if (childState.state.transitions.Length != 0)
                    throw new Exception("State has Transistion");
                
                // For the future
                foreach (var transition in childState.state.transitions)
                {
                    if (!transition.hasExitTime)
                    {
                        throw new Exception("!transition.hasExitTime.");
                    }
                }
            }
        }

        var animationClipsList = new List<AnimationClip>(animationClips);
        var animationClipIndex = new Dictionary<AnimationClip,  int>();
        /*
         * Animation anim_%i = {
         *  {
         *   {
         *    { times... },
         *    { values... },
         *    num_frames, offset,
         *   },
         *  ....
         *  },
         *  num_targets,
         * };
         */
        for (int animationClipNum = 0; animationClipNum < animationClipsList.Count; animationClipNum++)
        {
            var animationClip = animationClipsList[animationClipNum];

            animationClipIndex.Add(animationClip, animationClipNum);

            sb.AppendLine();
            sb.AppendLine($"//Animation {animationClip.name}");

            EditorCurveBinding[] curveBindings = AnimationUtility.GetCurveBindings(animationClip);

            StringBuilder animationStringBuilder = new StringBuilder();

            animationStringBuilder.AppendLine();
            animationStringBuilder.AppendLine($"static animation_track_t anim_{animationClipNum}_tracks[] = {{");

            var timesValuesToName = new Dictionary<string, string>();

            int numTargets = 0;
            for (int curveBindingNum = 0; curveBindingNum < curveBindings.Length; curveBindingNum++)
            {
                EditorCurveBinding binding = curveBindings[curveBindingNum];
                // Retrieve the animation curve for this binding
                AnimationCurve curve = AnimationUtility.GetEditorCurve(animationClip, binding);

                var propertyKey = $"{binding.type.Name}::{binding.propertyName}";
                if (!knownPropertyKeys.ContainsKey(propertyKey))
                {
                    throw new Exception($"Unsupported propertyKey {propertyKey}");
                }

                var propertyKeyInfo = knownPropertyKeys[propertyKey];

                if (propertyKeyInfo.Ignored)
                {
                    continue;
                }

                if (propertyKeyInfo.type != AnimationPropertyType.Float)
                {
                    throw new Exception("Unsupported propertyKeyInfo");
                }

                numTargets++;
                //Debug.Log($" Binding: {binding.path} Property: {binding.propertyName} Type: {binding.type.Name} | Time: {key.time} Value: {key.value}");

                animationStringBuilder.AppendLine(" {");

                StringBuilder timesStringBuilder = new StringBuilder();

                timesStringBuilder.Append($" = {{");
                // Enumerate through all keyframes in this curve
                foreach (Keyframe key in curve.keys)
                {
                    timesStringBuilder.Append($"{key.time}, ");
                }
                timesStringBuilder.AppendLine("};");

                var timesString = timesStringBuilder.ToString();

                if (!timesValuesToName.ContainsKey(timesString))
                {
                    string name = $"anim_{animationClipNum}_track_{curveBindingNum}_times";
                    sb.Append("float ");
                    sb.Append(name);
                    sb.Append("[]");
                    sb.AppendLine(timesString);

                    timesValuesToName.Add(timesString, name);
                }

                sb.Append($"float anim_{animationClipNum}_track_{curveBindingNum}_values[] = {{");
                // Enumerate through all keyframes in this curve
                foreach (Keyframe key in curve.keys)
                {
                    sb.Append($"{key.value}, ");
                }
                sb.AppendLine("};");

                
                animationStringBuilder.AppendLine($"  {timesValuesToName[timesString]},");
                animationStringBuilder.AppendLine($"  anim_{animationClipNum}_track_{curveBindingNum}_values,");
                animationStringBuilder.AppendLine($"  {curve.keys.Length}, {propertyKeyInfo.key},");

                animationStringBuilder.AppendLine(" },");
            }
            animationStringBuilder.AppendLine("};");

            animationStringBuilder.AppendLine();
            animationStringBuilder.AppendLine($"static animation_t anim_{animationClipNum} = {{");
            animationStringBuilder.AppendLine($" anim_{animationClipNum}_tracks,");
            animationStringBuilder.AppendLine($" {numTargets},");
            animationStringBuilder.AppendLine("};");

            sb.AppendLine();
            sb.AppendLine(animationStringBuilder.ToString());
        }

        for (int animatorNum = 0; animatorNum < animators.Count; animatorNum++)
        {
            var animator = animators[animatorNum];
            var animatorController = animator.runtimeAnimatorController as AnimatorController;
            if (animatorController is null)
            {
                continue;
            }

            sb.AppendLine();
            sb.AppendLine($"//{animator.name} bound to {animatorController.name} (these are offsets to game_objects)");
            for (int clipNum = 0; clipNum < animatorController.animationClips.Length; clipNum++)
            {
                var animationClip = animatorController.animationClips[clipNum];

                sb.Append($"static size_t animator_{animatorNum}_binding_{clipNum}[] = {{");

                EditorCurveBinding[] curveBindings = AnimationUtility.GetCurveBindings(animationClip);

                for (int curveBindingNum = 0; curveBindingNum < curveBindings.Length; curveBindingNum++)
                {
                    EditorCurveBinding binding = curveBindings[curveBindingNum];
                    // Retrieve the animation curve for this binding
                    AnimationCurve curve = AnimationUtility.GetEditorCurve(animationClip, binding);

                    var propertyKey = $"{binding.type.Name}::{binding.propertyName}";
                    if (!knownPropertyKeys.ContainsKey(propertyKey))
                    {
                        throw new Exception($"Unsupported propertyKey {propertyKey}");
                    }

                    var propertyKeyInfo = knownPropertyKeys[propertyKey];

                    if (propertyKeyInfo.Ignored)
                    {
                        continue;
                    }

                    var boundGO = GameObjectFromPath(animator.gameObject, binding.path);
                    //Debug.Log($"path: {binding.path}: bound: {boundGO}");
                    if (boundGO != null )
                    {
                        sb.Append($"{ds.gameObjectIndex[boundGO]}, ");
                    }
                    else
                    {
                        sb.Append("SIZE_MAX, ");
                    }
                }
                sb.AppendLine("};");
            }



            for (int clipNum = 0; clipNum < animatorController.animationClips.Length; clipNum++)
            {
                var animationClip = animatorController.animationClips[clipNum];
                EditorCurveBinding[] curveBindings = AnimationUtility.GetCurveBindings(animationClip);

                sb.AppendLine($"unsigned animator_{animatorNum}_clip_{clipNum}_current_frames[{curveBindings.Length}];");
            }

            sb.Append($"static bound_animation_t animator_{animatorNum}_bindlist[] = {{ ");

            for (int clipNum = 0; clipNum < animatorController.animationClips.Length; clipNum++)
            {
                var animationClip = animatorController.animationClips[clipNum];

                sb.Append($"{{ &anim_{animationClipIndex[animationClip]}, animator_{animatorNum}_binding_{clipNum}, animator_{animatorNum}_clip_{clipNum}_current_frames }}, ");
            }
            sb.AppendLine("};");

            sb.AppendLine();
            sb.AppendLine($"static animator_t animator_{animatorNum} = {{ animator_{animatorNum}_bindlist, {animatorController.animationClips.Length} }};");
        }

        sb.AppendLine();
        sb.Append($"std::vector<animator_t*> animators = {{ ");
        for (int animatorNum = 0; animatorNum < animators.Count; animatorNum++)
        {
            var animator = animators[animatorNum];
            var animatorController = animator.runtimeAnimatorController as AnimatorController;
            if (animatorController is null)
            {
                continue;
            }
            sb.Append($"&animator_{animatorNum}, ");
        }
        sb.AppendLine("};");

        // write all text to anim.cpp
        File.WriteAllText("anim.cpp", sb.ToString());
    }

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

    class DreamTerrainData
    {
        public TerrainData tdata;
        public Vector2 tileScale;
        public Vector2 tileOffset;

        public DreamTerrainData(TerrainData tdata, Vector2 tileScale, Vector2 tileOffset) {
            this.tdata = tdata;
            this.tileScale = tileScale;
            this.tileOffset = tileOffset;
        }

        public override int GetHashCode()
        {
            return tdata.GetHashCode();
        }

        public override bool Equals(object obj)
        {
            var other = obj as DreamTerrainData;

            if (other != null)
            {
                return tdata == other.tdata && tileScale == other.tileScale && tileOffset == other.tileOffset;
            }

            return false;
        }

        // Overload == and != for consistency
        public static bool operator ==(DreamTerrainData left, DreamTerrainData right)
        {
            if (left is null) return right is null;

            return left.Equals(right);
        }

        public static bool operator !=(DreamTerrainData left, DreamTerrainData right)
        {
            return !(left == right);
        }
    }

    class DreamScene
    {
        public HashSet<Material> materials = new HashSet<Material>();
        public HashSet<Texture> textures = new HashSet<Texture>();
        public HashSet<DreamMesh> meshes = new HashSet<DreamMesh>();
        public HashSet<DreamTerrainData> terrains = new HashSet<DreamTerrainData>();
        public List<GameObject> gameObjects = new List<GameObject>();

        public List<Texture> textureList;
        public List<Material> materialList;
        public List<DreamMesh> meshList;
        public List<DreamTerrainData> terrainList;

        public Dictionary<Texture, int> textureIndex;
        public Dictionary<Material, int> materialIndex;
        public Dictionary<DreamMesh, int> meshIndex;
        public Dictionary<DreamTerrainData, int> terrainIndex;
        public Dictionary<GameObject, int> gameObjectIndex;

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


    static DreamScene CollectScene()
    {
        var ds = new DreamScene();
        Scene scene = SceneManager.GetActiveScene();
        if (scene != null)
        {
            GameObject[] rootObjects = scene.GetRootGameObjects();
            foreach (GameObject go in rootObjects)
            {
                CollectObject(ds, go);
            }
        }

        ds.textureList = new List<Texture>(ds.textures);
        ds.materialList = new List<Material>(ds.materials);

        ds.meshList = new List<DreamMesh>(ds.meshes);
        ds.terrainList = new List<DreamTerrainData>(ds.terrains);

        ds.textureIndex = new Dictionary<Texture, int>();
        for (int i = 0; i < ds.textureList.Count; i++)
        {
            ds.textureIndex[ds.textureList[i]] = i;
        }
        ds.materialIndex = new Dictionary<Material, int>();
        for (int i = 0; i < ds.materialList.Count; i++)
        {
            ds.materialIndex[ds.materialList[i]] = i;
        }
        ds.meshIndex = new Dictionary<DreamMesh, int>();
        for (int i = 0; i < ds.meshList.Count; i++)
        {
            ds.meshIndex[ds.meshList[i]] = i;
        }
        ds.terrainIndex = new Dictionary<DreamTerrainData, int>();
        for (int i = 0; i < ds.terrainList.Count; i++)
        {
            ds.terrainIndex[ds.terrainList[i]] = i;
        }
        ds.gameObjectIndex = new Dictionary<GameObject, int>();
        for (int i = 0; i < ds.gameObjects.Count; i++)
        {
            ds.gameObjectIndex[ds.gameObjects[i]] = i;
        }
        return ds;
    }

    [MenuItem("Dreamcast/Export Scene")]
    static void ExportScene()
    {
        Debug.Log("Collecting ...");
        DreamScene ds = CollectScene();

        Debug.Log("Exporting ...");
        // Write binary file with header "DCUE0002"
        using (FileStream fs = new FileStream("dream.dat", FileMode.Create))
        using (BinaryWriter writer = new BinaryWriter(fs, Encoding.UTF8))
        {
            // Write header (8 bytes)
            writer.Write(Encoding.ASCII.GetBytes("DCUE0002"));

            // --------------------
            // Write Textures Section
            // --------------------
            writer.Write(ds.textureList.Count);
            foreach (Texture tex in ds.textureList)
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
            writer.Write(ds.materialList.Count);
            foreach (Material mat in ds.materialList)
            {
                // Write material color components (as floats)
                Color col = mat.color;
                writer.Write(col.a);
                writer.Write(col.r);
                writer.Write(col.g);
                writer.Write(col.b);

                // Write whether the material has a main texture.
                // If so, write the index of that texture.
                bool hasTexture = (mat.mainTexture != null) && ds.textureIndex.ContainsKey(mat.mainTexture);
                writer.Write(hasTexture);
                if (hasTexture)
                {
                    writer.Write(ds.textureIndex[mat.mainTexture]);
                }
            }

            // --------------------
            // Write Meshes Section
            // --------------------
            writer.Write(ds.meshList.Count);
            foreach (DreamMesh dmesh in ds.meshList)
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

                    //Debug.Log("Replica: " + current_replica + " replica_base " + replica_base);

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

                //Debug.Log("Replicas: " + base_replicas_decode.Length + " replicated vtx: " + (replica_base - mesh.vertices.Length));

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
            // Write Terrains Section
            // --------------------
            writer.Write(ds.terrains.Count);
            foreach (DreamTerrainData dterraindata in ds.terrains)
            {
                var tdata = dterraindata.tdata;

                int width = tdata.heightmapResolution;
                int height = tdata.heightmapResolution;
                float[,] heights = tdata.GetHeights(0, 0, width, height);

                Vector3 size = tdata.size;
                writer.Write(size.x);
                writer.Write(size.y);
                writer.Write(size.z);

                writer.Write(dterraindata.tileScale.x);
                writer.Write(dterraindata.tileScale.y);

                writer.Write(dterraindata.tileOffset.x);
                writer.Write(dterraindata.tileOffset.y);

                writer.Write(width);
                writer.Write(height);

                for (int y = 0; y < height; y++)
                {
                    for (int x = 0; x < width; x++)
                    {
                        writer.Write(heights[y, x]);
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
                bool hasMesh = (mesh != null) && ds.meshIndex.ContainsKey(mesh);
                writer.Write(hasMesh);
                if (hasMesh)
                {
                    writer.Write(ds.meshIndex[mesh]);
                }
                // Write material index if material exists.
                writer.Write((int)materials.Length);
                foreach (Material material in materials)
                {
                    if (material != null)
                    {
                        writer.Write(ds.materialIndex[material]);
                    }
                    else
                    {
                        writer.Write((int)-1);
                    }
                }

                // terrain attachment
                Terrain terrain = go.GetComponent<Terrain>();
                if (terrain != null && terrain.terrainData != null)
                {
                    if (hasMesh)
                    {
                        throw new Exception("Can't have both terrain AND mesh on same game object");
                    }

                    var tdata = terrain.terrainData;
                    var mat = terrain.materialTemplate;


                    var dterraindata = new DreamTerrainData(tdata, mat.mainTextureScale, mat.mainTextureOffset);
                    writer.Write(ds.terrainIndex[dterraindata]);
                    writer.Write(ds.materialIndex[mat]);
                } else
                {
                    writer.Write((int)-1);
                    writer.Write((int)-1);
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

            {
                var terrain = gameObject.GetComponent<Terrain>();

                if (terrain != null && terrain.terrainData != null)
                {
                    var tdata = terrain.terrainData;

                    var material = terrain.materialTemplate;
                    if (material == null )
                    {
                        throw new Exception("material == null");
                    }
                    ds.materials.Add(material);
                    if (material.mainTexture != null)
                    {
                        if (material.mainTexture is Texture2D)
                        {
                            ds.textures.Add(material.mainTexture);
                        }
                    }

                    ds.terrains.Add(new DreamTerrainData(tdata, material.mainTextureScale, material.mainTextureOffset));
                }
            }
        }

        for (int i = 0; i < gameObject.transform.childCount; i++)
        {
            CollectObject(ds, gameObject.transform.GetChild(i).gameObject);
        }
    }
}
