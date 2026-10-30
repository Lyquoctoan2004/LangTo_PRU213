import unreal


unreal.EditorLoadingAndSavingUtils.load_map("/Game/Mountain")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

restored_landscapes = 0
restored_components = 0
restored_instances = 0
restored_shadows = 0

for actor in actors:
    if isinstance(actor, unreal.Landscape) and not actor.get_editor_property("enable_nanite"):
        actor.modify()
        actor.set_editor_property("enable_nanite", True)
        restored_landscapes += 1

    for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        _, end_distance = component.get_cull_distances()
        if end_distance <= 0:
            continue

        component.modify()
        component.set_cull_distances(0, 0)
        restored_components += 1
        restored_instances += component.get_instance_count()

        if not component.get_editor_property("cast_shadow"):
            component.set_editor_property("cast_shadow", True)
            restored_shadows += 1

if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError("Failed to save the restored Mountain level")

print("RESTORED_LANDSCAPES=%d" % restored_landscapes)
print("RESTORED_COMPONENTS=%d" % restored_components)
print("RESTORED_INSTANCES=%d" % restored_instances)
print("RESTORED_SHADOW_COMPONENTS=%d" % restored_shadows)
