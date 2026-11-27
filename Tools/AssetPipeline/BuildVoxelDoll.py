"""Original voxel teddy: exposed voxel faces merged into six articulated glTF parts.

No external packages or downloaded art. Coordinates are authored in engine cm,
then converted to glTF metres with the loader's X reflection accounted for.
"""
import json
import math
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / 'Game/Content/BabyDoll/VoxelDoll'
OUT.mkdir(parents=True, exist_ok=True)
CELL = 4
COLORS = [(0.82, .30, .45), (.22, .62, .78), (.92, .77, .64),
          (.035, .028, .04), (.85, .045, .09), (.98, .94, .84), (.16, .08, .10)]
FACES = [((1,0,0),(0,0,-1),(0,1,0)),((-1,0,0),(0,0,1),(0,1,0)),
         ((0,1,0),(1,0,0),(0,0,-1)),((0,-1,0),(1,0,0),(0,0,1)),
         ((0,0,1),(1,0,0),(0,1,0)),((0,0,-1),(-1,0,0),(0,1,0))]


def ellipsoid(voxels, center, radii, color):
    for x in range(math.floor((center[0]-radii[0])/CELL), math.ceil((center[0]+radii[0])/CELL)):
        for y in range(math.floor((center[1]-radii[1])/CELL), math.ceil((center[1]+radii[1])/CELL)):
            for z in range(math.floor((center[2]-radii[2])/CELL), math.ceil((center[2]+radii[2])/CELL)):
                p = ((x+.5)*CELL, (y+.5)*CELL, (z+.5)*CELL)
                if sum(((p[i]-center[i])/radii[i])**2 for i in range(3)) <= 1:
                    voxels[x,y,z] = color(p) if callable(color) else color


def write(name, voxels):
    blob, views, accessors, primitives = bytearray(), [], [], []

    def add(rows, kind, width, fmt, component):
        while len(blob) % 4: blob.append(0)
        offset = len(blob)
        for row in rows: blob.extend(struct.pack('<'+fmt*width, *row))
        views.append(dict(buffer=0, byteOffset=offset, byteLength=len(blob)-offset))
        a = dict(bufferView=len(views)-1, componentType=component, count=len(rows), type=kind)
        if kind == 'VEC3':
            a.update(min=[min(p[i] for p in rows) for i in range(3)], max=[max(p[i] for p in rows) for i in range(3)])
        accessors.append(a)
        return len(accessors)-1

    for color in sorted(set(voxels.values())):
        positions, normals, uvs, indices = [], [], [], []
        for cell, material in sorted(voxels.items()):
            if material != color: continue
            for n, u, v in FACES:
                if tuple(cell[i]+n[i] for i in range(3)) in voxels: continue
                begin = len(positions)
                for a,b in [(-1,-1),(1,-1),(1,1),(-1,1)]:
                    p = [(cell[i]+.5)*CELL + CELL*.5*(n[i]+u[i]*a+v[i]*b) for i in range(3)]
                    positions.append((-p[0]/100,p[1]/100,p[2]/100))
                    normals.append((-n[0],n[1],n[2]))
                    uvs.append(((a+1)/2,(b+1)/2))
                indices.extend(begin+i for i in [0,2,1,0,3,2])
        primitives.append(dict(attributes=dict(POSITION=add(positions,'VEC3',3,'f',5126),
                                              NORMAL=add(normals,'VEC3',3,'f',5126),
                                              TEXCOORD_0=add(uvs,'VEC2',2,'f',5126)),
                               indices=add([(i,) for i in indices],'SCALAR',1,'I',5125), material=color))
    data = dict(asset=dict(version='2.0',generator='BabyDoll original voxel asset generator'),scene=0,
                scenes=[dict(nodes=[0])],nodes=[dict(children=list(range(1,len(primitives)+1)))] + [dict(mesh=i) for i in range(len(primitives))],
                meshes=[dict(primitives=[primitive]) for primitive in primitives],
                materials=[dict(name='Voxel_'+str(i),pbrMetallicRoughness=dict(baseColorFactor=[*c,1],metallicFactor=0,roughnessFactor=.92)) for i,c in enumerate(COLORS)],
                buffers=[dict(uri=name+'.bin',byteLength=len(blob))],bufferViews=views,accessors=accessors)
    (OUT/(name+'.gltf')).write_text(json.dumps(data,separators=(',',':')),encoding='utf-8')
    (OUT/(name+'.bin')).write_bytes(blob)
    print(name, len(voxels), 'voxels', sum(accessors[p['indices']]['count'] for p in primitives)//3, 'triangles')


split = lambda p: 0 if p[0] < 0 else 1
v = {}
ellipsoid(v,(0,0,0),(25,27,17),split)
ellipsoid(v,(0,-3,13),(16,18,7),2)
# Central stitched seam, visible on the back while playing.
for y in range(-5,7):
    for x in [-1,0]: v[x,y,-5] = 6
    if y % 3 == 0:
        for x in [-2,-1,0,1]: v[x,y,-5] = 5
ellipsoid(v,(0,-15,-17),(10,9,8),2)
# Clean red bow, knot and two tails on the back.
ellipsoid(v,(-11,13,-19),(12,9,5),4)
ellipsoid(v,(11,13,-19),(12,9,5),4)
ellipsoid(v,(0,12,-23),(5,6,5),4)
ellipsoid(v,(-5,2,-20),(4,12,4),4)
ellipsoid(v,(5,2,-20),(4,12,4),4)
write('Body',v)
v = {}
ellipsoid(v,(0,0,0),(29,24,20),split)
for x in [-23,23]:
    ellipsoid(v,(x,21,0),(12,12,10),0 if x<0 else 1)
    ellipsoid(v,(x,22,7),(7,7,5),2)
ellipsoid(v,(0,-8,17),(16,10,8),2)
ellipsoid(v,(0,-4,24),(6,4,3),3)
for x in [-13,13]:
    ellipsoid(v,(x,4,18),(5,6,5),3)
    ellipsoid(v,(x-1,6,22),(2.5,2.5,2.5),5)
# Tear on the blue cheek and asymmetrical mouth stitches.
ellipsoid(v,(16,-6,19),(3,7,3),5)
for x,y in [(-9,-13),(-5,-15),(0,-15),(5,-13)]: ellipsoid(v,(x,y,23),(2.5,2.5,2.5),6)
write('Head',v)
for name,color in [('LeftArm',0),('RightArm',1)]:
    v={}
    ellipsoid(v,(0,-12,0),(10,21,11),color)
    ellipsoid(v,(0,-25,3),(9,9,10),2)
    write(name,v)
for name,color in [('LeftLeg',0),('RightLeg',1)]:
    v={}
    ellipsoid(v,(0,-10,0),(11,16,11),color)
    ellipsoid(v,(0,-19,6),(12,9,17),color)
    ellipsoid(v,(0,-20,18),(7,5,4),2)
    write(name,v)
(OUT/'Source.json').write_text(json.dumps(dict(description='Original procedural voxel teddy for the BabyDoll prototype',
    generator='Tools/AssetPipeline/BuildVoxelDoll.py',voxelSizeCm=CELL,
    animation='Game procedural articulated animation, replaceable independently of gameplay',
    externalAssets=False),indent=2),encoding='utf-8')
