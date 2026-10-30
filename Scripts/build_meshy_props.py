import math

import unreal


# Shows the Meshy item models that would otherwise only be icons in the bag - but only where they look arranged: tools on
# the cutting table, accessories on their own table beside the dress form, and a row of display pedestals (golden cocoon, master's scissors, village
# seal, ancient pattern) behind the festival stage. Everything goes in the always-loaded sublevel Meshy_Props and is
# found again by the tag MeshyProp, so re-running replaces the previous build. Run it on /Game/Mountain after
# build_chapter4_6_village.py (it anchors on that script's stations by actor label).
MAP_PATH = "/Game/Mountain"
SUBLEVEL_PATH = "/Game/SilkVillage/Maps/Meshy_Props"
ROOT_FOLDER = "MeshyProps"
MESH_ROOT = "/Game/SilkVillage/Meshes"

CYLINDER = "/Game/LevelPrototyping/Meshes/SM_Cylinder"  # 100 x 100 x 100 cm, pivot at base
FAB_TABLE = "/Game/Fab/Old_Wooden_Table_Low-poly_PBR/old_wooden_table/StaticMeshes/old_wooden_table"
GATE = (41700.0, -43500.0)
SQUARE = (42100.0, -41300.0)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
spawned = []
skipped = []
used = set()


def mesh_path(item_id):
    for path in eal.list_assets(MESH_ROOT, recursive=True, include_folder=False):
        if path.split("/")[-1].split(".")[0] == "SM_" + item_id:
            return path.split(".")[0]
    raise RuntimeError("No imported mesh for " + item_id)


def ensure_sublevel():
    if not world.get_path_name().startswith(MAP_PATH):
        raise RuntimeError("Open {} before running this script".format(MAP_PATH))
    for streaming in unreal.EditorLevelUtils.get_levels(world):
        if streaming.get_outer().get_path_name().startswith(SUBLEVEL_PATH):
            return
    if eal.does_asset_exist(SUBLEVEL_PATH):
        unreal.EditorLevelUtils.add_level_to_world(world, SUBLEVEL_PATH, unreal.LevelStreamingAlwaysLoaded)
    else:
        unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded, SUBLEVEL_PATH, False)


def clear_previous_build():
    removed = 0
    for actor in actor_subsystem.get_all_level_actors():
        if actor.actor_has_tag("MeshyProp"):
            actor_subsystem.destroy_actor(actor)
            removed += 1
    return removed


def ground_z(x, y, top=3000.0):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, top), unreal.Vector(x, y, -3000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, spawned, unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def spawn_fitted(path, x, y, z, yaw, height, label, folder="Props", tags=(), fit_largest=False):
    """Fits a model to `height` cm (or, with fit_largest, so its LARGEST dimension is `height` cm: flat things like scissors
    have almost no height) and stands its base-centre at (x, y, z). Models have arbitrary pivots and sizes."""
    mesh = unreal.load_asset(path)
    bounds = mesh.get_bounds()
    e = bounds.box_extent
    s = height / ((max(e.x, e.y, e.z) if fit_largest else e.z) * 2.0)
    c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    ox, oy, oz = bounds.origin.x, bounds.origin.y, bounds.origin.z - bounds.box_extent.z
    loc = unreal.Vector(x - s * (ox * c - oy * sn), y - s * (ox * sn + oy * c), z - s * oz)
    actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, loc, unreal.Rotator(0, 0, yaw))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(s, s, s))
    actor.static_mesh_component.set_collision_profile_name("NoCollision")  # decoration: never blocks the player
    actor.set_actor_label(label)
    actor.set_folder_path("{}/{}".format(ROOT_FOLDER, folder))
    actor.tags = [unreal.Name(t) for t in ("MeshyProp",) + tuple(tags)]
    spawned.append(actor)
    return actor


def prop(item_id, x, y, height, yaw=0.0, base_z=None, folder="Props"):
    """An item model on the ground (or on top of something at base_z); `height` is its largest dimension in cm."""
    z = ground_z(x, y) if base_z is None else base_z
    if z is None:
        skipped.append("{} at ({:.0f}, {:.0f}): no ground".format(item_id, x, y))
        return None
    used.add(item_id)
    return spawn_fitted(mesh_path(item_id), x, y, z, yaw, height, "{}_{}".format(item_id, len(spawned)), folder, fit_largest=True)


def station(label):
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor.get_actor_location()
    skipped.append("anchor {} not found (run build_chapter4_6_village.py first)".format(label))
    return None


def table_at(x, y, yaw, label, folder):
    z = ground_z(x, y)
    spawn_fitted(FAB_TABLE, x, y, z, yaw, 80.0, label, folder)
    return z + 80.0  # the table top


def build_trays():
    for actor in actor_subsystem.get_all_level_actors():
        if actor.actor_has_tag("Interact_SilkwormTray"):
            p = actor.get_actor_location()
            for i, (dx, dy) in enumerate(((140.0, 40.0), (170.0, -30.0), (130.0, -80.0))):
                prop("ITM_COCOON_001", p.x + dx, p.y + dy, 14.0, i * 60.0, folder="SilkwormTrays")


def build_silk_stations():
    p = station("Station_ReelingPot")
    if p:
        for i, (dx, dy) in enumerate(((190.0, 60.0), (210.0, 0.0), (190.0, -60.0))):
            prop("ITM_RAW_SILK_001", p.x + dx, p.y + dy, 22.0, i * 50.0, folder="Reeling")
    p = station("Station_SpinningWheel")
    if p:
        for i, (dx, dy) in enumerate(((-190.0, 70.0), (-210.0, 10.0))):
            prop("ITM_RAW_SILK_001", p.x + dx, p.y + dy, 22.0, i * 40.0, folder="Spinning")


def build_dye_house():
    p = station("Station_Dye")
    if not p:
        return
    for i, (dx, dy) in enumerate(((170.0, 110.0), (200.0, 40.0), (170.0, -30.0))):
        prop("ITM_RED_THREAD_001", p.x + dx, p.y + dy, 30.0, i * 70.0, folder="DyeHouse")
    prop("ITM_TREE_BARK_001", p.x - 170.0, p.y + 90.0, 28.0, 20.0, folder="DyeHouse")
    prop("ITM_TREE_BARK_001", p.x - 200.0, p.y + 30.0, 24.0, 90.0, folder="DyeHouse")
    prop("ITM_PURPLE_BERRY_001", p.x - 170.0, p.y - 50.0, 22.0, 0.0, folder="DyeHouse")
    prop("ITM_PURPLE_BERRY_001", p.x - 140.0, p.y - 100.0, 20.0, 140.0, folder="DyeHouse")
    prop("TOOL_WATER_GOURD_001", p.x + 20.0, p.y + 190.0, 34.0, 0.0, folder="DyeHouse")


def build_weaving_and_tailoring():
    p = station("Station_Weave")
    if p:
        pass  # nothing on the ground by the loom: loose items looked like litter
    p = station("Station_Cut")
    if p:
        # The cutting table itself is the station; its tools lie on top.
        top = p.z + 80.0
        prop("TOOL_SCISSORS_001", p.x - 25.0, p.y + 10.0, 24.0, 30.0, base_z=top, folder="Tailoring")
        prop("TOOL_NEEDLE_THREAD_001", p.x + 25.0, p.y - 10.0, 16.0, 80.0, base_z=top, folder="Tailoring")
        prop("TOOL_WEAVING_SHUTTLE_001", p.x + 5.0, p.y + 35.0, 22.0, 100.0, base_z=top, folder="Tailoring")
    p = station("Station_Sew")
    if p:
        # An accessories table beside the dress form.
        tx, ty = p.x + 220.0, p.y + 30.0
        top = table_at(tx, ty, 0.0, "AccessoriesTable", "Tailoring")
        prop("ACC_SILK_SCARF_001", tx - 30.0, ty, 24.0, 0.0, base_z=top, folder="Tailoring")
        prop("ACC_BELT_001", tx + 5.0, ty + 15.0, 24.0, 40.0, base_z=top, folder="Tailoring")
        prop("ACC_BRACELET_001", tx + 35.0, ty - 12.0, 12.0, 0.0, base_z=top, folder="Tailoring")


def build_gate_and_festival():
    # Display pedestals behind the stage: the rewards of the Master's Challenge.
    # The performance disc (Station_ChStage) is empty by itself: a dress form stands on it as the model for the show.
    p = station("Station_ChStage")
    if p:
        yaw = math.degrees(math.atan2(SQUARE[1] - p.y, SQUARE[0] - p.x))
        spawn_fitted("/Game/SilkVillage/Meshes/Chapter5/PROP_DRESS_FORM_001/SM_PROP_DRESS_FORM_001", p.x, p.y, p.z + 4.0, yaw, 165.0,
                     "StageModel_DressForm", "Stage")
        used.add("PROP_DRESS_FORM_001")
    # A row on level grass south of the square (the old spot beside the river bank sloped 150 cm over four pedestals).
    items = (("ITM_GOLDEN_COCOON_001", 28.0), ("TOOL_MASTER_SCISSORS_001", 36.0), ("ITM_VILLAGE_SEAL_001", 22.0), ("ITM_PATTERN_FRAGMENT_001", 30.0))
    heights = [ground_z(SQUARE[0] - 240.0 + 160.0 * i, SQUARE[1] - 1200.0) for i in range(len(items))]
    pedestal_z = min(h for h in heights if h is not None) if any(h is not None for h in heights) else None
    for i, (item, h) in enumerate(items):
        x, y = SQUARE[0] - 240.0 + 160.0 * i, SQUARE[1] - 1200.0
        z = pedestal_z
        if z is None:
            skipped.append("pedestal {}: no ground".format(item))
            continue
        pedestal = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
        pedestal.static_mesh_component.set_static_mesh(unreal.load_asset(CYLINDER))
        pedestal.set_actor_scale3d(unreal.Vector(0.55, 0.55, 0.9))
        pedestal.set_actor_label("Pedestal_{}".format(item))
        pedestal.set_folder_path(ROOT_FOLDER + "/Stage")
        pedestal.tags = [unreal.Name("MeshyProp")]
        spawned.append(pedestal)
        prop(item, x, y, h, 25.0 * i, base_z=z + 90.0, folder="Stage")


def main():
    ensure_sublevel()
    level_subsystem.set_current_level_by_name(SUBLEVEL_PATH.split("/")[-1])
    removed = clear_previous_build()
    build_weaving_and_tailoring()
    build_gate_and_festival()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.EditorLevelLibrary.save_all_dirty_levels()
    level_subsystem.set_current_level_by_name("PersistentLevel")
    print("MESHY_PROPS removed={} placed={} items_used={}".format(removed, len(spawned), len(used)))
    for line in skipped:
        print("SKIPPED", line)


main()
