import bpy,collections,json,os
from mathutils import Matrix
from mathutils.bvhtree import BVHTree
root=os.path.dirname(__file__)
exec(compile(open(os.path.join(root,'validate_rig.py')).read(),'validate_rig.py','exec'))
body.data.calc_loop_triangles()
tris=[t.vertices[:] for t in body.data.loop_triangles]
verts=[v.co.copy() for v in body.data.vertices]
tree=BVHTree.FromPolygons(verts,tris,all_triangles=True)
original=[v.co.copy() for v in cloth.data.vertices]
def bary(p,a,b,c):
 v0=b-a;v1=c-a;v2=p-a
 d00=v0.dot(v0);d01=v0.dot(v1);d11=v1.dot(v1);d20=v2.dot(v0);d21=v2.dot(v1)
 den=d00*d11-d01*d01
 if abs(den)<1e-20:return (1,0,0)
 v=(d11*d20-d01*d21)/den;w=(d00*d21-d01*d20)/den
 values=[max(0,1-v-w),max(0,v),max(0,w)];s=sum(values)
 return [x/s for x in values]
weights=[{body.vertex_groups[g.group].name:g.weight for g in v.groups} for v in body.data.vertices]
for group in cloth.vertex_groups:group.remove(range(len(cloth.data.vertices)))
for v in cloth.data.vertices:
 p,n,idx,d=tree.find_nearest(v.co)
 tri=tris[idx];blend=bary(p,*[verts[i] for i in tri]);values=collections.defaultdict(float)
 for vi,alpha in zip(tri,blend):
  for name,w in weights[vi].items():values[name]+=alpha*w
 values=sorted([(name,w) for name,w in values.items() if w>.0001],key=lambda pair:-pair[1])[:8]
 total=sum(w for name,w in values)
 for name,w in values:
  vg=cloth.vertex_groups.get(name) or cloth.vertex_groups.new(name=name)
  vg.add([v.index],w/total,'REPLACE')
 clearance=(v.co-p).dot(n)
 if clearance<.005:v.co+=n*(.005-clearance)
cloth.data.update()
print('After weight transfer',json.dumps(run_tests()))
try:
 for iteration in range(8):
  changed=0
  for name in poses:
   set_pose(name)
   dg=bpy.context.evaluated_depsgraph_get();be=body.evaluated_get(dg);ce=cloth.evaluated_get(dg)
   bm=be.to_mesh();cm=ce.to_mesh();bm.calc_loop_triangles()
   tree=BVHTree.FromPolygons([v.co for v in bm.vertices],[t.vertices[:] for t in bm.loop_triangles],all_triangles=True)
   transforms={p.name:(p.matrix@p.bone.matrix_local.inverted()).to_3x3() for p in cloth.parent.pose.bones}
   adjustments=[]
   for v in cm.vertices:
    p,n,i,d=tree.find_nearest(v.co);signed=(v.co-p).dot(n)
    if signed<.002:
     src=cloth.data.vertices[v.index];mat=Matrix(((0,0,0),(0,0,0),(0,0,0)))
     for g in src.groups:mat+=transforms[cloth.vertex_groups[g.group].name]*g.weight
     pos=src.co+mat.inverted_safe()@(n*(.003-signed))
     offset=pos-original[v.index]
     if offset.length>.03:pos=original[v.index]+offset.normalized()*.03
     adjustments.append((v.index,pos))
   be.to_mesh_clear();ce.to_mesh_clear()
   for idx,pos in adjustments:cloth.data.vertices[idx].co=pos
   cloth.data.update();changed+=len(adjustments)
  print('clearance iteration',iteration,changed)
  if changed==0:break
finally:restore()
after=run_tests()
max_delta=max((v.co-original[v.index]).length for v in cloth.data.vertices)
print('FINAL',json.dumps(after),'maximum position correction mm',max_delta*1000)
with open(os.path.join(root,'rig_after.json'),'w') as f:json.dump({'poses':after,'maximum_correction_mm':max_delta*1000},f,indent=2)
