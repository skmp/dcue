using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEditor;
using UnityEngine.SceneManagement;
using UnityEditor.Animations;
using mango;
using System.Linq;
using Ludiq;
using UnityEngine.UI;
using Pavo.Behaviors;

public class DreamExporter : MonoBehaviour
{
    [MenuItem("Dreamcast/Export Fonts")]
    public static void ExportFonts()
    {
        var ds = CollectScene();

        string fontCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+[]{}|;':\",.<>?/`~";


        StringBuilder sb = new StringBuilder();
        sb.AppendLine("#include \"fonts.h\"");

        for (int dfontNum = 0; dfontNum < ds.dfonts.Count; dfontNum++)
        {
            var dfont = ds.dfonts[dfontNum];
            var font = dfont.font;

            foreach (char c in fontCharacters)
            {
                font.RequestCharactersInTexture(c.ToString(), dfont.size, dfont.style);
            }
        }

        for (int fontNum = 0; fontNum < ds.fonts.Count; fontNum++)
        {
            var font = ds.fonts[fontNum];

            var atlas = GetReadableTexture(font.material.mainTexture);

            File.WriteAllBytes($"font_{fontNum}.png", atlas.EncodeToPNG());

            sb.AppendLine($"texture_t font_texture_{fontNum};");
        }

        for (int dfontNum = 0; dfontNum < ds.dfonts.Count; dfontNum++)
        {
            var dfont = ds.dfonts[dfontNum];
            var font = dfont.font;

            CharacterInfo chInfo;

            sb.Append($"font_char_t font_chars_{dfontNum}[] = {{ ");
            for (int i = 0; i < fontCharacters.Length; i++)
            {
                char c = fontCharacters[i];
                if (!font.GetCharacterInfo(c, out chInfo, dfont.size, dfont.style))
                {
                    throw new Exception("Missing character " + c + " from font " + font.name);
                }
                sb.Append($"{{ {{ {chInfo.uvTopRight.x}, {chInfo.uvTopRight.y}, {chInfo.uvTopLeft.x}, {chInfo.uvTopLeft.y}, {chInfo.uvBottomLeft.x}, {chInfo.uvBottomLeft.y}, {chInfo.uvBottomRight.x}, {chInfo.uvBottomRight.y} }}, {{ {chInfo.minX}, {chInfo.minY}, {chInfo.maxX}, {chInfo.maxY} }}, {chInfo.advance} }}, ");
            }
            sb.AppendLine("};");

            sb.Append($"font_t fonts_{dfontNum} = {{ ");
            sb.Append($"&font_texture_{ds.fontIndex[font]}, font_chars_{dfontNum}, {dfont.size}, fs_{dfont.style}, ");
            sb.AppendLine($"}}; //{dfont.font.name}");
        }

        sb.AppendLine("");
        sb.AppendLine("void InitializeFonts() {");
        for (int fontNum = 0; fontNum < ds.fonts.Count; fontNum++)
        {
            var font = ds.fonts[fontNum];
            sb.AppendLine($" load_pvr(\"font_{fontNum}.pvr\", &font_texture_{fontNum});");
        }
        sb.AppendLine("};");
        File.WriteAllText("fonts.cpp", sb.ToString());
    }
    struct BakedMeshInfo
    {
        public int indicesId;
        public int verticesId;
        public int numTriangles;
        public int numVertices;
    }
    static BakedMeshInfo BakeCollisionMesh(Mesh mesh, Dictionary<string, int> all_indices, Dictionary<string, int> all_vertices)
    {
        var triangles = mesh.triangles;
        List<Vector3> vertices = new List<Vector3>(mesh.vertices);
        Dictionary<Vector3, int> processedVertices = new Dictionary<Vector3, int>();

        var processedTriangles = new List<int>();

        float machineEpsilon = 1.1920929E-6f;
        Vector3 max = new Vector3();
        foreach(var vertex in vertices)
        {
            if (max.x < Math.Abs(vertex.x))
            {
                max.x = Math.Abs(vertex.x);
            }
            if (max.y < Math.Abs(vertex.y))
            {
                max.y = Math.Abs(vertex.y);
            }
            if (max.z < Math.Abs(vertex.z))
            {
                max.z = Math.Abs(vertex.z);
            }
        }

        float epsilon = 3 * (max.x + max.y + max.z) * machineEpsilon;

        if (epsilon < 0.01f)
        {
            epsilon = 0.01f;
        }

        for (int i = 0; i < triangles.Length; i += 3)
        {
            float area = Vector3.Cross(vertices[triangles[i + 1]] - vertices[triangles[i]], vertices[triangles[i + 2]] - vertices[triangles[i]]).magnitude / 2.0f;
            if (area < epsilon)
            {
                //Debug.LogWarning($"Triangle {i / 3} is degenerate, area: {area}");
                continue;
            }

            if ((vertices[triangles[i + 1]] - vertices[triangles[i]]).magnitude < epsilon)
            {
                //Debug.LogWarning($"Triangle {i / 3} is degenerate, edge: {vertices[triangles[i + 1]] - vertices[triangles[i]]}");
                continue;
            }
            if ((vertices[triangles[i + 2]] - vertices[triangles[i]]).magnitude < epsilon)
            {
                //Debug.LogWarning($"Triangle {i / 3} is degenerate, edge: {vertices[triangles[i + 2]] - vertices[triangles[i]]}");
                continue;
            }
            if ((vertices[triangles[i + 2]] - vertices[triangles[i + 1]]).magnitude < epsilon)
            {
                //Debug.LogWarning($"Triangle {i / 3} is degenerate, edge: {vertices[triangles[i + 2]] - vertices[triangles[i + 1]]}");
                continue;
            }

            int[] new_triangle = new int[3] { triangles[i], triangles[i + 1], triangles[i + 2] };

            for (int j = 0; j < 3; j++)
            {
                if (!processedVertices.ContainsKey(vertices[new_triangle[j]]))
                {
                    processedVertices.Add(vertices[new_triangle[j]], processedVertices.Count);
                }
                new_triangle[j] = processedVertices[vertices[new_triangle[j]]];
            }

            // CCW for physics
            processedTriangles.Add(new_triangle[0]);
            processedTriangles.Add(new_triangle[1]);
            processedTriangles.Add(new_triangle[2]);
        }

        var stringifiedTriangles = String.Join(", ", processedTriangles.ToArray());
        StringBuilder sb = new StringBuilder();
        foreach (var vertex in processedVertices.Keys)
        {
            sb.Append($"{vertex.x}, {vertex.y}, {vertex.z}, ");
        }

        var stringifiedVertices = sb.ToString();

        if (!all_indices.ContainsKey(stringifiedTriangles))
        {
            all_indices.Add(stringifiedTriangles, all_indices.Count);
        }

        if (!all_vertices.ContainsKey(stringifiedVertices))
        {
            all_vertices.Add(stringifiedVertices, all_vertices.Count);
        }
        BakedMeshInfo rv;
        rv.indicesId = all_indices[stringifiedTriangles];
        rv.verticesId = all_vertices[stringifiedVertices];
        rv.numTriangles = processedTriangles.Count;
        rv.numVertices = processedVertices.Count;
        //Debug.Log($"Baked mesh {mesh.name} with {rv.numTriangles} triangles and {rv.numVertices} vertices");
        return rv;
    }

    static void ProcessPhysics(DreamScene ds)
    {
        StringBuilder sb = new StringBuilder();
        sb.AppendLine("#include <cstdint>");
        sb.AppendLine("#include <cstddef>");
        sb.AppendLine("#include \"physics.h\"");
        sb.AppendLine("#include \"dcue/types-native.h\"");
        sb.AppendLine("using namespace native;");

        var all_vertices = new Dictionary<string, int>();
        var all_indices = new Dictionary<string, int>();


        StringBuilder meshCollidersStringBuilder = new StringBuilder();

        // box colliders
        sb.AppendLine();
        for(int boxColliderNum = 0; boxColliderNum < ds.boxColliders.Count; boxColliderNum++)
        {
            var boxCollider = ds.boxColliders[boxColliderNum];
            sb.Append($"box_collider_t box_collider_{boxColliderNum} = {{ ");
            sb.Append($"nullptr, {{ {boxCollider.center.x}, {boxCollider.center.y}, {boxCollider.center.z} }}, {{ {boxCollider.size.x / 2}, {boxCollider.size.y / 2}, {boxCollider.size.z / 2} }} ");
            sb.AppendLine("};");
        }
        sb.Append("box_collider_t* box_colliders[] = { ");
        for (int boxColliderNum = 0; boxColliderNum < ds.boxColliders.Count; boxColliderNum++)
        {
            var boxCollider = ds.boxColliders[boxColliderNum];
            sb.Append($"&box_collider_{boxColliderNum}, ");
        }
        sb.AppendLine("nullptr, };");

        // sphere colliders
        sb.AppendLine();
        for (int sphereColliderNum = 0; sphereColliderNum < ds.sphereColliders.Count; sphereColliderNum++)
        {
            var sphereCollider = ds.sphereColliders[sphereColliderNum];
            sb.Append($"sphere_collider_t sphere_collider_{sphereColliderNum} = {{ ");
            sb.Append($"nullptr, {{ {sphereCollider.center.x}, {sphereCollider.center.y}, {sphereCollider.center.z} }}, {sphereCollider.radius} ");
            sb.AppendLine("};");
        }
        sb.Append("sphere_collider_t* sphere_colliders[] = { ");
        for (int sphereColliderNum = 0; sphereColliderNum < ds.sphereColliders.Count; sphereColliderNum++)
        {
            var sphereCollider = ds.sphereColliders[sphereColliderNum];
            sb.Append($"&sphere_collider_{sphereColliderNum}, ");
        }
        sb.AppendLine("nullptr, };");

        // capsule colliders
        sb.AppendLine();
        for (int capsuleColliderNum = 0; capsuleColliderNum < ds.capsuleColliders.Count; capsuleColliderNum++)
        {
            var capsuleCollider = ds.capsuleColliders[capsuleColliderNum];
            sb.Append($"capsule_collider_t capsule_collider_{capsuleColliderNum} = {{ ");
            sb.Append($"nullptr, {{ {capsuleCollider.center.x}, {capsuleCollider.center.y}, {capsuleCollider.center.z} }}, {capsuleCollider.radius}, {capsuleCollider.height} ");
            sb.AppendLine("};");
        }

        sb.Append("capsule_collider_t* capsule_colliders[] = { ");
        for (int capsuleColliderNum = 0; capsuleColliderNum < ds.capsuleColliders.Count; capsuleColliderNum++)
        {
            var capsuleCollider = ds.capsuleColliders[capsuleColliderNum];
            sb.Append($"&capsule_collider_{capsuleColliderNum}, ");
        }
        sb.AppendLine("nullptr, };");

        // mesh colliders
        meshCollidersStringBuilder.AppendLine();
        for (int meshColliderNum = 0; meshColliderNum < ds.meshColliders.Count; meshColliderNum++)
        {
            var meshCollider = ds.meshColliders[meshColliderNum];

            var bakedMeshInfo = BakeCollisionMesh(meshCollider.sharedMesh, all_indices, all_vertices);

            meshCollidersStringBuilder.Append($"mesh_collider_t mesh_collider_{meshColliderNum} = {{ ");
            meshCollidersStringBuilder.Append($"nullptr, collision_mesh_vertices_{bakedMeshInfo.verticesId}, collision_mesh_indices_{bakedMeshInfo.indicesId}, {bakedMeshInfo.numVertices}, {bakedMeshInfo.numTriangles}, ");
            meshCollidersStringBuilder.AppendLine("};");
        }

        meshCollidersStringBuilder.AppendLine();
        meshCollidersStringBuilder.Append("mesh_collider_t* mesh_colliders[] = { ");
        for (int meshColliderNum = 0; meshColliderNum < ds.meshColliders.Count; meshColliderNum++)
        {
            var meshCollider = ds.meshColliders[meshColliderNum];
            if (meshCollider.sharedMesh == null)
            {
                continue;
            }
            meshCollidersStringBuilder.Append($"&mesh_collider_{meshColliderNum}, ");
        }
        meshCollidersStringBuilder.AppendLine("nullptr, };");

        sb.AppendLine();
        foreach (var vertices in all_vertices)
        {
            sb.AppendLine($"static float collision_mesh_vertices_{vertices.Value}[] = {{ {vertices.Key} }};");
        }

        sb.AppendLine();
        foreach (var indices in all_indices)
        {
            sb.AppendLine($"static uint16_t collision_mesh_indices_{indices.Value}[] = {{ {indices.Key} }};");
        }

        sb.AppendLine();
        sb.AppendLine(meshCollidersStringBuilder.ToString());

        File.WriteAllText("physics.cpp", sb.ToString());
    }
    static void ProcessCameras(DreamScene ds)
    {

        StringBuilder sb = new StringBuilder();

        sb.AppendLine("#include <cstdint>");
        sb.AppendLine("#include \"cameras.h\"");
        
        for (int cameraNum = 0; cameraNum < ds.cameras.Count; cameraNum++)
        {
            var camera = ds.cameras[cameraNum];
            sb.Append($"camera_t camera_{cameraNum} = {{ ");
            sb.Append($"nullptr, {camera.fieldOfView}, {camera.nearClipPlane}, {camera.farClipPlane}, ");
            sb.AppendLine("};");
        }

        GenerateComponentArray(ds, sb, "camera", ds.cameras);

        File.WriteAllText("cameras.cpp", sb.ToString());
    }

    static string typenameToVarname(string typename)
    {
        // convert from snake_case to camelCase
        string[] parts = typename.Split('_');
        for (int i = 1; i < parts.Length; i++)
        {
            parts[i] = char.ToUpper(parts[i][0]) + parts[i].Substring(1);
        }
        return string.Join("", parts);
    }
    private static void GenerateComponentDeclarations<T>(DreamScene ds, StringBuilder sb, string componentName, List<T> sceneComponents, Dictionary<T, int> componentIndex, Dictionary<IInteraction, int> interactionsIndex = null) where T:Component
    {
        for (int componentNum = 0; componentNum < sceneComponents.Count; componentNum++)
        {
            sb.AppendLine($"extern {componentName}_t {componentName}_{componentNum};");
            if (interactionsIndex != null)
            {
                var interaciton = sceneComponents[componentNum] as IInteraction;
                if (interaciton == null)
                {
                    throw new Exception($"Component {componentName}_{componentNum} is not an IInteraction");
                }
                interactionsIndex[interaciton] = componentNum;
            }
        }

        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];
            var components = gameObject.GetComponents<T>();

            if (components.Length != 0)
            {
                sb.Append($"static {componentName}_t* {componentName}s_{gameObjectNum}[] = {{ ");
                for (int componentNum = 0; componentNum < components.Length; componentNum++)
                {
                    var component = components[componentNum];
                    if (ds.rejectedComponents.Contains(component))
                    {
                        continue;
                    }
                    sb.Append($"&{componentName}_{componentIndex[component]}, ");
                }
                sb.AppendLine("nullptr, };");
            }
        }
    }
    private static void GenerateInteractionDeclarations(DreamScene ds, StringBuilder sb, Dictionary<IInteraction, int> interactionsIndex)
    {
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];
            var components = gameObject.GetComponents<IInteraction>().Where(x => supportedInteractionTypes.ContainsKey(x.GetType())).ToArray();

            bool sort = components.Any(x => x.Index != 0);
            if (sort)
            {
                Array.Sort(components, delegate (IInteraction x, IInteraction y) { return x.Index.CompareTo(y.Index); });
            }
            if (components.Length != 0)
            {
                sb.Append($"static interaction_t* interactions_{gameObjectNum}[] = {{ ");
                for (int componentNum = 0; componentNum < components.Length; componentNum++)
                {
                    var component = components[componentNum];
                    if (ds.rejectedComponents.Contains(component))
                    {
                        continue;
                    }
                    
                    sb.Append($"(interaction_t*)&{supportedInteractionTypes[component.GetType()]}_{interactionsIndex[component]}, ");
                }
                sb.AppendLine("nullptr, };");
            }
        }
    }

    private static void GenerateComponentArrays(DreamScene ds, StringBuilder sb)
    {
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];

            List<string> components = new List<string>();

            // components
            if (gameObject.GetComponent<Animator>() != null) components.Add("animator");
            if (gameObject.GetComponent<Camera>() != null) components.Add("camera");

            // scripts
            if (gameObject.GetComponent<ProximityInteractable>() != null) components.Add("proximity_interactable");
            if (gameObject.GetComponent<PlayerMovement2>() != null) components.Add("player_movement");
            if (gameObject.GetComponent<MouseLook>() != null) components.Add("mouse_look");
            if (gameObject.GetComponent<Interactable>() != null) components.Add("interactable");

            // interactions
            bool hasInteractions = false;
            foreach (var interactionType in supportedInteractionTypes.Keys)
            {
                if (gameObject.GetComponent(interactionType) != null)
                {
                    hasInteractions = true;
                    components.Add(supportedInteractionTypes[interactionType]);
                }
            }

            // interactions list
            if (hasInteractions)
            {
                components.Add("interaction");
            }

            // physics
            if (gameObject.GetComponent<BoxCollider>() != null) components.Add("box_collider");
            if (gameObject.GetComponent<SphereCollider>() != null) components.Add("sphere_collider");
            if (gameObject.GetComponent<CapsuleCollider>() != null) components.Add("capsule_collider");
            if (gameObject.GetComponent<MeshCollider>() != null) components.Add("mesh_collider");

            sb.Append($"static component_t components_{gameObjectNum}[] = {{ ");
            foreach(var component in components)
            {
                sb.Append($"ct_{component}, {{ .{typenameToVarname(component)}s = {component}s_{gameObjectNum} }} , ");
            }
            sb.AppendLine("ct_eol };");
        }
    }

    private static void GenerateComponentArray<T>(DreamScene ds, StringBuilder sb, string componentName, List<T> components) where T : Component
    {
        sb.Append($"{componentName}_t* {componentName}s[] = {{ ");
        for (int componentNum = 0; componentNum < components.Count; componentNum++)
        {
            sb.Append($"&{componentName}_{componentNum}, ");
        }
        sb.AppendLine("nullptr, };");

        //sb.Append($"static size_t {componentName}_gameObjects[] = {{");
        //for (int componentNum = 0; componentNum < components.Count; componentNum++)
        //{
        //    sb.Append($"{ds.gameObjectIndex[components[componentNum].gameObject]}, ");
        //}
        //sb.AppendLine("nullptr, };");
    }

    static string escapeCodeString(string v)
    {
        return $"\"{v.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r")}\"";
    }

    static Dictionary<Type, string> supportedInteractionTypes = new Dictionary<Type, string>() {
       { typeof(gameobjectactiveinactive2), "game_object_activeinactive" },
       { typeof(timedactiveinactive), "timed_activeinactive" },
       { typeof(Fadein), "fadein" },
    };

    // scripts
    static void ProcessScripts(DreamScene ds)
    {
        //HashSet<Type> interactionTypes = new HashSet<Type>();
        //var interactions = GetSceneComponents<IInteraction>();

        //foreach (var interaction in interactions)
        //{
        //    if (interaction == null)
        //    {
        //        continue;
        //    }
        //    interactionTypes.Add(interaction.GetType());
        //}

        //foreach (var interactionType in interactionTypes)
        //{
        //    Debug.Log($"Found interaction type: {interactionType}");
        //}

        /////////////// proximity_interactable //////////////
        StringBuilder sb = new StringBuilder();
        sb.AppendLine("#include <cstdint>");
        sb.AppendLine("#include \"scripts.h\"");

        for (int proximityInteractableNum = 0; proximityInteractableNum < ds.proximityInteractables.Count; proximityInteractableNum++)
        {
            var proximityInteractable = ds.proximityInteractables[proximityInteractableNum];
            sb.Append($"proximity_interactable_t proximity_interactable_{proximityInteractableNum} = {{ ");

            // TODO: this is a hack here, fix it
            if (proximityInteractable.player == null)
            {
                sb.Append($"nullptr, {ds.gameObjectIndex[GameObject.Find("playa")]}, {proximityInteractable.radious}, {proximityInteractable.distsance}, {proximityInteractable.MultipleShot.ToString().ToLower()}, ");
            }
            else
            {
                sb.Append($"nullptr, {ds.gameObjectIndex[proximityInteractable.player.gameObject]}, {proximityInteractable.radious}, {proximityInteractable.distsance}, {proximityInteractable.MultipleShot.ToString().ToLower()}, ");
            }
            sb.AppendLine("};");
        }

        GenerateComponentArray(ds, sb, "proximity_interactable", ds.proximityInteractables);

        /////////////// player_movement ///////////////
        for (int playerMovementNum = 0; playerMovementNum < ds.playerMovement2s.Count; playerMovementNum++)
        {
            var playerMovement = ds.playerMovement2s[playerMovementNum];
            sb.Append($"player_movement_t player_movement_{playerMovementNum} = {{ ");
            sb.Append($"nullptr, {playerMovement.speed}, {playerMovement.gravity}, {playerMovement.groundDistance}, ");
            sb.AppendLine("};");
        }
        GenerateComponentArray(ds, sb, "player_movement", ds.playerMovement2s);

        /////////////// mouse_look ///////////////
        for (int mouseLookNum = 0; mouseLookNum < ds.mouseLooks.Count; mouseLookNum++)
        {
            var mouseLook = ds.mouseLooks[mouseLookNum];
            sb.Append($"mouse_look_t mouse_look_{mouseLookNum} = {{ ");
            sb.Append($"nullptr, {ds.gameObjectIndex[mouseLook.playerBody.gameObject]}, ");
            sb.AppendLine("};");
        }
        GenerateComponentArray(ds, sb, "mouse_look", ds.mouseLooks);


        /////////////// interactable ///////////////
        for (int interactableNum = 0; interactableNum < ds.interactables.Count; interactableNum++)
        {
            var interactable = ds.interactables[interactableNum];
            string message = "nullptr";
            if (interactable.Message.Length != 0)
            {
                sb.AppendLine($"static const char* interactable_{interactableNum}_messages[] = {{ {string.Join(", ", interactable.Message.Select( x => escapeCodeString(x) ).ToArray())}, nullptr, }};");
                message = $"interactable_{interactableNum}_messages";
            }
            sb.Append($"interactable_t interactable_{interactableNum} = {{ ");
            sb.Append($"nullptr, {escapeCodeString(interactable.SpeakerName)}, {escapeCodeString(interactable.LookAtMessage)}, {message}, {interactable.InteractionRadious}, /* TODO: add more here */");
            sb.AppendLine("};");
        }
        GenerateComponentArray(ds, sb, "interactable", ds.interactables);



        //////////////
        ////////////// INTERACTIONS ///////////////
        //////////////

        ////////////// game_object_activeinactive //////////
        for (int gameObjectActiveInactiveNum = 0; gameObjectActiveInactiveNum < ds.gameobjectactiveinactive2s.Count; gameObjectActiveInactiveNum++)
        {
            var gameObjectActiveInactive = ds.gameobjectactiveinactive2s[gameObjectActiveInactiveNum];
            sb.Append($"game_object_activeinactive_t game_object_activeinactive_{gameObjectActiveInactiveNum} = {{ ");
            string gameObjectIndex = gameObjectActiveInactive.GameObjectToToggle != null ? ds.gameObjectIndex[gameObjectActiveInactive.GameObjectToToggle].ToString() : "SIZE_MAX";
            sb.Append($"nullptr, {gameObjectIndex}, \"{gameObjectActiveInactive.message}\", {gameObjectActiveInactive.SetTo.ToString().ToLower()}, ");
            sb.AppendLine("};");
        }

        GenerateComponentArray(ds, sb, "game_object_activeinactive", ds.gameobjectactiveinactive2s);


        /////////////// timed_activeinactive ///////////////
        for (int timedActiveInactiveNum = 0; timedActiveInactiveNum < ds.timedactiveinactives.Count; timedActiveInactiveNum++)
        {
            var timedActiveInactive = ds.timedactiveinactives[timedActiveInactiveNum];
            sb.Append($"timed_activeinactive_t timed_activeinactive_{timedActiveInactiveNum} = {{ ");
            string gameObjectIndex = timedActiveInactive.GameObject != null ? ds.gameObjectIndex[timedActiveInactive.GameObject].ToString() : "SIZE_MAX";
            sb.Append($"nullptr, {gameObjectIndex}, {timedActiveInactive.GetDelay()}, {timedActiveInactive.SetTo.ToString().ToLower()}, ");
            sb.AppendLine("};");
        }
        GenerateComponentArray(ds, sb, "timed_activeinactive", ds.timedactiveinactives);

        /////////////// fadein ///////////////
        for (int fadeinNum = 0; fadeinNum < ds.fadeins.Count; fadeinNum++)
        {
            var fadein = ds.fadeins[fadeinNum];
            sb.Append($"fadein_t fadein_{fadeinNum} = {{ ");
            sb.Append($"nullptr, {fadein.fadeInDuration}, {fadein.targetVolume2}, ");
            sb.AppendLine("};");
        }
        GenerateComponentArray(ds, sb, "fadein", ds.fadeins);


        File.WriteAllText("scripts.cpp", sb.ToString());
    }

    [MenuItem("Dreamcast/Export Components")]
    public static void ProcessComponents()
    {
        DreamScene ds = CollectScene();

        StringBuilder sb = new StringBuilder();

        sb.AppendLine("#include \"components.h\"");
        sb.AppendLine("using namespace native;");

        // components
        GenerateComponentDeclarations(ds, sb, "animator", ds.animators, ds.animatorIndex);
        GenerateComponentDeclarations(ds, sb, "camera", ds.cameras, ds.cameraIndex);

        // scripts
        GenerateComponentDeclarations(ds, sb, "proximity_interactable", ds.proximityInteractables, ds.proximityInteractableIndex);
        GenerateComponentDeclarations(ds, sb, "player_movement", ds.playerMovement2s, ds.playerMovement2Index);
        GenerateComponentDeclarations(ds, sb, "mouse_look", ds.mouseLooks, ds.mouseLookIndex);
        GenerateComponentDeclarations(ds, sb, "interactable", ds.interactables, ds.interactableIndex);

        // interactions
        Dictionary<IInteraction, int> interactionsIndex = new Dictionary<IInteraction, int>();
        GenerateComponentDeclarations(ds, sb, "game_object_activeinactive", ds.gameobjectactiveinactive2s, ds.gameobjectactiveinactive2Index, interactionsIndex);
        GenerateComponentDeclarations(ds, sb, "timed_activeinactive", ds.timedactiveinactives, ds.timedactiveinactiveIndex, interactionsIndex);
        GenerateComponentDeclarations(ds, sb, "fadein", ds.fadeins, ds.fadeinIndex, interactionsIndex);

        // interaction lists
        GenerateInteractionDeclarations(ds, sb, interactionsIndex);

        // physics

        GenerateComponentDeclarations(ds, sb, "box_collider", ds.boxColliders, ds.boxColliderIndex);
        GenerateComponentDeclarations(ds, sb, "sphere_collider", ds.sphereColliders, ds.sphereColliderIndex);
        GenerateComponentDeclarations(ds, sb, "capsule_collider", ds.capsuleColliders, ds.capsuleColliderIndex);
        GenerateComponentDeclarations(ds, sb, "mesh_collider", ds.meshColliders, ds.meshColliderIndex);

        GenerateComponentArrays(ds, sb);

        sb.Append("component_t* components[] = { ");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            sb.Append($"components_{gameObjectNum}, ");
        }
        sb.AppendLine("};");

        sb.AppendLine("void InitializeComponents(std::vector<game_object_t*> gameObjects) {");
        sb.AppendLine(" for (size_t gameObjectNum = 0; gameObjectNum < gameObjects.size(); gameObjectNum++) {");
        sb.AppendLine("  gameObjects[gameObjectNum]->components = components[gameObjectNum];");
        sb.AppendLine("  component_t* currentComponentList = components[gameObjectNum];");
        sb.AppendLine("  while (currentComponentList->componentType != ct_eol)");
        sb.AppendLine("  {");
        sb.AppendLine("      currentComponentList++;");
        sb.AppendLine("      auto component = currentComponentList->components;");
        sb.AppendLine("      do");
        sb.AppendLine("      {");
        sb.AppendLine("          (*component)->gameObject = gameObjects[gameObjectNum];");
        sb.AppendLine("      } while (*++component);");
        sb.AppendLine("      currentComponentList++;");
        sb.AppendLine("  }");
        sb.AppendLine(" }");
        sb.AppendLine("}");

        File.WriteAllText("components.cpp", sb.ToString());

        ProcessAnimations(ds);
        ProcessCameras(ds);

        ProcessScripts(ds);

        ProcessPhysics(ds);
    }
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
        sb.AppendLine("");
        
        sb.AppendLine("static size_t parents[] = { ");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];
            if (gameObject.transform.parent != null)
            {
                sb.Append($"{ds.gameObjectIndex[gameObject.transform.parent.gameObject]}, ");
            }
            else
            {
                sb.Append("SIZE_MAX, ");
            }
        }
        sb.AppendLine("};");



        sb.Append("static r_vector3_t initialPositions[] = { ");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];
            var position = gameObject.transform.localPosition;
            sb.Append($"{{{position.x}, {position.y}, {position.z}}}, ");
        }
        sb.AppendLine("};");
        sb.Append("static r_vector3_t initialRotations[] = { ");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];
            var rotation = gameObject.transform.localEulerAngles;
            sb.Append($"{{{rotation.x}, {rotation.y}, {rotation.z}}}, ");
        }
        sb.AppendLine("};");
        sb.Append("static r_vector3_t initialScales[] = { ");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            var gameObject = ds.gameObjects[gameObjectNum];
            var scale = gameObject.transform.localScale;
            sb.Append($"{{{scale.x}, {scale.y}, {scale.z}}}, ");
        }
        sb.AppendLine("};");
        sb.AppendLine("");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            sb.Append($"static size_t children_{gameObjectNum}[] = {{ ");
            var gameObject = ds.gameObjects[gameObjectNum];
            for (int childNum = 0; childNum < gameObject.transform.childCount; childNum++)
            {
                sb.Append($"{ds.gameObjectIndex[gameObject.transform.GetChild(childNum).gameObject]}, ");
            }
            sb.Append("SIZE_MAX, ");
            sb.AppendLine($"}}; //{gameObject.name}");
        }
        sb.Append($"static size_t* children[] = {{ ");
        for (int gameObjectNum = 0; gameObjectNum < ds.gameObjects.Count; gameObjectNum++)
        {
            sb.Append($" children_{gameObjectNum}, ");
        }
        sb.AppendLine("};");
        sb.AppendLine("");

        sb.AppendLine("void InitializeHierarchy(std::vector<game_object_t*> gameObjects) {");
        sb.AppendLine(" for (size_t gameObjectNum = 0; gameObjectNum < gameObjects.size(); gameObjectNum++) {");
        sb.AppendLine("  if (parents[gameObjectNum] != SIZE_MAX) {");
        sb.AppendLine("   gameObjects[gameObjectNum]->parent = gameObjects[parents[gameObjectNum]];");
        sb.AppendLine("  }");
        sb.AppendLine("  gameObjects[gameObjectNum]->children = children[gameObjectNum];");
        sb.AppendLine("  gameObjects[gameObjectNum]->position = initialPositions[gameObjectNum];");
        sb.AppendLine("  gameObjects[gameObjectNum]->rotation = initialRotations[gameObjectNum];");
        sb.AppendLine("  gameObjects[gameObjectNum]->scale = initialScales[gameObjectNum];");
        sb.AppendLine(" }");
        sb.AppendLine("}");

        File.WriteAllText("hierarchy.cpp", sb.ToString());
    }

    static void ProcessAnimations(DreamScene ds)
    {

        StringBuilder sb = new StringBuilder();

        sb.AppendLine("#include \"animations.h\"");
        sb.AppendLine();

        var animators = ds.animators;

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
                sb.AppendLine($"animator_t animator_{animatorNum} = {{ nullptr, nullptr, 0, {ds.gameObjectIndex[animator.gameObject]} }};");
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
            sb.AppendLine($"animator_t animator_{animatorNum} = {{ nullptr, animator_{animatorNum}_bindlist, {animatorController.animationClips.Length}, {ds.gameObjectIndex[animator.gameObject]} }};");
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
        File.WriteAllText("animations.cpp", sb.ToString());
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

    class DreamFont
    {
        public Font font;
        public FontStyle style;
        public int size;

        public DreamFont(Font font, FontStyle style, int size)
        {
            this.font = font;
            this.style = style;
            this.size = size;
        }

        public override int GetHashCode()
        {
            return font.GetHashCode();
        }

        public override bool Equals(object obj)
        {
            var other = obj as DreamFont;

            if (other != null)
            {
                return font == other.font && style == other.style && size == other.size;
            }

            return false;
        }

        // Overload == and != for consistency
        public static bool operator ==(DreamFont left, DreamFont right)
        {
            if (left is null) return right is null;

            return left.Equals(right);
        }

        public static bool operator !=(DreamFont left, DreamFont right)
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

        public HashSet<Component> rejectedComponents = new HashSet<Component>();

        // components
        public List<Animator> animators;
        public Dictionary<Animator, int> animatorIndex;

        public List<Camera> cameras;
        public Dictionary<Camera, int> cameraIndex;

        // scripts
        public List<ProximityInteractable> proximityInteractables;
        public Dictionary<ProximityInteractable, int> proximityInteractableIndex;

        public List<gameobjectactiveinactive2> gameobjectactiveinactive2s;
        public Dictionary<gameobjectactiveinactive2, int> gameobjectactiveinactive2Index;

        public List<timedactiveinactive> timedactiveinactives;
        public Dictionary<timedactiveinactive, int> timedactiveinactiveIndex;

        public List<Fadein> fadeins;
        public Dictionary<Fadein, int> fadeinIndex;

        public List<PlayerMovement2> playerMovement2s;
        public Dictionary<PlayerMovement2, int> playerMovement2Index;

        public List<MouseLook> mouseLooks;
        public Dictionary<MouseLook, int> mouseLookIndex;

        public List<Interactable> interactables;
        public Dictionary<Interactable, int> interactableIndex;

        // Physics
        public List<Rigidbody> rigidbodies;
        public Dictionary<Rigidbody, int> rigidbodyIndex;

        public List<BoxCollider> boxColliders;
        public Dictionary<BoxCollider, int> boxColliderIndex;

        public List<SphereCollider> sphereColliders;
        public Dictionary<SphereCollider, int> sphereColliderIndex;

        public List<CapsuleCollider> capsuleColliders;
        public Dictionary<CapsuleCollider, int> capsuleColliderIndex;

        public List<MeshCollider> meshColliders;
        public Dictionary<MeshCollider, int> meshColliderIndex;

        // ui
        public List<Text> texts;
        public Dictionary<Text, int> textIndex;

        public List<DreamFont> dfonts;
        public Dictionary<DreamFont, int> dfontIndex;

        public List<Font> fonts;
        public Dictionary<Font, int> fontIndex;
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


    static List<T> GetSceneComponents<T>() where T : Component
    {
        var rgo = SceneManager.GetActiveScene().GetRootGameObjects();

        var components = new List<T>();

        foreach (var go in rgo)
        {
            components.AddRange(go.GetComponentsInChildren<T>(true));
        }

        return components;
    }

    private static Dictionary<T, int> CreateComponentIndex<T>(List<T> components) where T : Component
    {
        var componentIndex = new Dictionary<T, int>();
        for (int i = 0; i < components.Count; i++)
        {
            componentIndex[components[i]] = i;
        }
        return componentIndex;
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

        // components
        ds.animators = GetSceneComponents<Animator>();
        ds.animatorIndex = CreateComponentIndex(ds.animators);

        ds.cameras = GetSceneComponents<Camera>();
        ds.cameraIndex = CreateComponentIndex(ds.cameras);

        // scripts
        ds.proximityInteractables = GetSceneComponents<ProximityInteractable>();
        ds.proximityInteractableIndex = CreateComponentIndex(ds.proximityInteractables);
        
        ds.gameobjectactiveinactive2s = GetSceneComponents<gameobjectactiveinactive2>();
        ds.gameobjectactiveinactive2Index = CreateComponentIndex(ds.gameobjectactiveinactive2s);

        ds.timedactiveinactives = GetSceneComponents<timedactiveinactive>();
        ds.timedactiveinactiveIndex = CreateComponentIndex(ds.timedactiveinactives);

        ds.fadeins = GetSceneComponents<Fadein>();
        ds.fadeinIndex = CreateComponentIndex(ds.fadeins);

        ds.playerMovement2s = GetSceneComponents<PlayerMovement2>();
        ds.playerMovement2Index = CreateComponentIndex(ds.playerMovement2s);

        ds.mouseLooks = GetSceneComponents<MouseLook>();
        ds.mouseLookIndex = CreateComponentIndex(ds.mouseLooks);

        ds.interactables = GetSceneComponents<Interactable>();
        ds.interactableIndex = CreateComponentIndex(ds.interactables);

        // Physics
        ds.rigidbodies = GetSceneComponents<Rigidbody>();
        ds.rigidbodyIndex = CreateComponentIndex(ds.rigidbodies);

        ds.boxColliders = GetSceneComponents<BoxCollider>();
        ds.boxColliderIndex = CreateComponentIndex(ds.boxColliders);

        ds.sphereColliders = GetSceneComponents<SphereCollider>();
        ds.sphereColliderIndex = CreateComponentIndex(ds.sphereColliders);

        ds.capsuleColliders = GetSceneComponents<CapsuleCollider>();
        ds.capsuleColliderIndex = CreateComponentIndex(ds.capsuleColliders);

        var meshColliders = GetSceneComponents<MeshCollider>();
        foreach(var mc in meshColliders.Where(mc => mc.sharedMesh == null))
        {
            if (mc.sharedMesh == null)
            {
                ds.rejectedComponents.Add(mc);
            }
        }
        ds.meshColliders = meshColliders.Where(mc => mc.sharedMesh != null).ToList();
        ds.meshColliderIndex = CreateComponentIndex(ds.meshColliders);

        // ui
        ds.texts = GetSceneComponents<Text>();
        ds.textIndex = CreateComponentIndex(ds.texts);

        ds.dfonts = new List<DreamFont>();
        ds.dfontIndex = new Dictionary<DreamFont, int>();

        foreach(var text in ds.texts)
        {
            var font = new DreamFont(text.font, text.fontStyle, text.fontSize);
            if (!ds.dfonts.Contains(font))
            {
                ds.dfonts.Add(font);
                ds.dfontIndex[font] = ds.dfonts.Count - 1;
            }
        }

        ds.fonts = new List<Font>();
        ds.fontIndex = new Dictionary<Font, int>();

        foreach (var dfont in ds.dfonts)
        {
            var font = dfont.font;
            if (!ds.fonts.Contains(font))
            {
                ds.fonts.Add(font);
                ds.fontIndex[font] = ds.fonts.Count - 1;
            }
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
