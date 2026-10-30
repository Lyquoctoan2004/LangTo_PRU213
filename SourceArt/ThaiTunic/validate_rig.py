import bpy, math, json, os
from mathutils import Quaternion, Vector
from mathutils.bvhtree import BVHTree
body=bpy.data.objects['SK_BaseBody']
cloth=bpy.data.objects['SK_ThaiTunic']
pants=bpy.data.objects['SK_ThaiPants']
arms=list({o.parent.name:o.parent for o in [body,cloth,pants]}.values())
backup={a.name:{p.name:(p.matrix_basis.copy(),p.rotation_mode) for p in a.pose.bones} for a in arms}
poses={
 'rest':{},
 'arms_down':{'upperarm_l':('Y',35),'upperarm_r':('Y',-35)},
 'arms_up':{'upperarm_l':('Y',-65),'upperarm_r':('Y',65)},
 'reach':{'upperarm_l':('Z',-55),'upperarm_r':('Z',55),'lowerarm_l':('Z',-55),'lowerarm_r':('Z',55)},
 'twist':{'spine_02':('Z',30),'spine_03':('Z',15)},
 'bend':{'spine_01':('X',25),'spine_02':('X',20)},
 'stride':{'thigh_l':('X',35),'thigh_r':('X',-35),'calf_l':('X',-25),'calf_r':('X',-25)}
}
def set_pose(name):
 for arm in arms:
  for p in arm.pose.bones:p.matrix_basis=backup[arm.name][p.name][0].copy()
  for bn,(axis,deg) in poses[name].items():
   p=arm.pose.bones[bn]
   v=Vector(tuple(1 if k==axis else 0 for k in 'XYZ'))
   v=p.bone.matrix_local.to_3x3().inverted()@v
   p.rotation_mode='QUATERNION'
   p.rotation_quaternion=Quaternion(v,math.radians(deg))
 bpy.context.view_layer.update()
def restore():
 for arm in arms:
  for p in arm.pose.bones:
   p.rotation_mode=backup[arm.name][p.name][1]
   p.matrix_basis=backup[arm.name][p.name][0]
 bpy.context.view_layer.update()
def evaluate_collision():
 dg=bpy.context.evaluated_depsgraph_get()
 be=body.evaluated_get(dg);ce=cloth.evaluated_get(dg)
 bm=be.to_mesh();cm=ce.to_mesh();bm.calc_loop_triangles()
 tree=BVHTree.FromPolygons([v.co for v in bm.vertices],[t.vertices[:] for t in bm.loop_triangles],all_triangles=True)
 distances=[]
 for v in cm.vertices:
  p,n,idx,d=tree.find_nearest(v.co)
  distances.append((v.co-p).dot(n))
 be.to_mesh_clear();ce.to_mesh_clear()
 return {'inside_2mm':sum(d<-.002 for d in distances),'inside_any':sum(d<0 for d in distances),'minimum_clearance_mm':round(min(distances)*1000,3)}
def run_tests():
 report={}
 try:
  for name in poses:
   set_pose(name);report[name]=evaluate_collision()
 finally:restore()
 return report
report=run_tests()
print(json.dumps(report))
