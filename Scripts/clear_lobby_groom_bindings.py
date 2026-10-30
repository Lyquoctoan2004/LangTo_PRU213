"""Clear groom binding assets on every groom component in the Lobby level.

The binding never becomes valid against the Mutable-generated character mesh, and
projecting it while Mutable regenerates the mesh asserts in
HairStrandsMeshProjection.cpp (MeshPositionBuffer0 == Section.PositionBuffer) on PIE start.
The strands follow the head through the head-bone attachment (GroomRebindSubsystem),
so the binding is not needed.
"""
import unreal

LEVEL = "/Game/Lobby/Lobby"

unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

cleared = 0
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    for groom in actor.get_components_by_class(unreal.GroomComponent):
        binding = groom.get_editor_property("binding_asset")
        unreal.log_warning("GroomBinding: %s.%s groom=%s binding=%s" % (
            actor.get_name(), groom.get_name(),
            groom.get_editor_property("groom_asset"), binding))
        if binding:
            actor.modify()
            groom.modify()
            groom.set_editor_property("binding_asset", None)
            cleared += 1

unreal.log_warning("GroomBinding: cleared %d binding(s)" % cleared)
if cleared:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
