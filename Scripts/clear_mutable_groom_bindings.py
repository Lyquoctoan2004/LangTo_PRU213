"""Clear BindingAsset on every Groom node in the character Customizable Objects.

HairStrandsMutableExtension creates the groom components with the node's binding and
renders them in the same frame. In PIE the Mutable mesh sections do not share one
skin-cache position buffer, so the binding's mesh projection asserts in
HairStrandsMeshProjection.cpp (MeshPositionBuffer0 == Section.PositionBuffer).
The strands follow the head through GroomRebindSubsystem's head-bone attachment.
"""
import unreal

ASSETS = [
    "/Game/Character/Grooms/Aoi/CO_Groom_Hair_Aoi",
    "/Game/Character/Grooms/Nasim/CO_Groom_Hair_Nasim",
    "/Game/Character/Grooms/Nasim/CO_Groom_Hair_Nasim_NomadWaveUndercut",
    "/Game/Character/Grooms/Nasim/CO_Groom_Hair_Nasim_UpdoMessyBun",
]

nodes = []
for path in ASSETS:
    co = unreal.load_asset(path)
    name = path.rsplit("/", 1)[1]
    for i in range(32):
        node = unreal.find_object(None, "%s.%s:CustomizableObjectGraph_0.CustomizableObjectNodeGroomConstant_%d" % (path, name, i))
        if node:
            node.modify()
            co.modify()
            nodes.append(node)
unreal.log_warning("GroomBinding: found %d groom node(s)" % len(nodes))

# GroomData is not exposed to Python; the SET command imports text into every instance.
unreal.SystemLibrary.execute_console_command(None, "set CustomizableObjectNodeGroomConstant GroomData (BindingAsset=None)")

for path in ASSETS:
    ok = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log_warning("GroomBinding: saved %s -> %s" % (path, ok))
