"""Blender에서 독립 테스트 워크스페이스를 만들어 static/mixed/실패 경로를 검증한다."""

import json
import math
import sys
import tempfile
from pathlib import Path

import bpy

scripts = Path(__file__).resolve().parents[1] / 'Scripts'
sys.path.insert(0, str(scripts))
from convert_asset import Convert, BakeRestRotation
from gltf_asset import Document, NodePaths, ReadJSON, Require, Validate, WorldMatrix, WriteJSON


def Clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)


def ExportFBX(path):
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=False, add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, path_mode='COPY', embed_textures=True)


def Recipe(name, source, **options):
    value = {'schemaVersion': 1, 'name': name, 'source': str(source), 'output': f'Game/Content/{name}', 'includeSourceAnimations': True}
    value.update(options)
    path = workspace / f'{name}.json'
    WriteJSON(path, value)
    return path


def ExpectFailure(callback, text):
    try:
        callback()
    except ValueError as error:
        Require(text in str(error), f'Unexpected error: {error}')
        return
    raise AssertionError(f'Expected failure: {text}')


repository = Path(__file__).resolve().parents[3]
testRoot = repository / 'Intermediate/AssetPipeline/Tests'
testRoot.mkdir(parents=True, exist_ok=True)
workspace = Path(tempfile.mkdtemp(prefix='run-', dir=testRoot))
profile = ReadJSON(scripts.parent / 'Profiles/SeonEngine.json')

# 애니메이션이 있는 다중 재질 static FBX. 엔진이 모든 면과 객체 애니메이션을 읽어야 한다.
Clear()
bpy.context.scene.render.fps = 30
bpy.context.scene.render.fps_base = 1
bpy.context.scene.frame_start = 1
bpy.context.scene.frame_end = 31
bpy.ops.mesh.primitive_cube_add()
cube = bpy.context.object
cube.name = 'AnimatedCube'
for name in ('Red', 'Blue'):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (1, 0, 0, 1) if name == 'Red' else (0, 0, 1, 1)
    if name == 'Red':
        material.use_nodes = True
        image = bpy.data.images.new('Texture With Spaces', width=4, height=4)
        image.generated_color = (0.8, 0.2, 0.1, 1)
        image.filepath_raw = str(workspace / 'Texture With Spaces.jpg')
        image.file_format = 'JPEG'
        image.save()
        texture = material.node_tree.nodes.new('ShaderNodeTexImage')
        texture.image = image
        shader = next(node for node in material.node_tree.nodes if node.type == 'BSDF_PRINCIPLED')
        material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
    cube.data.materials.append(material)
for face in cube.data.polygons:
    face.material_index = face.index % 2
cube.location.x = 0
cube.keyframe_insert(data_path='location', frame=1)
cube.location.x = 2
cube.keyframe_insert(data_path='location', frame=31)
staticSource = workspace / 'Static.fbx'
ExportFBX(staticSource)
staticRecipe = Recipe('Static', staticSource, sampleRate=60)
Convert(staticRecipe, workspace, False)
staticPath = workspace / 'Game/Content/Static/Static.gltf'
staticResult = Validate(staticPath, profile)
Require(staticResult['classification'] == 'static' and len(staticResult['meshes']) == 2, 'Static classification/material split failed')
Require(staticResult['triangles'] == 12 and staticResult['animations'], 'Static geometry/animation was lost')
Require(abs(staticResult['animations'][0]['durationSeconds'] - 1) < 0.0001, 'Resampling changed the clip duration')
staticDocument = Document(staticPath)
firstSampler = staticDocument.data['animations'][0]['samplers'][0]
Require(len(staticDocument.Read(firstSampler['input'])) == 61, '60 Hz resampling failed')
Require(staticDocument.data.get('images') and all(image['uri'].endswith('.png') and '%' not in image['uri'] and ' ' not in image['uri'] for image in staticDocument.data['images']), 'Texture conversion/naming failed')
ExpectFailure(lambda: Convert(staticRecipe, workspace, False), 'Output already exists')
Convert(staticRecipe, workspace, True)

# 형상이 없는 뼈를 포함해도 실제 가중치가 없는 메시를 skeletal로 오분류하면 안 된다.
Clear()
bpy.ops.mesh.primitive_cube_add()
plain = bpy.context.object
plain.name = 'Plain'
bpy.ops.object.armature_add()
rig = bpy.context.object
rig.name = 'Rig'
bpy.ops.mesh.primitive_cube_add(location=(3, 0, 0))
skin = bpy.context.object
skin.name = 'Skin'
group = skin.vertex_groups.new(name='Bone')
group.add(list(range(len(skin.data.vertices))), 1, 'REPLACE')
modifier = skin.modifiers.new('Armature', 'ARMATURE')
modifier.object = rig
skin.parent = rig
mixedSource = workspace / 'Mixed.fbx'
ExportFBX(mixedSource)
mixedRecipe = Recipe('Mixed', mixedSource, includeSourceAnimations=False)
Convert(mixedRecipe, workspace, False)
mixedPath = workspace / 'Game/Content/Mixed/Mixed.gltf'
mixedResult = Validate(mixedPath, profile)
Require(mixedResult['classification'] == 'mixed' and mixedResult['skins'] == 1, 'Mixed classification failed')

# 검증 실패가 게시된 정상 결과를 변경하지 않아야 한다.
before = staticPath.read_bytes()
badRecipe = ReadJSON(staticRecipe)
badRecipe['clips'] = [{'name': 'Bad', 'source': str(mixedSource)}]
WriteJSON(staticRecipe, badRecipe)
ExpectFailure(lambda: Convert(staticRecipe, workspace, True), 'exactly one target armature')
Require(staticPath.read_bytes() == before, 'Failed conversion damaged published asset')

# 채널 시각이 달라지면 현재 엔진이 트랙을 버리므로 validator가 차단해야 한다.
document = Document(staticPath)
animation = document.data['animations'][0]
channel = animation['channels'][0]
sampler = animation['samplers'][channel['sampler']]
sampler['interpolation'] = 'CUBICSPLINE'
corrupt = staticPath.parent / 'Unsupported.gltf'
WriteJSON(corrupt, document.data)
ExpectFailure(lambda: Validate(corrupt, profile), 'Unsupported animation interpolation')

# 재변환 시 사용자가 수정한 결과물은 -Force로도 덮어쓰지 않는다.
mixedPath.write_text(mixedPath.read_text(encoding='utf-8') + ' ', encoding='utf-8')
ExpectFailure(lambda: Convert(mixedRecipe, workspace, True), 'Generated file was edited')
mixedPath.write_text(mixedPath.read_text(encoding='utf-8')[:-1], encoding='utf-8')
# write_text의 개행 변환 대신 manifest 원본을 복구해 probe 입력을 유지한다.
manifest = ReadJSON(mixedPath.parent / 'AssetManifest.json')
stagedMixed = Path(manifest['stage']) / 'Asset/Mixed.gltf'
mixedPath.write_bytes(stagedMixed.read_bytes())

# 실제 XBot의 부모 축이 회전된 골반에서도 월드 수평 이동만 제거됐는지 검사한다.
xbotPath = repository / 'Game/Content/Characters/XBot/XBot.gltf'
if xbotPath.is_file():
    xbot = Document(xbotPath)
    _, parents = NodePaths(xbot.data['nodes'])
    hips = next(index for index, node in enumerate(xbot.data['nodes']) if node.get('name') == 'mixamorig:Hips')
    matrix = WorldMatrix(xbot.data['nodes'], parents, parents.get(hips))
    for animation in xbot.data['animations']:
        channel = next(channel for channel in animation['channels'] if channel['target'] == {'node': hips, 'path': 'translation'})
        values = xbot.Read(animation['samplers'][channel['sampler']]['output'])
        world = [[sum(matrix[row][column] * value[column] for column in range(3)) + matrix[row][3] for row in range(3)] for value in values]
        for axis in (0, 2):
            Require(max(point[axis] for point in world) - min(point[axis] for point in world) < 1e-5, 'In-place motion still drifts horizontally')
        Require(max(point[1] for point in world) - min(point[1] for point in world) > 0.0001, 'In-place mode erased vertical hip motion')

# 기준 뼈 회전이 다른 두 동일 계층 리그에서 보정된 회전과 타깃 뼈 길이를 검사한다.
Clear()
rigs = []
for name, roll in [('SourceRig', 0.0), ('TargetRig', 0.2)]:
    bpy.ops.object.armature_add()
    obj = bpy.context.object
    obj.name = name
    bpy.ops.object.mode_set(mode='EDIT')
    obj.data.edit_bones[0].roll = roll
    bpy.ops.object.mode_set(mode='OBJECT')
    rigs.append(obj)
sourceRig, targetRig = rigs
sourceBone = sourceRig.pose.bones[0]
sourceBone.rotation_mode = 'QUATERNION'
from mathutils import Quaternion
for frame, angle in [(1, 0.0), (11, 0.7)]:
    sourceBone.rotation_quaternion = Quaternion((0, 0, 1), angle)
    sourceBone.keyframe_insert(data_path='rotation_quaternion', frame=frame)
BakeRestRotation(targetRig, sourceRig, sourceRig.animation_data.action)
for frame in (1, 6, 11):
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    sourceRest = sourceRig.data.bones[0].matrix_local.to_quaternion()
    targetRest = targetRig.data.bones[0].matrix_local.to_quaternion()
    expected = sourceBone.matrix.to_quaternion() @ sourceRest.inverted() @ targetRest
    actual = targetRig.pose.bones[0].matrix.to_quaternion()
    Require(abs(expected.rotation_difference(actual).angle) < 0.001, 'Rest-rotation bake changed the expected rotation')

result = {'status': 'passed', 'workspace': str(workspace), 'probeAssets': [str(staticPath), str(mixedPath)], 'checks': ['animated static mesh', 'multiple materials preserve all triangles', '60 Hz resampling preserves duration', 'JPEG/space-named texture to PNG', 'mixed static and skeletal mesh', 'explicit reconversion', 'existing output protection', 'failed conversion preserves output', 'unsupported interpolation rejected', 'edited output protection', 'XBot world-space in-place with preserved hip height', 'rest-rotation bake matches source rotation delta']}
WriteJSON(testRoot / 'Results.json', result)
print('INTEGRATION_TESTS_PASSED ' + json.dumps(result))
