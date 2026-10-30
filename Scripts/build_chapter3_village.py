import math

import unreal


# Chapter 3 station lives in its own always-loaded sublevel of Mountain, next to Chapter1_Village and
# Chapter2_Village: the spinning wheel (guồng se tơ) beside cô Mai's house. UVillageStorySubsystem finds it by tag:
#   Interact_SpinningWheel
# The reeling pot of Chapter 2 (Interact_ReelingPot) is reused for the Day 3 batch. Re-run safely; it clears
# its previous build.
MAP_PATH = "/Game/Mountain"
SUBLEVEL_PATH = "/Game/SilkVillage/Maps/Chapter3_Village"
ROOT_FOLDER = "Chapter3"
INTERACTABLE_BP = "/Game/SilkVillage/Blueprints/BP_VillageInteractable"

BEAM = "/Game/MS/3D/Worn_Wooden_Beam_tgnhcaafa/SM_Worn_Wooden_Beam_tgnhcaafa_00_N1"  # 24 x 24 x 240 cm, pivot centered
CYLINDER = "/Game/LevelPrototyping/Meshes/SM_Cylinder"  # 100 x 100 x 100 cm, pivot at base
BARREL = "/Game/MS/3D/Wooden_Barrel_tmgmfeefa/SM_Wooden_Barrel_tmgmfeefa_00_N1"
ROPE = "/Game/MS/3D/Rope_Coil_vcsoddfs/SM_Rope_Coil_vcsoddfs_00_N1"
WHEEL_MESH = "/Game/SilkVillage/Meshes/Chapter3/PROP_SPINNING_WHEEL_001/SM_PROP_SPINNING_WHEEL_001"
THREAD_MESH ="/Game/SilkVillage/Meshes/Chapter3/ITM_SILK_THREAD_001/SM_ITM_SILK_THREAD_001"

# Same key points as build_chapter1_village.py.
SQUARE = (42100.0, -41300.0)
DYE_YARD = (43800.0, -37800.0)  # cô Mai: stream bank beside her house
# Candidate spots for the wheel, nearest first: beside Mai, facing the square. The first open one wins.
WHEEL_OFFSETS = [(-420.0, 160.0), (-420.0, -200.0), (-300.0, 380.0), (200.0, 420.0), (-600.0, 0.0)]

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
spawned = []
skipped = []


def ensure_sublevel():
    if not world.get_path_name().startswith(MAP_PATH):
        raise RuntimeError("Open {} before running this script".format(MAP_PATH))
    for streaming in unreal.EditorLevelUtils.get_levels(world):
        if streaming.get_outer().get_path_name().startswith(SUBLEVEL_PATH):
            return
    if unreal.EditorAssetLibrary.does_asset_exist(SUBLEVEL_PATH):
        unreal.EditorLevelUtils.add_level_to_world(world, SUBLEVEL_PATH, unreal.LevelStreamingAlwaysLoaded)
    else:
        unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded, SUBLEVEL_PATH, False)


def clear_previous_build():
    removed = 0
    for actor in actor_subsystem.get_all_level_actors():
        if str(actor.get_folder_path()).startswith(ROOT_FOLDER) and SUBLEVEL_PATH.split("/")[-1] in actor.get_path_name():
            actor_subsystem.destroy_actor(actor)
            removed += 1
    return removed


def ground_hit(x, y, top=3000.0):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, top), unreal.Vector(x, y, -3000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, spawned, unreal.DrawDebugTrace.NONE, True)
    if not hit:
        raise RuntimeError("No ground at {}, {}".format(x, y))
    t = hit.to_tuple()
    return t[4].z, t[9]


def ground_z(x, y, top=3000.0):
    return ground_hit(x, y, top)[0]


def is_open_ground(x, y, label):
    """Terrain, not a tree canopy or a roof: landscape or a terrain mesh below 250 cm."""
    z, actor = ground_hit(x, y)
    name = actor.get_actor_label() if actor else ""
    ok = z < 250.0 and (isinstance(actor, unreal.Landscape) or name.startswith(("taDvf", "SM_Nordic_Beach_Rocky")))
    if not ok:
        skipped.append("{} ({:.0f}, {:.0f}) hit {} at z={:.0f}".format(label, x, y, name, z))
    return ok


def yaw_to(src, dst):
    return math.degrees(math.atan2(dst[1] - src[1], dst[0] - src[0]))


def place(actor, label, folder, tags=()):
    actor.set_actor_label(label)
    actor.set_folder_path("{}/{}".format(ROOT_FOLDER, folder))
    actor.tags = [unreal.Name(t) for t in ("Chapter3",) + tuple(tags)]
    spawned.append(actor)
    return actor


def spawn_mesh(path, location, rotation, scale, label, folder, tags=()):
    actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    actor.set_actor_scale3d(scale)
    return place(actor, label, folder, tags)


def spawn_post(x, y, height, label, folder):
    z = ground_z(x, y)
    return spawn_mesh(BEAM, unreal.Vector(x, y, z + height * 0.5 - 20.0), unreal.Rotator(0, 0, 0),
                      unreal.Vector(1.0, 1.0, height / 240.0), label, folder)


def walk_through(actor):
    """Models use the Trigger preset: the player walks through; InteractionRange drives the E prompt."""
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        comp.set_collision_profile_name("Trigger")
        comp.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
    return actor


def spawn_interactable(mesh, location, yaw, mesh_scale, mesh_z, label, folder, prompt, item_id, tags, radius=200.0):
    """BP_VillageInteractable: its inherited SM_Pickup shows the mesh, E goes to the story subsystem."""
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(INTERACTABLE_BP).generated_class(), location,
                                                   unreal.Rotator(0, 0, yaw))
    actor.set_editor_property("ItemID", unreal.Name(item_id))  # no DT_ItemData row -> FallbackPrompt
    actor.set_editor_property("FallbackPrompt", unreal.Text(prompt))
    pickup = actor.get_component_by_class(unreal.StaticMeshComponent)
    loaded = unreal.load_asset(mesh)
    pickup.set_static_mesh(loaded)
    if isinstance(mesh_scale, float):
        # A number means "fit to this height (cm)": centre the model's base on the actor and scale it to that height.
        bounds = loaded.get_bounds()
        s = mesh_scale / (bounds.box_extent.z * 2.0)
        mesh_scale = unreal.Vector(s, s, s)
        pickup.set_relative_location(unreal.Vector(-bounds.origin.x * s, -bounds.origin.y * s,
                                                   -(bounds.origin.z - bounds.box_extent.z) * s), False, False)
    else:
        pickup.set_relative_location(unreal.Vector(0, 0, mesh_z), False, False)
    pickup.set_relative_scale3d(mesh_scale)
    walk_through(actor)
    for sphere in actor.get_components_by_class(unreal.SphereComponent):
        sphere.set_sphere_radius(radius)
    return place(actor, label, folder, tags)


def build_spinning_wheel():
    spot = None
    for dx, dy in WHEEL_OFFSETS:
        x, y = DYE_YARD[0] + dx, DYE_YARD[1] + dy
        if is_open_ground(x, y, "Station_SpinningWheel"):
            spot = (x, y)
            break
    if not spot:
        raise RuntimeError("No open ground near cô Mai's house; add an offset to WHEEL_OFFSETS")
    x, y = spot
    z = ground_z(x, y)
    face = yaw_to(spot, DYE_YARD)
    fwd = (math.cos(math.radians(face)), math.sin(math.radians(face)))
    side = (-fwd[1], fwd[0])

    # The station itself: the Meshy spinning wheel model (E opens the spinning mini-game), fitted to 140 cm.
    spawn_interactable(WHEEL_MESH, unreal.Vector(x, y, z), face, 140.0, 0.0, "Station_SpinningWheel",
                       "Spinning", "Se tơ (guồng quay)", "SpinningWheel", ("Interact_SpinningWheel",))
    # Finished-thread spools beside it, for the look of the yard.
    if unreal.EditorAssetLibrary.does_asset_exist(THREAD_MESH):
        for i, s in enumerate((-160.0, -110.0, 160.0)):
            sx, sy = x + side[0] * s, y + side[1] * s
            spawn_mesh(THREAD_MESH, unreal.Vector(sx, sy, ground_z(sx, sy)), unreal.Rotator(0, 0, i * 70.0),
                       unreal.Vector(1.2, 1.2, 1.2), "Spin_Spool{}".format(i), "Spinning")
    return spot


def main():
    ensure_sublevel()
    level_subsystem.set_current_level_by_name(SUBLEVEL_PATH.split("/")[-1])
    removed = clear_previous_build()
    spot = build_spinning_wheel()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    level_subsystem.set_current_level_by_name("PersistentLevel")
    count = sum(1 for a in actor_subsystem.get_all_level_actors() if "Chapter3" in [str(t) for t in a.tags])
    print("CHAPTER3_BUILD removed={} placed={} wheel=({:.0f}, {:.0f})".format(removed, count, spot[0], spot[1]))
    for line in skipped:
        print("SKIPPED", line)


main()
