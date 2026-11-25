"""Blender 백그라운드 전용 FBX 변환 진입점. 엔진 소스는 변경하지 않는다."""

import argparse
import hashlib
import json
import math
import re
import shutil
import sys
import traceback
from pathlib import Path

import bpy
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gltf_asset import Document, MergeAnimation, ReadJSON, Require, ResourcePath, SplitPrimitives, Validate, WriteJSON


def Fingerprint(path):
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def ImportFBX(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path), use_anim=True, automatic_bone_orientation=False, ignore_leaf_bones=False)
    return [obj for obj in bpy.data.objects if obj not in before]


def ActionSlot(obj, action):
    slots = list(action.slots)
    if obj.animation_data and obj.animation_data.action == action and obj.animation_data.action_slot:
        return obj.animation_data.action_slot
    matches = [slot for slot in slots if slot.target_id_type == obj.id_type]
    Require(len(matches) == 1, f'Action slot is ambiguous: {action.name}')
    return matches[0]


def Select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.hide_set(False)
        obj.select_set(True)
    if objects:
        bpy.context.view_layer.objects.active = objects[0]


def MatrixDifference(first, second):
    return max(abs(first[row][column] - second[row][column]) for row in range(4) for column in range(4))


def CheckSkeleton(target, source, tolerance, allowRootRestTranslation, retarget):
    first = {bone.name: bone for bone in target.data.bones}
    second = {bone.name: bone for bone in source.data.bones}
    Require(first.keys() == second.keys(), f'Skeleton bone names differ; retargeting is required. Missing: {sorted(first.keys() - second.keys())}; extra: {sorted(second.keys() - first.keys())}')
    offset = None
    if allowRootRestTranslation:
        roots = [bone for bone in first.values() if bone.parent is None]
        Require(len(roots) == 1, 'Root translation alignment requires exactly one skeleton root')
        offset = roots[0].matrix_local.translation - second[roots[0].name].matrix_local.translation
    for name, bone in first.items():
        other = second[name]
        parent = bone.parent.name if bone.parent else None
        otherParent = other.parent.name if other.parent else None
        Require(parent == otherParent, f'Skeleton hierarchy differs: {name}; retargeting is required')
        aligned = other.matrix_local.copy()
        if offset is not None:
            aligned.translation += offset
        if retarget:
            Require(abs(bone.length - other.length) <= max(tolerance, bone.length * 0.01), f'Bone proportions differ by more than 1%: {name}; general retargeting is required')
            Require(bone.use_inherit_rotation and other.use_inherit_rotation and bone.inherit_scale == other.inherit_scale == 'FULL', f'Unsupported bone inheritance: {name}')
        else:
            Require(MatrixDifference(bone.matrix_local, aligned) <= tolerance, f'Skeleton rest pose differs: {name}; retargeting is required')
    # 액션의 시작 위치 대신 FBX import가 보관한 객체 기준 변환을 비교한다.
    Require(MatrixDifference(target.matrix_parent_inverse, source.matrix_parent_inverse) <= tolerance, 'Armature parent spaces differ; retargeting is required')
    Require(target.parent is None and source.parent is None, 'External clips with parented armatures require explicit retargeting')
    return list(offset) if offset is not None else None


def BakeRestRotation(target, source, sourceAction):
    """동일 계층/유사 길이 뼈의 모델 공간 회전을 기준 자세에 보정하고 타깃 길이를 유지한다."""
    target.animation_data_create()
    target.animation_data.action = None
    objectMatrix = source.matrix_world.copy()
    first = math.floor(sourceAction.frame_range[0])
    last = math.ceil(sourceAction.frame_range[1])
    ordered = []
    pending = list(target.data.bones)
    while pending:
        ready = [bone for bone in pending if bone.parent is None or bone.parent.name in ordered]
        Require(ready, 'Cannot order skeleton')
        ordered.extend(bone.name for bone in ready)
        pending = [bone for bone in pending if bone not in ready]
    for frame in range(first, last + 1):
        bpy.context.scene.frame_set(frame)
        Require(MatrixDifference(objectMatrix, source.matrix_world) < 0.0001, 'Animated armature object transforms require explicit baking into a root bone')
        matrices = {}
        for name in ordered:
            bone = target.data.bones[name]
            other = source.data.bones[name]
            pose = source.pose.bones[name]
            rotation = pose.matrix.to_quaternion() @ other.matrix_local.to_quaternion().inverted() @ bone.matrix_local.to_quaternion()
            if bone.parent:
                restLocal = bone.parent.matrix_local.inverted() @ bone.matrix_local
                sourceRestLocal = other.parent.matrix_local.inverted() @ other.matrix_local
                sourcePoseLocal = source.pose.bones[other.parent.name].matrix.inverted() @ pose.matrix
                delta = sourcePoseLocal.translation - sourceRestLocal.translation
                position = matrices[bone.parent.name] @ (restLocal.translation + delta)
                parentPose, parentRest = matrices[bone.parent.name], bone.parent.matrix_local
            else:
                position = bone.matrix_local.translation + pose.matrix.translation - other.matrix_local.translation
                parentPose = parentRest = Matrix.Identity(4)
            desired = Matrix.LocRotScale(position, rotation, (1, 1, 1))
            matrices[name] = desired
            basis = bone.convert_local_to_pose(desired, bone.matrix_local, parent_matrix=parentPose, parent_matrix_local=parentRest, invert=True)
            targetPose = target.pose.bones[name]
            targetPose.matrix_basis = basis
            targetPose.rotation_mode = 'QUATERNION'
            for path in ('location', 'rotation_quaternion', 'scale'):
                targetPose.keyframe_insert(data_path=path, frame=frame, group=name)
    return target.animation_data.action


def PrepareMeshes(objects, profile, warnings):
    result = []
    for obj in objects:
        if obj.type != 'MESH':
            continue
        Require(not obj.data.shape_keys, f'{obj.name}: shape keys are unsupported; bake or remove them explicitly')
        Require(all(modifier.type == 'ARMATURE' for modifier in obj.modifiers), f'{obj.name}: non-armature modifiers require explicit baking')
        modifiers = [modifier for modifier in obj.modifiers if modifier.type == 'ARMATURE' and modifier.object]
        Require(len(modifiers) <= 1, f'{obj.name}: multiple armature modifiers unsupported')
        armature = modifiers[0].object if modifiers else None
        boneNames = {bone.name for bone in armature.data.bones} if armature else set()
        boneGroups = {group.index for group in obj.vertex_groups if group.name in boneNames}
        weighted = any(group.group in boneGroups and group.weight > 0 for vertex in obj.data.vertices for group in vertex.groups)
        if armature and not weighted:
            obj.modifiers.remove(modifiers[0])
            warnings.append(f'{obj.name}: armature modifier without effective skin weights removed; exported as static')
        if weighted:
            trimmed = 0
            for vertex in obj.data.vertices:
                influences = sorted([(group.group, group.weight) for group in vertex.groups if group.group in boneGroups and group.weight > 0], key=lambda value: (-value[1], value[0]))
                Require(influences, f'{obj.name}: unweighted vertex {vertex.index}')
                keep = influences[:profile['maxInfluences']]
                total = sum(weight for _, weight in keep)
                for group, _ in influences[len(keep):]:
                    obj.vertex_groups[group].remove([vertex.index])
                    trimmed += 1
                for group, weight in keep:
                    obj.vertex_groups[group].add([vertex.index], weight / total, 'REPLACE')
            if trimmed:
                warnings.append(f'{obj.name}: removed {trimmed} small influences and normalized the remaining weights')
        result.append({'name': obj.name, 'kind': 'skeletal' if weighted else 'static', 'vertices': len(obj.data.vertices)})
    Require(result, 'Source FBX contains no meshes; animation-only FBX must be added under clips with a base model')
    for image in bpy.data.images:
        if image.type == 'IMAGE' and image.source == 'FILE':
            imagePath = Path(bpy.path.abspath(image.filepath))
            Require(image.packed_file or imagePath.is_file(), f'Missing source texture: {image.filepath}')
    return result


def Export(objects, path, animations):
    Select(objects)
    # 크기/좌표는 glTF exporter가 표준 미터/Y-up으로 변환한다. 엔진 보정을 중복 적용하지 않는다.
    bpy.ops.export_scene.gltf(filepath=str(path), check_existing=False, export_format='GLTF_SEPARATE', use_selection=True, export_texcoords=True, export_normals=True, export_tangents=False, export_materials='EXPORT', export_image_format='AUTO', export_texture_dir='Textures', export_cameras=False, export_lights=False, export_yup=True, export_animations=animations, export_animation_mode='ACTIVE_ACTIONS', export_force_sampling=True, export_frame_step=1, export_frame_range=False, export_anim_slide_to_zero=True, export_optimize_animation_size=False, export_skins=True, export_influence_nb=4, export_all_influences=False, export_morph=False, export_morph_animation=False, export_try_sparse_sk=False, export_try_omit_sparse_sk=False, export_draco_mesh_compression_enable=False)


def ResolveSource(value, recipeDirectory):
    path = Path(value)
    path = path if path.is_absolute() else recipeDirectory / path
    path = path.resolve()
    Require(path.suffix.lower() == '.fbx' and path.is_file(), f'FBX file not found: {path}')
    return path


def Convert(recipePath, root, force):
    recipe = ReadJSON(recipePath)
    Require(recipe.get('schemaVersion') == 1, 'Unsupported recipe schemaVersion')
    name = recipe['name']
    Require(re.fullmatch(r'[A-Za-z0-9_-]+', name), 'Asset name must use ASCII letters, digits, underscore or hyphen')
    output = (root / recipe['output']).resolve()
    contentRoot = (root / 'Game/Content').resolve()
    Require(output.is_relative_to(contentRoot) and output != contentRoot, 'Output must be a subdirectory of Game/Content')
    Require(not output.exists() or force, f'Output already exists: {output}; use -Force for a previously generated asset')
    if output.exists():
        marker = output / 'AssetManifest.json'
        Require(marker.is_file() and ReadJSON(marker).get('generator') == 'SeonAssetPipeline', 'Refusing to overwrite a directory not owned by AssetPipeline')
        previous = ReadJSON(marker)
        Require(previous.get('asset') == name, 'Output belongs to a different asset')
        for relative, expectedHash in previous.get('files', {}).items():
            existing = ResourcePath(output, relative)
            Require(not existing.exists() or hashlib.sha256(existing.read_bytes()).hexdigest() == expectedHash, f'Generated file was edited; preserve it before reconverting: {existing}')
    sampleRate = recipe.get('sampleRate', 30)
    Require(isinstance(sampleRate, int) and 1 <= sampleRate <= 240, 'sampleRate must be an integer between 1 and 240')
    clips = recipe.get('clips', [])
    Require(len({clip['name'] for clip in clips}) == len(clips), 'Duplicate clip names')
    source = ResolveSource(recipe['source'], recipePath.parent)
    clipSources = [ResolveSource(clip['source'], recipePath.parent) for clip in clips]
    profilePath = Path(__file__).resolve().parents[1] / 'Profiles/SeonEngine.json'
    profile = ReadJSON(profilePath)
    Require(profile['maxInfluences'] == 4 and profile['animationInterpolation'] == 'LINEAR', 'Unsupported exporter profile')
    work = root / 'Intermediate/AssetPipeline' / name
    work.mkdir(parents=True, exist_ok=True)
    # 매 실행마다 별도 작업 폴더를 사용해 이전 결과와 실패 결과가 섞이지 않게 한다.
    import tempfile
    stage = Path(tempfile.mkdtemp(prefix='conversion-', dir=work))
    assetDirectory = stage / 'Asset'
    assetDirectory.mkdir()
    scriptHashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(Path(__file__).resolve().parent.glob('*.py'))}
    report = {'schemaVersion': 1, 'generator': 'SeonAssetPipeline', 'status': 'running', 'asset': name, 'blenderVersion': bpy.app.version_string, 'scriptHashes': scriptHashes, 'profile': profile, 'recipe': recipe, 'sources': [Fingerprint(path) for path in [source, *clipSources]], 'stage': str(stage), 'output': str(output), 'warnings': []}
    reportPath = work / 'Report.json'
    WriteJSON(reportPath, report)
    try:
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        bpy.context.scene.render.fps = sampleRate
        bpy.context.scene.render.fps_base = 1.0
        beforeActions = set(bpy.data.actions)
        objects = ImportFBX(source)
        sourceActions = set(bpy.data.actions) - beforeActions
        report['sourceMeshes'] = PrepareMeshes(objects, profile, report['warnings'])
        rigs = [obj for obj in objects if obj.type == 'ARMATURE']
        originalActions = []
        for obj in objects:
            if obj.animation_data and obj.animation_data.action:
                action = obj.animation_data.action
                originalActions.append((obj, action, ActionSlot(obj, action)))
            if obj.animation_data:
                obj.animation_data_clear()
            if obj.type == 'ARMATURE':
                for bone in obj.pose.bones:
                    bone.matrix_basis.identity()
        if recipe.get('includeSourceAnimations', True):
            Require(sourceActions <= {action for _, action, _ in originalActions}, 'Source FBX contains inactive animation takes; split takes before converting')
        elif sourceActions:
            report['warnings'].append('Source FBX animations were explicitly excluded by includeSourceAnimations=false')
        if any(obj.type not in ('MESH', 'ARMATURE', 'EMPTY') for obj in objects):
            report['warnings'].append('FBX cameras/lights are excluded from the mesh conversion')
        bpy.context.view_layer.update()
        targetRestMatrices = {obj: obj.matrix_world.copy() for obj in rigs}
        outputPath = assetDirectory / f'{name}.gltf'
        Export(objects, outputPath, False)
        document = Document(outputPath)
        if recipe.get('includeSourceAnimations', True) and originalActions:
            # 복수 객체에 배정된 활성 액션을 한 FBX 장면 애니메이션으로 내보낸다.
            for obj, action, slot in originalActions:
                obj.animation_data_create()
                obj.animation_data.action = action
                obj.animation_data.action_slot = slot
            tempPath = stage / 'Source.gltf'
            Export(objects, tempPath, True)
            exported = Document(tempPath)
            for index, animation in enumerate(exported.data.get('animations', [])):
                MergeAnimation(document, exported, animation, animation.get('name', f'Source{index}'), sampleRate=sampleRate)
            for obj, _, _ in originalActions:
                obj.animation_data_clear()
        for clip, clipSource in zip(clips, clipSources):
            Require(len(rigs) == 1, 'External clips currently require exactly one target armature')
            target = rigs[0]
            # 이전 클립의 포즈가 다음 FBX import와 비교에 영향을 주지 않게 초기화한다.
            target.animation_data_clear()
            for bone in target.pose.bones:
                bone.matrix_basis.identity()
            bpy.context.view_layer.update()
            beforeActions = set(bpy.data.actions)
            imported = ImportFBX(clipSource)
            clipActions = set(bpy.data.actions) - beforeActions
            sourceRigs = [obj for obj in imported if obj.type == 'ARMATURE']
            Require(len(sourceRigs) == 1, f'{clipSource.name}: expected exactly one source armature')
            sourceRig = sourceRigs[0]
            retarget = clip.get('retarget', 'none')
            Require(retarget in ('none', 'restRotation'), f'Unknown retarget mode: {retarget}')
            offset = CheckSkeleton(target, sourceRig, profile['skeletonTolerance'], clip.get('allowRootRestTranslation', False), retarget == 'restRotation')
            if offset is not None:
                report.setdefault('rootRestAlignments', []).append({'clip': clip['name'], 'armatureLocalOffset': offset})
            Require(sourceRig.animation_data and sourceRig.animation_data.action, f'{clipSource.name}: no active animation')
            action = sourceRig.animation_data.action
            slot = ActionSlot(sourceRig, action)
            actionName = clip.get('action')
            Require(not actionName or action.name == actionName, f'Active action is {action.name}, requested {actionName}')
            # 활성 액션 하나만 지원한다. 여러 take를 조용히 버리지 않는다.
            importedActions = {obj.animation_data.action for obj in imported if obj.animation_data and obj.animation_data.action}
            Require(len(importedActions) == 1, 'External FBX contains multiple active actions; split takes before converting')
            Require(clipActions == importedActions, 'External FBX contains inactive animation takes; split takes before converting')
            bpy.context.scene.frame_set(int(action.frame_range[0]))
            bpy.context.view_layer.update()
            # 오브젝트 스케일과 정적 좌표계가 다르면 같은 뼈 이름이어도 직접 재사용할 수 없다.
            Require(MatrixDifference(sourceRig.matrix_world, target.matrix_world) <= profile['skeletonTolerance'], 'Armature object transforms differ; explicit retargeting is required')
            if retarget == 'restRotation':
                BakeRestRotation(target, sourceRig, action)
                report['warnings'].append(f'{clip["name"]}: baked restRotation retarget; identical hierarchy and bone lengths within 1% verified')
            else:
                target.animation_data_create()
                target.animation_data.action = action
                target.animation_data.action_slot = slot
            clipPath = stage / f'Clip{len(document.data.get("animations", []))}.gltf'
            Export(objects, clipPath, True)
            exported = Document(clipPath)
            animations = exported.data.get('animations', [])
            Require(len(animations) == 1, f'{clipSource.name}: expected one exported animation, got {len(animations)}')
            MergeAnimation(document, exported, animations[0], clip['name'], clip.get('inPlaceBone'), sampleRate)
            target.animation_data_clear()
            for obj in imported:
                bpy.data.objects.remove(obj, do_unlink=True)
            target.matrix_world = targetRestMatrices[target]
        SplitPrimitives(document)
        # exporter의 URI 인코딩을 제거하고 충돌 없는 ASCII 리소스 이름을 만든다.
        for index, image in enumerate(document.data.get('images', [])):
            imagePath = ResourcePath(assetDirectory, image['uri'])
            safePath = assetDirectory / 'Textures' / f'Texture{index}.png'
            safePath.parent.mkdir(exist_ok=True)
            if imagePath.read_bytes()[:8] == b'\x89PNG\r\n\x1a\n':
                if imagePath != safePath:
                    shutil.copyfile(imagePath, safePath)
            else:
                imageData = bpy.data.images.load(str(imagePath), check_existing=False)
                Require(len(imageData.pixels) > 0, f'Cannot decode exported texture: {imagePath}')
                imageData.file_format = 'PNG'
                imageData.save(filepath=str(safePath))
                bpy.data.images.remove(imageData)
            image['uri'] = f'Textures/{safePath.name}'
        document.Save()
        validation = Validate(outputPath, profile)
        report['validation'] = validation
        report['warnings'].extend(validation['warnings'])
        expected = {clip['name'] for clip in clips}
        actual = {clip['name'] for clip in validation['animations']}
        Require(expected <= actual, f'Missing exported clips: {expected - actual}')
        definition = {'schemaVersion': 1, 'model': f'{recipe["output"].rstrip("/")}/{name}.gltf', 'classification': validation['classification'], 'clips': {clip['name']: {'animationName': clip['name'], 'durationSeconds': clip['durationSeconds']} for clip in validation['animations']}}
        WriteJSON(assetDirectory / 'AssetDefinition.json', definition)
        # 재현 정보는 출력에도 남기되 아직 구현하지 않은 캐릭터 로더가 읽는 파일로 가장하지 않는다.
        report['status'] = 'passed'
        resources = {f'{name}.gltf', 'AssetDefinition.json'}
        resources.update(buffer['uri'] for buffer in document.data.get('buffers', []))
        resources.update(image['uri'] for image in document.data.get('images', []))
        if output.exists():
            previousFiles = ReadJSON(output / 'AssetManifest.json').get('files', {})
            for relative in resources:
                Require(not (output / relative).exists() or relative in previousFiles, f'Output would overwrite an untracked file: {relative}')
        report['files'] = {relative: hashlib.sha256((assetDirectory / relative).read_bytes()).hexdigest() for relative in sorted(resources)}
        WriteJSON(assetDirectory / 'AssetManifest.json', report)
        # 검증에 성공한 파일만 게시한다. 기존 파일 전체를 삭제하지 않는다.
        output.mkdir(parents=True, exist_ok=True)
        for relative in sorted(resources | {'AssetManifest.json'}):
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(assetDirectory / relative, destination)
        WriteJSON(reportPath, report)
        print('ASSET_PIPELINE_SUCCESS ' + json.dumps({'asset': str(outputPath), 'published': str(output), 'validation': validation}, ensure_ascii=False))
    except Exception as error:
        report['status'] = 'failed'
        report['error'] = str(error)
        WriteJSON(reportPath, report)
        raise


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--recipe', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--force', action='store_true')
    arguments = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    try:
        Convert(arguments.recipe.resolve(), arguments.root.resolve(), arguments.force)
    except Exception:
        traceback.print_exc()
        sys.exit(1)
