import bpy, math, json
from mathutils import Quaternion, Vector
from mathutils.bvhtree import BVHTree
body=bpy.data.objects['SK_BaseBody']
pants=bpy.data.objects['SK_ThaiPants']
arms=[body.parent,pants.parent]
backup={a.name:{p.name:(p.matrix_basis.copy(),p.rotation_mode) for p in a.pose.bones} for a in arms}
def set_pose(thigh,calf):
 for arm in arms:
  for p in arm.pose.bones:
   p.matrix_basis=backup[arm.name][p.name][0].copy()
  for suffix,sign in [('l',1),('r',-1)]:
   for bn,deg in [('thigh',thigh*sign),('calf',calf)]:
    p=arm.pose.bones[bn+'_'+suffix]
    axis=p.bone.matrix_local.to_3x3().inverted()@Vector((1,0,0))
    p.rotation_mode='QUATERNION'
    p.rotation_quaternion=Quaternion(axis,math.radians(deg))
 bpy.context.view_layer.update()
def penetration():
 dg=bpy.context.evaluated_depsgraph_get()
 be=body.evaluated_get(dg)
 pe=pants.evaluated_get(dg)
 bm=be.to_mesh()
 pm=pe.to_mesh()
 bm.calc_loop_triangles()
 tree=BVHTree.FromPolygons([v.co for v in bm.vertices],[tuple(t.vertices) for t in bm.loop_triangles],all_triangles=True)
 ds=[]
 for v in pm.vertices:
  p,n,i,d=tree.find_nearest(v.co)
  ds.append((v.co-p).dot(n))
 be.to_mesh_clear()
 pe.to_mesh_clear()
 return {'inside_2mm':sum(d<-.002 for d in ds),'worst_mm':round(min(ds)*1000,2)}
results={}
try:
 for label,t,c in [('rest',0,0),('stride',35,-25),('high_step',65,-80)]:
  set_pose(t,c)
  results[label]=penetration()
finally:
 for arm in arms:
  for p in arm.pose.bones:
   p.rotation_mode=backup[arm.name][p.name][1]
   p.matrix_basis=backup[arm.name][p.name][0]
 bpy.context.view_layer.update()
print(json.dumps(results))
