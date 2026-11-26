"""테스트룸용 단위 상자와 바닥 링을 외부 glTF로 생성한다."""
import json
import math
import struct
from pathlib import Path

out = Path('Game/Content/BabyDoll/Geometry')
out.mkdir(parents=True, exist_ok=True)

def write(name, positions, normals, uvs, indices):
    blob = bytearray()
    views, accessors = [], []
    def add(values, kind, width, fmt, component):
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        for row in values:
            blob.extend(struct.pack('<' + fmt * width, *row))
        views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(blob) - offset})
        accessors.append({'bufferView': len(views) - 1, 'componentType': component, 'count': len(values), 'type': kind})
        if kind == 'VEC3':
            accessors[-1]['min'] = [min(row[i] for row in values) for i in range(3)]
            accessors[-1]['max'] = [max(row[i] for row in values) for i in range(3)]
        return len(accessors) - 1
    p = add(positions, 'VEC3', 3, 'f', 5126)
    n = add(normals, 'VEC3', 3, 'f', 5126)
    uv = add(uvs, 'VEC2', 2, 'f', 5126)
    ix = add([(i,) for i in indices], 'SCALAR', 1, 'H', 5123)
    data = {'asset': {'version': '2.0'}, 'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}], 'meshes': [{'primitives': [{'attributes': {'POSITION': p, 'NORMAL': n, 'TEXCOORD_0': uv}, 'indices': ix, 'material': 0}]}], 'materials': [{'pbrMetallicRoughness': {'metallicFactor': 0, 'roughnessFactor': 0.8}}], 'buffers': [{'uri': name + '.bin', 'byteLength': len(blob)}], 'bufferViews': views, 'accessors': accessors}
    (out / (name + '.bin')).write_bytes(blob)
    (out / (name + '.gltf')).write_text(json.dumps(data), encoding='utf-8')

positions, normals, uvs, indices = [], [], [], []
for normal, axis_u, axis_v in [((1,0,0),(0,0,-1),(0,1,0)),((-1,0,0),(0,0,1),(0,1,0)),((0,1,0),(1,0,0),(0,0,-1)),((0,-1,0),(1,0,0),(0,0,1)),((0,0,1),(1,0,0),(0,1,0)),((0,0,-1),(-1,0,0),(0,1,0))]:
    begin = len(positions)
    for u,v in [(-1,-1),(1,-1),(1,1),(-1,1)]:
        positions.append(tuple((normal[i] + axis_u[i]*u + axis_v[i]*v)*0.5 for i in range(3)))
        normals.append(normal)
        uvs.append(((u+1)/2,(v+1)/2))
    indices.extend(begin+i for i in [0,1,2,0,2,3])
write('Box', positions, normals, uvs, indices)
positions, normals, uvs, indices = [], [], [], []
for i in range(97):
    a = 2*math.pi*i/96
    for radius in [0.985, 1.0]:
        positions.append((math.cos(a)*radius, 0, math.sin(a)*radius))
        normals.append((0,1,0))
        uvs.append((i/96, radius))
for i in range(96):
    b=i*2
    indices.extend([b,b+2,b+1,b+1,b+2,b+3])
write('Ring', positions, normals, uvs, indices)
print('Generated Box and Ring glTF')
