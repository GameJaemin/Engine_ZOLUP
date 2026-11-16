using System;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

internal sealed class DevMapExporter : EditorWindow
{
    private const float MetersToCentimeters = 100f;
    private static string OutputPathKey => "com.zolup.devmap-exporter.outputPath." + Application.dataPath;

    private string outputPath;
    private string status;
    private MessageType statusType;
    [SerializeField] private ExportScope exportScope = ExportScope.ActiveScene;
    private int? selectedSceneHandle;

    private enum ExportScope
    {
        ActiveScene,
        Selection
    }

    [Serializable]
    private sealed class MapData
    {
        public ObjectData[] objects;
    }

    [Serializable]
    private sealed class ObjectData
    {
        public int id;
        public string name;
        public TransformData transform;
        public ComponentData[] components = Array.Empty<ComponentData>();
    }

    [Serializable]
    private sealed class ComponentData
    {
        public string type = string.Empty;
    }

    [Serializable]
    private sealed class TransformData
    {
        public float[] position;
        public float[] rotation;
        public float[] scale;
    }

    [MenuItem("Tools/ZOLUP/DevMap Exporter")]
    private static void Open()
    {
        GetWindow<DevMapExporter>("DevMap Exporter");
    }

    private void OnEnable()
    {
        minSize = new Vector2(440, 150);
        outputPath = EditorPrefs.GetString(OutputPathKey, string.Empty);
        EditorApplication.hierarchyChanged += Repaint;
        EditorSceneManager.activeSceneChangedInEditMode += OnActiveSceneChanged;
    }

    private void OnDisable()
    {
        EditorApplication.hierarchyChanged -= Repaint;
        EditorSceneManager.activeSceneChangedInEditMode -= OnActiveSceneChanged;
    }

    private void OnActiveSceneChanged(Scene previousScene, Scene nextScene)
    {
        Repaint();
    }

    private void OnGUI()
    {
        exportScope = (ExportScope)EditorGUILayout.EnumPopup("Export Scope", exportScope);
        bool isMainStage = StageUtility.GetCurrentStageHandle() == StageUtility.GetMainStageHandle();
        bool canExport = isMainStage && !EditorApplication.isPlayingOrWillChangePlaymode;
        Scene scene;
        if (exportScope == ExportScope.Selection)
        {
            scene = DrawScenePicker();
        }
        else
        {
            scene = SceneManager.GetActiveScene();
            EditorGUILayout.LabelField("Active Scene", GetSceneLabel(scene));
        }

        using (new EditorGUILayout.HorizontalScope())
        {
            EditorGUILayout.SelectableLabel(
                string.IsNullOrEmpty(outputPath) ? "Choose a JSON output file" : outputPath,
                EditorStyles.textField, GUILayout.Height(EditorGUIUtility.singleLineHeight));
            if (GUILayout.Button("Browse…", GUILayout.Width(85)))
                ChooseOutputPath();
        }

        bool hasSource = scene.IsValid() && scene.isLoaded;
        using (new EditorGUI.DisabledScope(!canExport || !hasSource || string.IsNullOrEmpty(outputPath)))
        {
            string label = File.Exists(outputPath) ? "Export JSON (Overwrite)" : "Export JSON";
            if (GUILayout.Button(label))
            {
                var objects = scene.GetRootGameObjects()
                    .SelectMany(root => root.GetComponentsInChildren<Transform>(true))
                    .Select(transform => transform.gameObject)
                    .ToArray();
                Export(objects);
            }
        }

        if (!string.IsNullOrEmpty(status))
            EditorGUILayout.HelpBox(status, statusType);
    }

    private Scene DrawScenePicker()
    {
        var scenes = Enumerable.Range(0, SceneManager.sceneCount)
            .Select(SceneManager.GetSceneAt)
            .Where(scene => scene.IsValid() && scene.isLoaded && !EditorSceneManager.IsPreviewScene(scene))
            .ToArray();
        var labels = new string[scenes.Length + 1];
        labels[0] = scenes.Length == 0 ? "No loaded scenes" : "Choose an open scene";
        for (int i = 0; i < scenes.Length; i++)
            labels[i + 1] = GetSceneLabel(scenes[i]);

        int currentIndex = Array.FindIndex(scenes, scene => scene.handle == selectedSceneHandle) + 1;
        int index = EditorGUILayout.Popup("Scene", currentIndex, labels);
        if (index <= 0)
        {
            selectedSceneHandle = null;
            return default;
        }

        Scene selectedScene = scenes[index - 1];
        selectedSceneHandle = selectedScene.handle;
        return selectedScene;
    }

    private static string GetSceneLabel(Scene scene)
    {
        if (!string.IsNullOrEmpty(scene.path))
            return scene.path;

        string name = string.IsNullOrEmpty(scene.name) ? "Untitled" : scene.name;
        return $"{name} (unsaved, {scene.handle})";
    }

    private void ChooseOutputPath()
    {
        string directory = string.IsNullOrEmpty(outputPath)
            ? Path.GetDirectoryName(Application.dataPath)
            : Path.GetDirectoryName(outputPath);
        string fileName = string.IsNullOrEmpty(outputPath)
            ? "DevMap"
            : Path.GetFileNameWithoutExtension(outputPath);
        string path = EditorUtility.SaveFilePanel("Choose DevMap Output", directory, fileName, "json");
        if (string.IsNullOrEmpty(path))
            return;

        outputPath = path;
        EditorPrefs.SetString(OutputPathKey, outputPath);
        status = string.Empty;
    }

    private void Export(GameObject[] objects)
    {
        try
        {
            var map = new MapData
            {
                objects = objects.Select((obj, index) => ExportObject(obj, index + 1)).ToArray()
            };
            File.WriteAllText(outputPath, JsonUtility.ToJson(map, true) + "\n", new UTF8Encoding(false));
            status = $"Exported {map.objects.Length} objects to {outputPath}";
            statusType = MessageType.Info;
            Debug.Log(status);
        }
        catch (Exception exception) when (exception is IOException ||
            exception is UnauthorizedAccessException || exception is InvalidOperationException)
        {
            status = exception.Message;
            statusType = MessageType.Error;
        }
    }

    private static ObjectData ExportObject(GameObject obj, int id)
    {
        Transform source = obj.transform;
        Vector3 position = source.position * MetersToCentimeters;
        Quaternion rotation = source.rotation.normalized;
        if (!IsFinite(position) || !IsFinite(new Vector3(rotation.x, rotation.y, rotation.z)) ||
            float.IsNaN(rotation.w) || float.IsInfinity(rotation.w))
        {
            throw new InvalidOperationException($"'{obj.name}' has an invalid world position or rotation.");
        }

        // Express the world-space basis in the object's rotation frame to recover signed scale.
        Matrix4x4 world = source.localToWorldMatrix;
        Quaternion inverseRotation = Quaternion.Inverse(rotation);
        Vector3 x = inverseRotation * (Vector3)world.GetColumn(0);
        Vector3 y = inverseRotation * (Vector3)world.GetColumn(1);
        Vector3 z = inverseRotation * (Vector3)world.GetColumn(2);
        if (!IsAxisAligned(x, Vector3.right) || !IsAxisAligned(y, Vector3.up) ||
            !IsAxisAligned(z, Vector3.forward))
        {
            throw new InvalidOperationException(
                $"'{obj.name}' cannot be exported as a world rotation and scale. Check for rotated children under scaled parents.");
        }

        return new ObjectData
        {
            id = id,
            name = obj.name,
            transform = new TransformData
            {
                position = new[] { position.x, position.y, position.z },
                rotation = new[] { rotation.x, rotation.y, rotation.z, rotation.w },
                scale = new[] { x.x, y.y, z.z }
            }
        };
    }

    private static bool IsAxisAligned(Vector3 basis, Vector3 axis)
    {
        Vector3 offAxis = basis - Vector3.Project(basis, axis);
        float tolerance = Mathf.Abs(Vector3.Dot(basis, axis)) * 0.0001f;
        return IsFinite(basis) && offAxis.magnitude <= tolerance;
    }

    private static bool IsFinite(Vector3 value)
    {
        return !float.IsNaN(value.x) && !float.IsInfinity(value.x) &&
            !float.IsNaN(value.y) && !float.IsInfinity(value.y) &&
            !float.IsNaN(value.z) && !float.IsInfinity(value.z);
    }
}
