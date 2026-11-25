"""외부 리소스 glTF 읽기, 애니메이션 통합과 엔진 호환성 검사. bpy 의존성 없음."""

import bisect
import json
import math
import struct
from pathlib import Path
from urllib.parse import unquote, urlparse


COMPONENTS = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
WIDTHS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def ReadJSON(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def WriteJSON(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\r\n')


def Require(condition, message):
    if not condition:
        raise ValueError(message)


def ResourcePath(directory, uri):
    Require(isinstance(uri, str) and uri and not urlparse(uri).scheme, f'External relative URI required: {uri}')
    Require('\\' not in uri and not Path(uri).is_absolute(), f'Invalid resource URI: {uri}')
    path = (directory / unquote(uri)).resolve()
    Require(path.is_relative_to(directory.resolve()), f'Resource escapes asset directory: {uri}')
    return path


class Document:
    def __init__(self, path):
        self.path = Path(path)
        self.data = ReadJSON(path)
        self.buffers = []
        for buffer in self.data.get('buffers', []):
            payload = ResourcePath(self.path.parent, buffer.get('uri')).read_bytes()
            Require(len(payload) >= buffer['byteLength'], 'Truncated buffer')
            self.buffers.append(bytearray(payload))

    def Read(self, index):
        accessor = self.data['accessors'][index]
        Require('sparse' not in accessor and 'bufferView' in accessor, f'Dense accessor required: {index}')
        view = self.data['bufferViews'][accessor['bufferView']]
        code, size = COMPONENTS[accessor['componentType']]
        width = WIDTHS[accessor['type']]
        elementSize = size * width
        stride = view.get('byteStride', elementSize)
        offset = accessor.get('byteOffset', 0)
        count = accessor['count']
        Require(count > 0 and stride >= elementSize, f'Invalid accessor layout: {index}')
        end = offset + (count - 1) * stride + elementSize
        Require(offset >= 0 and end <= view['byteLength'], f'Accessor outside view: {index}')
        payload = self.buffers[view['buffer']]
        start = view.get('byteOffset', 0)
        Require(start >= 0 and start + view['byteLength'] <= len(payload), 'View outside buffer')
        values = [list(struct.unpack_from('<' + code * width, payload, start + offset + item * stride)) for item in range(count)]
        if accessor.get('normalized'):
            divisor = {5120: 127, 5121: 255, 5122: 32767, 5123: 65535}[accessor['componentType']]
            values = [[max(-1.0, component / divisor) for component in row] for row in values]
        Require(all(math.isfinite(component) for row in values for component in row), f'Non-finite accessor: {index}')
        return values

    def Append(self, values, kind):
        Require(bool(values), 'Cannot append empty accessor')
        payload = self.buffers[0]
        payload.extend(b'\0' * ((-len(payload)) % 4))
        offset = len(payload)
        width = WIDTHS[kind]
        for row in values:
            payload.extend(struct.pack('<' + 'f' * width, *row))
        views = self.data.setdefault('bufferViews', [])
        views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(payload) - offset})
        accessor = {'bufferView': len(views) - 1, 'componentType': 5126, 'count': len(values), 'type': kind}
        if kind == 'SCALAR':
            accessor.update(min=[min(row[0] for row in values)], max=[max(row[0] for row in values)])
        self.data.setdefault('accessors', []).append(accessor)
        return len(self.data['accessors']) - 1

    def Save(self):
        for description, payload in zip(self.data['buffers'], self.buffers):
            description['byteLength'] = len(payload)
            ResourcePath(self.path.parent, description['uri']).write_bytes(payload)
        WriteJSON(self.path, self.data)


def Sample(times, values, time, rotation):
    index = bisect.bisect_right(times, time)
    if index == 0:
        return values[0][:]
    if index == len(times):
        return values[-1][:]
    a, b = values[index - 1][:], values[index][:]
    alpha = (time - times[index - 1]) / (times[index] - times[index - 1])
    if rotation:
        dot = sum(x * y for x, y in zip(a, b))
        if dot < 0:
            b = [-component for component in b]
            dot = -dot
        if dot < 0.9995:
            angle = math.acos(min(1.0, dot))
            first = math.sin((1 - alpha) * angle) / math.sin(angle)
            second = math.sin(alpha * angle) / math.sin(angle)
            result = [first * x + second * y for x, y in zip(a, b)]
        else:
            result = [(1 - alpha) * x + alpha * y for x, y in zip(a, b)]
        length = math.sqrt(sum(component * component for component in result))
        Require(length > 0, 'Zero-length quaternion')
        return [component / length for component in result]
    return [(1 - alpha) * x + alpha * y for x, y in zip(a, b)]


def NodePaths(nodes):
    parents = {}
    for index, node in enumerate(nodes):
        for child in node.get('children', []):
            Require(isinstance(child, int) and 0 <= child < len(nodes), 'Invalid child node')
            Require(child not in parents, 'Node has multiple parents')
            parents[child] = index
    paths = []
    for index in range(len(nodes)):
        names, visited = [], set()
        while True:
            Require(index not in visited, 'Cyclic node hierarchy')
            visited.add(index)
            names.append(nodes[index].get('name', ''))
            if index not in parents:
                break
            index = parents[index]
        paths.append(tuple(reversed(names)))
    return paths, parents


def MultiplyMatrix(first, second):
    return [[sum(first[row][index] * second[index][column] for index in range(4)) for column in range(4)] for row in range(4)]


def WorldMatrix(nodes, parents, index):
    identity = [[float(row == column) for column in range(4)] for row in range(4)]
    if index is None:
        return identity
    node = nodes[index]
    if 'matrix' in node:
        local = [[node['matrix'][column * 4 + row] for column in range(4)] for row in range(4)]
    else:
        x, y, z, w = node.get('rotation', [0, 0, 0, 1])
        scale = node.get('scale', [1, 1, 1])
        local = [[1 - 2*y*y - 2*z*z, 2*x*y - 2*z*w, 2*x*z + 2*y*w, 0], [2*x*y + 2*z*w, 1 - 2*x*x - 2*z*z, 2*y*z - 2*x*w, 0], [2*x*z - 2*y*w, 2*y*z + 2*x*w, 1 - 2*x*x - 2*y*y, 0], [0, 0, 0, 1]]
        for row in range(3):
            for column in range(3):
                local[row][column] *= scale[column]
            local[row][3] = node.get('translation', [0, 0, 0])[row]
    return MultiplyMatrix(WorldMatrix(nodes, parents, parents.get(index)), local)


def RemoveHorizontalMotion(values, parentMatrix):
    # 뼈 로컬 Y가 월드 위쪽이라는 보장은 없다. 부모 변환을 거쳐 월드 XZ 이동을 제거한다.
    a, b, c = parentMatrix[0][:3]
    d, e, f = parentMatrix[1][:3]
    g, h, i = parentMatrix[2][:3]
    determinant = a*(e*i-f*h) - b*(d*i-f*g) + c*(d*h-e*g)
    Require(abs(determinant) > 1e-15, 'Singular parent transform for in-place bone')
    inverseUp = [(c*h-b*i)/determinant, (a*i-c*g)/determinant, (b*g-a*h)/determinant]
    first = values[0][:]
    for index, row in enumerate(values):
        delta = [component - initial for component, initial in zip(row, first)]
        height = sum(parentMatrix[1][axis] * delta[axis] for axis in range(3))
        values[index] = [first[axis] + inverseUp[axis] * height for axis in range(3)]


def MergeAnimation(target, source, animation, name, inPlaceBone=None, sampleRate=30):
    targetPaths, targetParents = NodePaths(target.data['nodes'])
    sourcePaths, _ = NodePaths(source.data['nodes'])
    Require(len(targetPaths) == len(set(targetPaths)), 'Ambiguous target node paths')
    targetIndices = {path: index for index, path in enumerate(targetPaths)}
    channels = []
    for channel in animation['channels']:
        path = channel['target']['path']
        Require(path in ('translation', 'rotation', 'scale'), f'Unsupported animation path: {path}')
        sourceNode = channel['target']['node']
        nodePath = sourcePaths[sourceNode]
        Require(nodePath in targetIndices, f'Animation node not present in base model: {nodePath}')
        sampler = animation['samplers'][channel['sampler']]
        Require(sampler.get('interpolation', 'LINEAR') == 'LINEAR', 'Animations must be baked to LINEAR')
        times = [row[0] for row in source.Read(sampler['input'])]
        values = source.Read(sampler['output'])
        channels.append((targetIndices[nodePath], path, times, values))
    Require(channels, f'Empty animation: {name}')
    start = min(times[0] for _, _, times, _ in channels)
    end = max(times[-1] for _, _, times, _ in channels)
    duration = end - start
    Require(duration > 0, f'Animation has no duration: {name}')
    frameCount = math.ceil(duration * sampleRate)
    uniformTimes = [start + frame / sampleRate for frame in range(frameCount) if frame / sampleRate < duration - 1e-7] + [end]
    result = {'name': name, 'channels': [], 'samplers': []}
    inPlaceFound = False
    # 엔진은 같은 노드의 TRS 채널에 같은 시각 배열을 요구한다.
    for node in sorted({item[0] for item in channels}):
        tracks = [item for item in channels if item[0] == node]
        times = uniformTimes
        timeAccessor = target.Append([[time - start] for time in times], 'SCALAR')
        for _, path, keys, values in tracks:
            sampled = [Sample(keys, values, time, path == 'rotation') for time in times]
            if inPlaceBone and target.data['nodes'][node].get('name') == inPlaceBone and path == 'translation':
                ancestor = targetParents.get(node)
                while ancestor is not None:
                    Require(not any(item[0] == ancestor for item in channels), 'In-place mode requires non-animated ancestors of the selected bone')
                    ancestor = targetParents.get(ancestor)
                matrix = WorldMatrix(target.data['nodes'], targetParents, targetParents.get(node))
                RemoveHorizontalMotion(sampled, matrix)
                inPlaceFound = True
            output = target.Append(sampled, 'VEC4' if path == 'rotation' else 'VEC3')
            result['channels'].append({'sampler': len(result['samplers']), 'target': {'node': node, 'path': path}})
            result['samplers'].append({'input': timeAccessor, 'output': output, 'interpolation': 'LINEAR'})
    Require(not inPlaceBone or inPlaceFound, f'In-place translation bone missing: {inPlaceBone}')
    target.data.setdefault('animations', []).append(result)


def SplitPrimitives(document):
    data = document.data
    oldMeshes = data.get('meshes', [])
    meshes, mapping = [], {}
    for index, mesh in enumerate(oldMeshes):
        mapping[index] = []
        for part, primitive in enumerate(mesh['primitives']):
            mapping[index].append(len(meshes))
            meshes.append({'name': f"{mesh.get('name', 'Mesh')}_Part{part}", 'primitives': [primitive]})
    originalNodes = list(data.get('nodes', []))
    for node in originalNodes:
        if 'mesh' not in node:
            continue
        parts = mapping[node['mesh']]
        if len(parts) == 1:
            node['mesh'] = parts[0]
            continue
        del node['mesh']
        skin = node.pop('skin', None)
        for part, mesh in enumerate(parts):
            child = {'name': f"{node.get('name', 'Mesh')}_Part{part}", 'mesh': mesh}
            if skin is not None:
                child['skin'] = skin
            node.setdefault('children', []).append(len(data['nodes']))
            data['nodes'].append(child)
    data['meshes'] = meshes


def Validate(path, profile):
    document = Document(path)
    data = document.data
    Require(data.get('asset', {}).get('version') == '2.0', 'glTF 2.0 required')
    Require(not data.get('extensionsRequired'), 'Required extensions are unsupported')
    for index in range(len(data.get('accessors', []))):
        document.Read(index)
    nodes = data.get('nodes', [])
    _, parents = NodePaths(nodes)
    skins = data.get('skins', [])
    joints = {joint for skin in skins for joint in skin['joints']}
    warnings = []
    for skin in skins:
        Require(0 < len(skin['joints']) <= profile['maxJoints'], 'Joint limit exceeded or empty skin')
        Require(len(set(skin['joints'])) == len(skin['joints']), 'Duplicate skin joints')
        Require(all(0 <= joint < len(nodes) for joint in skin['joints']), 'Invalid skin joint')
        Require('inverseBindMatrices' in skin, 'Explicit inverse bind matrices required')
        accessor = data['accessors'][skin['inverseBindMatrices']]
        Require(accessor['type'] == 'MAT4' and accessor['componentType'] == 5126 and accessor['count'] == len(skin['joints']), 'Invalid inverse bind matrices')
    meshKinds = {}
    totalVertices = totalTriangles = 0
    for index, mesh in enumerate(data.get('meshes', [])):
        Require(len(mesh['primitives']) == 1, f'Mesh {index}: engine reads only the first primitive')
        primitive = mesh['primitives'][0]
        Require(primitive.get('mode', 4) == 4 and 'indices' in primitive, 'Indexed triangles required')
        Require(not primitive.get('targets'), 'Morph targets are unsupported')
        attributes = primitive['attributes']
        Require('POSITION' in attributes and 'NORMAL' in attributes, 'Positions and normals required')
        count = data['accessors'][attributes['POSITION']]['count']
        for semantic, kind in [('POSITION', 'VEC3'), ('NORMAL', 'VEC3'), ('TEXCOORD_0', 'VEC2')]:
            if semantic in attributes:
                accessor = data['accessors'][attributes[semantic]]
                Require(accessor['componentType'] == 5126 and accessor['type'] == kind and accessor['count'] == count, f'Invalid {semantic}')
        indices = document.Read(primitive['indices'])
        indexAccessor = data['accessors'][primitive['indices']]
        Require(indexAccessor['type'] == 'SCALAR' and indexAccessor['componentType'] in (5121, 5123, 5125), 'Invalid indices accessor')
        Require(len(indices) % 3 == 0 and all(0 <= row[0] < count for row in indices), 'Invalid triangle indices')
        Require(('JOINTS_0' in attributes) == ('WEIGHTS_0' in attributes), 'Incomplete skin attributes')
        skinned = 'JOINTS_0' in attributes
        meshKinds[index] = 'skeletal' if skinned else 'static'
        if skinned:
            Require(not any(key.startswith(('JOINTS_', 'WEIGHTS_')) and key not in ('JOINTS_0', 'WEIGHTS_0') for key in attributes), 'Only four influences supported')
            jointAccessor = data['accessors'][attributes['JOINTS_0']]
            weightAccessor = data['accessors'][attributes['WEIGHTS_0']]
            Require(jointAccessor['type'] == 'VEC4' and jointAccessor['componentType'] in (5121, 5123) and not jointAccessor.get('normalized'), 'Invalid joint accessor')
            Require(weightAccessor['type'] == 'VEC4' and weightAccessor['componentType'] == 5126, 'Float VEC4 weights required')
            Require(jointAccessor['count'] == weightAccessor['count'] == count, 'Skin vertex count mismatch')
            Require(all(min(row) >= 0 and abs(sum(row) - 1) < 0.001 for row in document.Read(attributes['WEIGHTS_0'])), 'Invalid skin weights')
        totalVertices += count
        totalTriangles += len(indices) // 3
    Require(meshKinds, 'No meshes to load')
    for index, node in enumerate(nodes):
        if 'mesh' not in node:
            continue
        Require(node['mesh'] in meshKinds, 'Invalid mesh node')
        ancestor = parents.get(index)
        while ancestor is not None:
            Require(ancestor not in joints, 'Engine skips mesh objects below joints')
            ancestor = parents.get(ancestor)
        skinned = meshKinds[node['mesh']] == 'skeletal'
        Require(skinned == ('skin' in node), 'Skin binding does not match mesh weights')
        if skinned:
            Require(0 <= node['skin'] < len(skins), 'Invalid skin binding')
            attributes = data['meshes'][node['mesh']]['primitives'][0]['attributes']
            limit = len(skins[node['skin']]['joints'])
            Require(all(0 <= joint < limit for row in document.Read(attributes['JOINTS_0']) for joint in row), 'Joint index outside skin')
    clips = []
    for animation in data.get('animations', []):
        groups, targets, duration = {}, set(), 0
        Require(animation.get('channels'), 'Empty animation')
        for channel in animation['channels']:
            node, path = channel['target']['node'], channel['target']['path']
            Require(0 <= node < len(nodes) and path in ('translation', 'rotation', 'scale'), 'Invalid animation target')
            Require((node, path) not in targets, 'Duplicate animation channel')
            targets.add((node, path))
            sampler = animation['samplers'][channel['sampler']]
            Require(sampler.get('interpolation', 'LINEAR') == profile['animationInterpolation'], 'Unsupported animation interpolation')
            inputAccessor, outputAccessor = data['accessors'][sampler['input']], data['accessors'][sampler['output']]
            Require(inputAccessor['componentType'] == outputAccessor['componentType'] == 5126 and inputAccessor['type'] == 'SCALAR', 'Float animation keys required')
            Require(outputAccessor['type'] == ('VEC4' if path == 'rotation' else 'VEC3'), 'Wrong animation value type')
            times = [row[0] for row in document.Read(sampler['input'])]
            values = document.Read(sampler['output'])
            Require(times and times[0] >= 0 and all(b > a for a, b in zip(times, times[1:])) and len(values) == len(times), 'Invalid animation timeline')
            Require(node not in groups or groups[node] == times, 'TRS channels must share key times')
            groups[node] = times
            if path == 'rotation':
                Require(all(abs(math.sqrt(sum(x * x for x in row)) - 1) <= 0.001 for row in values), 'Non-unit animation quaternion')
            duration = max(duration, times[-1])
        Require(duration > 0, 'Animation has no duration')
        clips.append({'name': animation.get('name'), 'durationSeconds': duration, 'channels': len(targets)})
    for image in data.get('images', []):
        imagePath = ResourcePath(document.path.parent, image.get('uri'))
        Require(image['uri'] == unquote(image['uri']), 'Engine does not decode percent-encoded URIs')
        Require(imagePath.read_bytes()[:8] == b'\x89PNG\r\n\x1a\n', 'PNG image required')
    for material in data.get('materials', []):
        if material.get('alphaMode') == 'MASK':
            warnings.append(f"{material.get('name')}: alpha MASK is rendered opaque by the engine")
        if material.get('doubleSided'):
            warnings.append(f"{material.get('name')}: doubleSided is not represented by the engine material")
        if material.get('extensions'):
            warnings.append(f"{material.get('name')}: optional material extensions are not represented by the engine material")
    kinds = set(meshKinds.values())
    return {'status': 'passed', 'classification': 'mixed' if len(kinds) > 1 else next(iter(kinds)), 'meshes': [{'index': index, 'name': data['meshes'][index].get('name'), 'kind': kind} for index, kind in meshKinds.items()], 'vertices': totalVertices, 'triangles': totalTriangles, 'skins': len(skins), 'jointCounts': [len(skin['joints']) for skin in skins], 'animations': clips, 'warnings': sorted(set(warnings))}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Validate a glTF against the SeonEngine profile')
    parser.add_argument('asset', type=Path)
    parser.add_argument('--profile', type=Path, default=Path(__file__).resolve().parents[1] / 'Profiles/SeonEngine.json')
    arguments = parser.parse_args()
    print(json.dumps(Validate(arguments.asset, ReadJSON(arguments.profile)), ensure_ascii=False, indent=2))
