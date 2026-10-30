import math

import unreal


# Chapters 4-6 stations live in their own always-loaded sublevel of Mountain, next to Chapter1_Village,
# Chapter2_Village and Chapter3_Village. UVillageStorySubsystem finds them by tag:
#   Interact_Craft (ItemID = DT_VillageCrafts row): Dye, Weave, Cut, Sew;
#                  ChDye, ChWeave, ChTailor, ChStage (the Master's Challenge on the festival stage)
#   Interact_Gather (ItemID = item): DYE_MATERIAL_001, DYE_WATER_001
#   Interact_Dialogue (ItemID = dialogue row): the three festival signs (market, crafting, cultural area)
# Stations whose first spot hits a tree canopy or a roof try the next offset. Re-run safely; it clears its previous build.
MAP_PATH = "/Game/Mountain"
SUBLEVEL_PATH = "/Game/SilkVillage/Maps/Chapter4_6_Village"
ROOT_FOLDER = "Chapter4_6"
INTERACTABLE_BP = "/Game/SilkVillage/Blueprints/BP_VillageInteractable"

BEAM = "/Game/MS/3D/Worn_Wooden_Beam_tgnhcaafa/SM_Worn_Wooden_Beam_tgnhcaafa_00_N1"  # 24 x 24 x 240 cm, pivot centered
CYLINDER = "/Game/LevelPrototyping/Meshes/SM_Cylinder"  # 100 x 100 x 100 cm, pivot at base
BARREL = "/Game/MS/3D/Wooden_Barrel_tmgmfeefa/SM_Wooden_Barrel_tmgmfeefa_00_N1"
BOARDS = "/Game/MS/3D/Old_Wood_Boards_ricaS/SM_Old_Wood_Boards_ricaS_00_N1"
ROPE = "/Game/MS/3D/Rope_Coil_vcsoddfs/SM_Rope_Coil_vcsoddfs_00_N1"
FLAG = "/Game/MedievalVillage/Meshes/Mesh_Components/SM_Flag_01"
PLANTS = [
    "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_03_1_RY",
    "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_05_5_RY",
    "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_09_1_RY",
]
FAB_LOOM = "/Game/Fab/Greco-Roman_loom/greco_roman_loom/StaticMeshes/greco_roman_loom"
# Meshy prop models (MeshyModels/, imported under /Game/SilkVillage/Meshes)
DYE_POT = "/Game/SilkVillage/Meshes/Chapter4/PROP_DYE_POT_001/SM_PROP_DYE_POT_001"
LOOM = "/Game/SilkVillage/Meshes/Chapter4/PROP_LOOM_001/SM_PROP_LOOM_001"
DRESS_FORM = "/Game/SilkVillage/Meshes/Chapter5/PROP_DRESS_FORM_001/SM_PROP_DRESS_FORM_001"
BARK = "/Game/SilkVillage/Meshes/Chapter4/ITM_TREE_BARK_001/SM_ITM_TREE_BARK_001"
BERRY = "/Game/SilkVillage/Meshes/Chapter4/ITM_PURPLE_BERRY_001/SM_ITM_PURPLE_BERRY_001"
GOURD = "/Game/SilkVillage/Meshes/Chapter4/TOOL_WATER_GOURD_001/SM_TOOL_WATER_GOURD_001"
PATTERN_FRAGMENT = "/Game/SilkVillage/Meshes/Chapter6/ITM_PATTERN_FRAGMENT_001/SM_ITM_PATTERN_FRAGMENT_001"
FAB_TABLE = "/Game/Fab/Old_Wooden_Table_Low-poly_PBR/old_wooden_table/StaticMeshes/old_wooden_table"
FAB_PONCHO = "/Game/Fab/Poncho/poncho__GLTF_/StaticMeshes/poncho__GLTF_"

# Same key points as build_chapter1_village.py.
SQUARE = (42100.0, -41300.0)
DYE_YARD = (43800.0, -37800.0)  # cô Mai: stream bank beside her house
WEAVE_YARD = (42700.0, -37900.0)  # cô Lụa: in front of her workshop
OLD_HOUSE = (42350.0, -38300.0)  # the weaver's workshop: the old house at the forest edge is behind it

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


def is_open_ground(x, y):
    """Terrain, not a tree canopy or a roof: landscape or a terrain mesh below 250 cm."""
    z, actor = ground_hit(x, y)
    name = actor.get_actor_label() if actor else ""
    return z < 250.0 and (isinstance(actor, unreal.Landscape) or name.startswith(("taDvf", "SM_Nordic_Beach_Rocky")))


def find_spot(anchor, offsets, label):
    for dx, dy in offsets:
        x, y = anchor[0] + dx, anchor[1] + dy
        if is_open_ground(x, y):
            return x, y
    skipped.append("{}: no open ground near ({:.0f}, {:.0f}) for {} offsets".format(label, anchor[0], anchor[1], len(offsets)))
    return None


def yaw_to(src, dst):
    return math.degrees(math.atan2(dst[1] - src[1], dst[0] - src[0]))


def place(actor, label, folder, tags=()):
    actor.set_actor_label(label)
    actor.set_folder_path("{}/{}".format(ROOT_FOLDER, folder))
    actor.tags = [unreal.Name(t) for t in ("Chapter4_6",) + tuple(tags)]
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


def spawn_interactable(mesh, location, yaw, mesh_scale, mesh_z, label, folder, prompt, item_id, tags, radius=190.0):
    """BP_VillageInteractable: its inherited SM_Pickup shows the mesh, E goes to the story subsystem."""
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(INTERACTABLE_BP).generated_class(), location,
                                                   unreal.Rotator(0, 0, yaw))
    actor.set_editor_property("ItemID", unreal.Name(item_id))
    actor.set_editor_property("FallbackPrompt", unreal.Text(prompt))
    pickup = actor.get_component_by_class(unreal.StaticMeshComponent)
    loaded = unreal.load_asset(mesh)
    pickup.set_static_mesh(loaded)
    if isinstance(mesh_scale, float):
        # A number means "fit to this height (cm)": Fab models have arbitrary pivots and sizes, so centre the model's
        # base on the actor and scale it to the target height.
        bounds = loaded.get_bounds()
        s = mesh_scale / (bounds.box_extent.z * 2.0)
        mesh_scale = unreal.Vector(s, s, s)
        pickup.set_relative_location(unreal.Vector(-bounds.origin.x * s, -bounds.origin.y * s, -(bounds.origin.z - bounds.box_extent.z) * s), False, False)
    else:
        pickup.set_relative_location(unreal.Vector(0, 0, mesh_z), False, False)
    pickup.set_relative_scale3d(mesh_scale)
    walk_through(actor)
    for sphere in actor.get_components_by_class(unreal.SphereComponent):
        sphere.set_sphere_radius(radius)
    return place(actor, label, folder, tags)


def craft_station(key, title, mesh, scale, anchor, offsets, face_to, folder, props=None, radius=190.0):
    """A crafting station: ItemID = the DT_VillageCrafts row; its title comes from the row."""
    spot = find_spot(anchor, offsets, key)
    if not spot:
        return None
    x, y = spot
    z = ground_z(x, y)
    yaw = yaw_to(spot, face_to)
    spawn_interactable(mesh, unreal.Vector(x, y, z), yaw, scale if isinstance(scale, float) else unreal.Vector(*scale), 0.0,
                       "Station_" + key, folder, title, key, ("Interact_Craft",), radius)
    if props:
        props(x, y, z, yaw)
    return spot


def frame_props(x, y, z, yaw):
    """Two posts and a rail behind a station: a reel / loom frame look."""
    fwd = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
    side = (-fwd[1], fwd[0])
    for i, s in enumerate((-70.0, 70.0)):
        spawn_post(x + side[0] * s + fwd[0] * 110.0, y + side[1] * s + fwd[1] * 110.0, 150.0, "Frame_Post{}".format(i), "Props")


# ---------------------------------------------------------------- Chapter 4: dye house and loom
def build_chapter4():
    craft_station("Dye", "Nồi nhuộm", DYE_POT, 120.0, DYE_YARD, [(250.0, 420.0), (-250.0, -450.0), (480.0, 0.0), (-500.0, 300.0)],
                  DYE_YARD, "Dyeing")
    craft_station("Weave", "Khung dệt", LOOM, 170.0, WEAVE_YARD, [(0.0, -420.0), (-300.0, -380.0), (350.0, -380.0)],
                  WEAVE_YARD, "Weaving")


def build_gather_sources():
    ring = [(700.0, 0.0), (550.0, 450.0), (0.0, 700.0), (-550.0, 450.0), (-700.0, 0.0), (-550.0, -450.0), (0.0, -700.0), (550.0, -450.0),
            (900.0, 300.0), (-900.0, 300.0), (300.0, 900.0), (-300.0, -900.0)]
    placed = 0
    for i in range(6):
        spot = find_spot(DYE_YARD, ring[i * 2 % len(ring):] + ring[:i * 2 % len(ring)], "DyeMaterial{}".format(i))
        if not spot:
            continue
        x, y = spot
        spawn_interactable((BARK, BERRY)[i % 2], unreal.Vector(x, y, ground_z(x, y)), (i * 67) % 360, 45.0,
                           0.0, "Gather_DyeMaterial_{}".format(i), "Gather", "Hái nguyên liệu nhuộm (lá, vỏ cây, đất màu)",
                           "DYE_MATERIAL_001", ("Interact_Gather",), 150.0)
        placed += 1
    # Water at the stream bank beside Mai's house.
    for i, offsets in enumerate(([(600.0, -600.0), (700.0, -300.0), (500.0, -800.0)], [(-100.0, -700.0), (-300.0, -650.0), (100.0, -900.0)])):
        spot = find_spot(DYE_YARD, offsets, "DyeWater{}".format(i))
        if not spot:
            continue
        x, y = spot
        spawn_interactable(GOURD, unreal.Vector(x, y, ground_z(x, y)), 0.0, 60.0, 0.0,
                           "Gather_DyeWater_{}".format(i), "Gather", "Lấy nước suối", "DYE_WATER_001", ("Interact_Gather",), 150.0)
        placed += 1
    return placed


# ---------------------------------------------------------------- Chapter 5: tailoring house
def build_chapter5():
    craft_station("Cut", "Bàn cắt vải", FAB_TABLE, 80.0, WEAVE_YARD, [(450.0, 320.0), (500.0, 100.0), (350.0, 500.0)],
                  WEAVE_YARD, "Tailoring")
    craft_station("Sew", "Khung may", DRESS_FORM, 165.0, WEAVE_YARD, [(-420.0, 330.0), (-480.0, 100.0), (-350.0, 520.0)],
                  WEAVE_YARD, "Tailoring")


# ---------------------------------------------------------------- Chapter 6: signs and festival stage (the practice hall went with the preparation days)
def build_chapter6():
    placed = 0
    stage = [("ChDye", "Bàn nhuộm sân khấu", DYE_POT, 120.0), ("ChWeave", "Khung dệt sân khấu", LOOM, 170.0),
             ("ChTailor", "Bàn may sân khấu", DRESS_FORM, 165.0), ("ChStage", "Sàn diễn", CYLINDER, (1.6, 1.6, 0.18))]
    for i, (key, title, mesh, scale) in enumerate(stage):
        # The festival stage row sits on the level grass just south of the square (ground falls away east of x=42700).
        dx = -920.0 + 460.0 * i
        offsets = [(dx, -700.0), (dx, -900.0), (dx, -500.0)]
        placed += 1 if craft_station(key, title, mesh, scale, SQUARE, offsets, SQUARE, "FestivalStage", None, 170.0) else 0
    signs = [("SIGN_MARKET", "Chợ lụa (Silk Market)", (1050.0, -300.0)), ("SIGN_CRAFT", "Khu nghệ nhân (Crafting Area)", (-1050.0, -300.0)),
             ("SIGN_CULTURE", "Khu văn hóa (Cultural Area)", (0.0, 1000.0))]
    for key, title, base in signs:
        offsets = [base, (base[0] + 200.0, base[1]), (base[0] - 200.0, base[1]), (base[0], base[1] + 250.0)]
        spot = find_spot(SQUARE, offsets, key)
        if not spot:
            continue
        x, y = spot
        z = ground_z(x, y)
        spawn_interactable(FLAG, unreal.Vector(x, y, z), yaw_to(spot, SQUARE), unreal.Vector(0.55, 0.55, 0.55), 0.0, "Sign_" + key,
                           "FestivalSigns", title, key, ("Interact_Dialogue",), 170.0)
        placed += 1
    return placed


def main():
    ensure_sublevel()
    level_subsystem.set_current_level_by_name(SUBLEVEL_PATH.split("/")[-1])
    removed = clear_previous_build()
    build_chapter4()
    gathered = build_gather_sources()
    build_chapter5()
    others = build_chapter6()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    level_subsystem.set_current_level_by_name("PersistentLevel")
    count = sum(1 for a in actor_subsystem.get_all_level_actors() if "Chapter4_6" in [str(t) for t in a.tags])
    print("CHAPTER4_6_BUILD removed={} placed={} gather={} ch6_stations={}".format(removed, count, gathered, others))
    for line in skipped:
        print("SKIPPED", line)


main()

