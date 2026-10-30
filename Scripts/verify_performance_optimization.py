import unreal


unreal.EditorLoadingAndSavingUtils.load_map("/Game/Mountain")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

cull_components = 0
cull_instances = 0
shadowless_components = 0
landscape_nanite = []

for actor in actors:
    if isinstance(actor, unreal.Landscape):
        landscape_nanite.append((actor.get_actor_label(), actor.get_editor_property("enable_nanite")))
    for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        _, end_distance = component.get_cull_distances()
        if end_distance > 0:
            cull_components += 1
            cull_instances += component.get_instance_count()
        if not component.get_editor_property("cast_shadow"):
            shadowless_components += 1

console_values = {
    "r.RayTracing": unreal.SystemLibrary.get_console_variable_int_value("r.RayTracing"),
    "r.Nanite.Foliage": unreal.SystemLibrary.get_console_variable_int_value("r.Nanite.Foliage"),
    "r.Streaming.MipBias": unreal.SystemLibrary.get_console_variable_float_value("r.Streaming.MipBias"),
    "r.ViewDistanceScale": unreal.SystemLibrary.get_console_variable_float_value("r.ViewDistanceScale"),
    "r.ScreenPercentage": unreal.SystemLibrary.get_console_variable_float_value("r.ScreenPercentage"),
    "foliage.DensityScale": unreal.SystemLibrary.get_console_variable_float_value("foliage.DensityScale"),
}

print("LANDSCAPE_NANITE=" + repr(landscape_nanite))
print("CULLED_COMPONENTS=%d" % cull_components)
print("CULLED_INSTANCES=%d" % cull_instances)
print("SHADOWLESS_COMPONENTS=%d" % shadowless_components)
print("PERFORMANCE_CVARS=" + repr(console_values))
