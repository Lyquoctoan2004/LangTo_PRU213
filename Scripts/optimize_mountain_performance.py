import collections

import unreal


MAP_PATH = "/Game/Mountain"


def choose_cull_distance(mesh_path):
    path = mesh_path.lower()
    if "grass" in path:
        return 8000, 14000, True, "grass"
    if any(token in path for token in ("/plants/", "moneyplant", "fern", "ivy", "vine", "shrub")):
        return 18000, 30000, True, "small_plant"
    if any(token in path for token in ("boulder", "forestrock", "smallrock", "riverbedrock")):
        return 35000, 60000, False, "small_rock"
    if any(token in path for token in ("spruce", "tree", "hornbeam", "bamboo")):
        return 90000, 150000, False, "tree"
    return None


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsystem.get_all_level_actors()

optimized_components = 0
optimized_instances = 0
shadowless_components = 0
landscapes_changed = 0
category_counts = collections.Counter()

for actor in actors:
    if isinstance(actor, unreal.Landscape):
        if actor.get_editor_property("enable_nanite"):
            actor.modify()
            actor.set_editor_property("enable_nanite", False)
            landscapes_changed += 1

    for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        if not mesh:
            continue
        mesh_path = mesh.get_path_name()
        settings = choose_cull_distance(mesh_path)
        if not settings:
            continue

        start_distance, end_distance, disable_shadow, category = settings
        component.modify()
        component.set_cull_distances(start_distance, end_distance)
        if disable_shadow:
            component.set_editor_property("cast_shadow", False)
            shadowless_components += 1
        optimized_components += 1
        optimized_instances += component.get_instance_count()
        category_counts[category] += component.get_instance_count()

if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError("Failed to save optimized Mountain level")

print("LANDSCAPES_NANITE_DISABLED=%d" % landscapes_changed)
print("OPTIMIZED_COMPONENTS=%d" % optimized_components)
print("OPTIMIZED_INSTANCES=%d" % optimized_instances)
print("SHADOWLESS_COMPONENTS=%d" % shadowless_components)
print("OPTIMIZED_BY_CATEGORY=" + repr(dict(category_counts)))
