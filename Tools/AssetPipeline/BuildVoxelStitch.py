"""Original split teddy antagonist for testing Stitches' opening/tentacle rules."""
import BuildVoxelDoll as voxel

voxel.COLORS = [(.48,.12,.24),(.10,.30,.43),(.66,.48,.35),(.025,.012,.018),(.65,.025,.06),(.94,.87,.68),(.12,.04,.045)]
split=lambda p: 0 if p[0]<0 else 1
body={}
voxel.ellipsoid(body,(0,150,0),(75,85,50),split)
head={}
voxel.ellipsoid(head,(0,260,0),(80,60,55),split)
for x in [-65,65]:
    voxel.ellipsoid(head,(x,305,0),(28,28,24),0 if x<0 else 1)
    voxel.ellipsoid(head,(x,308,18),(16,16,10),2)
# Blind stitched eye sockets.
for x in [-35,35]:
    voxel.ellipsoid(head,(x,270,48),(18,12,12),3)
    for offset in [-8,0,8]:voxel.ellipsoid(head,(x+offset,270,59),(2.5,16,3),5)
for name,cells in [('StitchTorso',body),('StitchHead',head)]:
    voxel.write(name+'Left',{p:c for p,c in cells.items() if p[0]<0})
    voxel.write(name+'Right',{p:c for p,c in cells.items() if p[0]>=0})
