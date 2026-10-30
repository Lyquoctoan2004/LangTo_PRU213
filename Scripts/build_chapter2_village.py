import math

import unreal


# Chapter 2 stations live in their own always-loaded sublevel of Mountain, next to Chapter1_Village:
# festival material sources, the gate and hanging stations, the arranging spots with their (hidden)
# props, and the silk reeling pot. UVillageStorySubsystem finds them by tag:
#   Interact_DecoMaterial (ItemID = Q11 objective: Bamboo/Cloth/Flower/Rope/WoodDeco)
#   Interact_FestivalGate, Interact_FestivalHang, Interact_ReelingPot
#   Interact_FestivalSpot (ItemID = key) + props tagged FestivalProp, FestivalProp_<key>
#   FestivalDecor_Gate / FestivalDecor_Hang: shown once that step is done
# Cocoon harvest reuses Chapter 1's silkworm trays. Re-run safely; it clears its previous build.
MAP_PATH = "/Game/Mountain"
SUBLEVEL_PATH = "/Game/SilkVillage/Maps/Chapter2_Village"
ROOT_FOLDER = "Chapter2"
INTERACTABLE_BP = "/Game/SilkVillage/Blueprints/BP_VillageInteractable"

BEAM = "/Game/MS/3D/Worn_Wooden_Beam_tgnhcaafa/SM_Worn_Wooden_Beam_tgnhcaafa_00_N1"  # 24 x 24 x 240 cm, pivot centered
CYLINDER = "/Game/LevelPrototyping/Meshes/SM_Cylinder"  # 100 x 100 x 100 cm, pivot at base
BARREL = "/Game/MS/3D/Wooden_Barrel_tmgmfeefa/SM_Wooden_Barrel_tmgmfeefa_00_N1"
BOARDS = "/Game/MS/3D/Old_Wood_Boards_ricaS/SM_Old_Wood_Boards_ricaS_00_N1"
ROPE = "/Game/MS/3D/Rope_Coil_vcsoddfs/SM_Rope_Coil_vcsoddfs_00_N1"
BAMBOO = "/Game/Bam_boo/FBX_AS01_BambusaVulgaris"
FLAG = "/Game/MedievalVillage/Meshes/Mesh_Components/SM_Flag_01"
PENNANT = "/Game/MedievalVillage/Meshes/Mesh_Components/SM_Flag_Small01"
FLOWERS = [
    "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_03_1_RY",
    "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_05_5_RY",
    "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_09_1_RY",
]
CLOTH_MATERIALS = [
    "/Game/MedievalVillage/Materials/Meshspecific/Flags/MI_Flag_01",
    "/Game/MedievalVillage/Materials/Meshspecific/Flags/MI_Flag_02",
    "/Game/MedievalVillage/Materials/Meshspecific/Flags/MI_RealFlags",
]
# Display props for the arranging spots.
FAB_TABLE = "/Game/Fab/Old_Wooden_Table_Low-poly_PBR/old_wooden_table/StaticMeshes/old_wooden_table"
FAB_STOOL = "/Game/Fab/Medieval_Market__Leather_Hanger___Stool/leather_hanger_stool_Sfab"
FAB_POTS = "/Game/Fab/Pots_and_vase_with_flowers/pots_and_vase_with_flowers/StaticMeshes/pots_and_vase_with_flowers"
FAB_LOOM = "/Game/Fab/Greco-Roman_loom/greco_roman_loom/StaticMeshes/greco_roman_loom"
FAB_PONCHO = "/Game/Fab/Poncho/poncho__GLTF_/StaticMeshes/poncho__GLTF_"
# The display shelf is the project's own wooden rack (Meshy import), since no shelf has been added from Fab.
FAB_SHELF = "/Game/SilkVillage/Meshes/Chapter4/PROP_DRYING_RACK_001/SM_PROP_DRYING_RACK_001"
DRESS_FORM = "/Game/SilkVillage/Meshes/Chapter5/PROP_DRESS_FORM_001/SM_PROP_DRESS_FORM_001"  # Meshy dress form
REEL_POT = "/Game/SilkVillage/Meshes/Chapter3/PROP_REELING_POT_001/SM_PROP_REELING_POT_001"  # Meshy model of the reeling pot
# Festival materials use their own Meshy item models (rope keeps the project's rope coil): key -> (mesh, height cm).
MESHY_DECO = {
    "Bamboo": ("/Game/SilkVillage/Meshes/Chapter2/DEC_BAMBOO_001/SM_DEC_BAMBOO_001", 130.0),
    "Cloth": ("/Game/SilkVillage/Meshes/Chapter2/DEC_COLOR_CLOTH_001/SM_DEC_COLOR_CLOTH_001", 85.0),
    "Flower": ("/Game/SilkVillage/Meshes/Chapter2/DEC_FLOWER_001/SM_DEC_FLOWER_001", 60.0),
    "WoodDeco": ("/Game/SilkVillage/Meshes/Chapter2/DEC_WOOD_ORNAMENT_001/SM_DEC_WOOD_ORNAMENT_001", 65.0),
}
# Extra yaw per model, in case its front is not +X. Adjust after looking at it in the viewport.
FAB_YAW_FIX = {FAB_TABLE: 0.0, FAB_STOOL: 0.0, FAB_POTS: 0.0, FAB_LOOM: 0.0, FAB_PONCHO: 0.0}
OUTFIT_MESH ="/Game/MeshyImports/Import_20261022_153843/The_Crimson_Sash_Import0004"
OUTFIT_IDLE = "/Game/SilkVillage/NPC/Anim/AS_NPC_Idle_Crimson"

# Same key points as build_chapter1_village.py.
GATE = (41700.0, -43500.0)
SQUARE = (42100.0, -41300.0)
REELING_WORKSHOP = (43500.0, -40500.0)  # Workshop_Reeling copy from Chapter 1

# (objective key, mesh, scale, prompt, positions)
MATERIAL_SOURCES = [
    ("Bamboo", BAMBOO, 0.6, "Chặt tre", [(40650.0, -41900.0), (40750.0, -40700.0)]),
    ("Cloth", FLAG, 0.6, "Lấy vải màu", [(42900.0, -42300.0), (41300.0, -42500.0)]),
    ("Flower", None, 1.0, "Hái hoa", [(40900.0, -42900.0), (42700.0, -43000.0), (43000.0, -41900.0), (41000.0, -40300.0)]),
    ("Rope", ROPE, 1.4, "Lấy dây thừng", [(41450.0, -40150.0), (42950.0, -40800.0)]),
    ("WoodDeco", BOARDS, 0.8, "Lấy đồ gỗ trang trí", [(42500.0, -40100.0)]),
]

# Arranging spots around the festival square: (key, prompt, offset from SQUARE)
SPOTS = [
    ("Table", "Bàn trưng bày lụa", (-450.0, 350.0)),
    ("Chairs", "Ghế cho khách", (-450.0, -250.0)),
    ("FlowerBasket", "Giỏ hoa", (450.0, -450.0)),
    ("Loom", "Khung dệt", (450.0, 350.0)),
    ("SilkCloth", "Vải lụa treo", (0.0, 550.0)),
    ("Shelf", "Kệ trưng bày", (-150.0, -550.0)),
    ("Outfit", "Trang phục mẫu", (200.0, -550.0)),
]

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
    actor.tags = [unreal.Name(t) for t in ("Chapter2",) + tuple(tags)]
    spawned.append(actor)
    return actor


def spawn_mesh(path, location, rotation, scale, label, folder, tags=(), material=None):
    actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    actor.set_actor_scale3d(scale)
    if material:
        actor.static_mesh_component.set_material(0, unreal.load_asset(material))
    return place(actor, label, folder, tags)


def spawn_fitted(path, x, y, z, yaw, label, folder, tags, height=None, scale=None, yaw_fix=0.0):
    """Fab models have arbitrary pivots and sizes: scale to a target height (or a fixed scale) and put the
    centre of the model's base at (x, y, z)."""
    mesh = unreal.load_asset(path)
    bounds = mesh.get_bounds()
    s = scale if scale else height / (bounds.box_extent.z * 2.0)
    yaw += yaw_fix + FAB_YAW_FIX.get(path, 0.0)
    ox, oy, oz = bounds.origin.x, bounds.origin.y, bounds.origin.z - bounds.box_extent.z
    c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    loc = unreal.Vector(x - s * (ox * c - oy * sn), y - s * (ox * sn + oy * c), z - s * oz)
    return spawn_mesh(path, loc, unreal.Rotator(0, 0, yaw), unreal.Vector(s, s, s), label, folder, tags)


def spawn_post(x, y, height, label, folder, tags=()):
    z = ground_z(x, y)
    return spawn_mesh(BEAM, unreal.Vector(x, y, z + height * 0.5 - 20.0), unreal.Rotator(0, 0, 0),
                      unreal.Vector(1.0, 1.0, height / 240.0), label, folder, tags)


def pennant_line(a, b, height, label, folder, tags=(), sag_cm=60.0, spacing=45.0):
    za, zb = ground_z(*a) + height, ground_z(*b) + height
    count = max(2, int(math.dist(a, b) / spacing))
    yaw = yaw_to(a, b)
    for i in range(1, count):
        t = i / count
        sag = sag_cm * 4.0 * t * (1.0 - t)
        loc = unreal.Vector(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, za + (zb - za) * t - sag)
        spawn_mesh(PENNANT, loc, unreal.Rotator(0, 0, yaw), unreal.Vector(1.4, 1.4, 1.4),
                   "{}_{:02d}".format(label, i), folder, tags, CLOTH_MATERIALS[i % len(CLOTH_MATERIALS)])


def spawn_interactable(mesh, location, yaw, mesh_scale, mesh_z, label, folder, prompt, item_id, tags, radius=180.0):
    """BP_VillageInteractable: its inherited SM_Pickup shows the mesh, E goes to the story subsystem."""
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(INTERACTABLE_BP).generated_class(), location,
                                                   unreal.Rotator(0, 0, yaw))
    actor.set_editor_property("ItemID", unreal.Name(item_id))  # no DT_ItemData row -> FallbackPrompt
    actor.set_editor_property("FallbackPrompt", unreal.Text(prompt))
    pickup = actor.get_component_by_class(unreal.StaticMeshComponent)
    loaded = unreal.load_asset(mesh)
    pickup.set_static_mesh(loaded)
    if isinstance(mesh_scale, float):
        # A number means "fit to this height (cm)": imported models have arbitrary pivots and sizes, so centre the
        # model's base on the actor and scale it to the target height.
        bounds = loaded.get_bounds()
        s = mesh_scale / (bounds.box_extent.z * 2.0)
        mesh_scale = unreal.Vector(s, s, s)
        pickup.set_relative_location(unreal.Vector(-bounds.origin.x * s, -bounds.origin.y * s,
                                                   -(bounds.origin.z - bounds.box_extent.z) * s), False, False)
    else:
        pickup.set_relative_location(unreal.Vector(0, 0, mesh_z), False, False)
    pickup.set_relative_scale3d(mesh_scale)
    walk_through(actor)
    fit_range(actor, radius)
    return place(actor, label, folder, tags)


def walk_through(actor):
    """Models use the Trigger preset: the player walks through; InteractionRange drives the E prompt."""
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        comp.set_collision_profile_name("Trigger")
        comp.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
    return actor


MAX_RANGE = 200.0  # wide canopies (bamboo) would otherwise reach over neighbouring stations


def fit_range(actor, min_radius):
    """InteractionRange reaches past the model's footprint, so big models trigger from any side."""
    model = actor.get_component_by_class(unreal.StaticMeshComponent)
    ext = model.get_editor_property("static_mesh").get_bounds().box_extent
    scale = model.get_editor_property("relative_scale3d")
    reach = math.hypot(ext.x * scale.x, ext.y * scale.y) + 80.0
    for sphere in actor.get_components_by_class(unreal.SphereComponent):
        sphere.set_sphere_radius(min(max(min_radius, reach), MAX_RANGE))


def build_material_sources():
    n = 0
    for key, mesh, scale, prompt, positions in MATERIAL_SOURCES:
        for i, (x, y) in enumerate(positions):
            label = "Deco_{}_{}".format(key, i)
            if not is_open_ground(x, y, label):
                continue
            if key in MESHY_DECO:
                path, fit = MESHY_DECO[key]  # the item's own Meshy model, fitted to a height in cm
                size = fit
            else:
                path, size = mesh or FLOWERS[i % len(FLOWERS)], unreal.Vector(scale, scale, scale)
            spawn_interactable(path, unreal.Vector(x, y, ground_z(x, y)), (i * 67) % 360,
                               size, 0.0, label, "Materials", prompt, key,
                               ("Interact_DecoMaterial",))
            n += 1
    return n


def build_gate_station():
    # Station: a pile of folded cloth just inside the gate.
    x, y = GATE[0] + 120.0, GATE[1] + 220.0
    spawn_interactable(FLAG, unreal.Vector(x, y, ground_z(x, y)), 90.0, unreal.Vector(0.5, 0.5, 0.5), 0.0,
                       "Station_Gate", "Gate", "Trang trí cổng lễ hội", "FestivalGate", ("Interact_FestivalGate",))
    # Revealed when done: a second, lower cloth row and flowers at both posts.
    half = 300.0
    left, right = (GATE[0] - half, GATE[1]), (GATE[0] + half, GATE[1])
    tags = ("FestivalDecor_Gate",)
    pennant_line(left, right, 250.0, "GateDecor_Cloth", "Gate", tags, sag_cm=40.0, spacing=35.0)
    for side, (px, py) in (("L", left), ("R", right)):
        for j, dx in enumerate((-60.0, 60.0)):
            fx, fy = px + dx, py + 40.0
            spawn_mesh(FLOWERS[j], unreal.Vector(fx, fy, ground_z(fx, fy)), unreal.Rotator(0, 0, j * 90),
                       unreal.Vector(1.2, 1.2, 1.2), "GateDecor_Flower{}{}".format(side, j), "Gate", tags)


def build_hang_station():
    x, y = SQUARE[0] + 250.0, SQUARE[1] + 120.0
    spawn_interactable(BARREL, unreal.Vector(x, y, ground_z(x, y)), 0.0, unreal.Vector(0.8, 0.8, 0.8), 0.0,
                       "Station_Hang", "FestivalSquare", "Treo đèn, vải, hoa", "FestivalHang", ("Interact_FestivalHang",))
    # Revealed when done: cloth lines crossing the square, lanterns, flowers at the corner posts.
    tags = ("FestivalDecor_Hang",)
    c = [(SQUARE[0] - 700, SQUARE[1] - 700), (SQUARE[0] + 700, SQUARE[1] - 700),
         (SQUARE[0] + 700, SQUARE[1] + 700), (SQUARE[0] - 700, SQUARE[1] + 700)]
    pennant_line(c[0], c[2], 360.0, "HangDecor_ClothA", "FestivalSquare", tags, sag_cm=90.0)
    pennant_line(c[1], c[3], 360.0, "HangDecor_ClothB", "FestivalSquare", tags, sag_cm=90.0)
    for i in range(1, 8):
        t = i / 8.0
        for line, (a, b) in enumerate(((c[0], c[2]), (c[1], c[3]))):
            lx, ly = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            lz = ground_z(lx, ly) + 360.0 - 90.0 * 4.0 * t * (1.0 - t) - 45.0
            spawn_mesh(CYLINDER, unreal.Vector(lx, ly, lz), unreal.Rotator(0, 0, 0), unreal.Vector(0.28, 0.28, 0.36),
                       "HangDecor_Lantern{}_{}".format(line, i), "FestivalSquare", tags, CLOTH_MATERIALS[0])
    for i, (px, py) in enumerate(c):
        fx, fy = px + (60.0 if px < SQUARE[0] else -60.0), py + (60.0 if py < SQUARE[1] else -60.0)
        spawn_mesh(FLOWERS[i % len(FLOWERS)], unreal.Vector(fx, fy, ground_z(fx, fy)), unreal.Rotator(0, 0, i * 45),
                   unreal.Vector(1.3, 1.3, 1.3), "HangDecor_Flower{}".format(i), "FestivalSquare", tags)


def build_spot_props(key, x, y, z, yaw):
    """Props for one spot, hidden at play start and shown when the player places them."""
    tags = ("FestivalProp", "FestivalProp_" + key)
    folder = "FestivalProps"
    rot = unreal.Rotator(0, 0, yaw)
    fwd = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
    side = (-fwd[1], fwd[0])

    def at(f, s):
        return x + fwd[0] * f + side[0] * s, y + fwd[1] * f + side[1] * s

    if key == "Table":
        spawn_fitted(FAB_TABLE, x, y, z, yaw, "Prop_Table", folder, tags, height=75.0)
    elif key == "Chairs":
        for i, s in enumerate((-110.0, 0.0, 110.0)):
            bx, by = at(0.0, s)
            spawn_fitted(FAB_STOOL, bx, by, z, yaw, "Prop_Chair{}".format(i), folder, tags, scale=0.5)
    elif key == "FlowerBasket":
        spawn_fitted(FAB_POTS, x, y, z, yaw, "Prop_Basket", folder, tags, height=90.0)
    elif key == "Loom":
        spawn_fitted(FAB_LOOM, x, y, z, yaw, "Prop_Loom", folder, tags, height=150.0)
    elif key == "SilkCloth":
        # Two posts and a rail, with ponchos of silk hanging from it.
        a, b = at(0.0, -150.0), at(0.0, 150.0)
        spawn_post(a[0], a[1], 230.0, "Prop_Silk_PostA", folder, tags)
        spawn_post(b[0], b[1], 230.0, "Prop_Silk_PostB", folder, tags)
        spawn_mesh(BEAM, unreal.Vector(x, y, z + 215.0), unreal.Rotator(0, 90, yaw + 90.0), unreal.Vector(0.3, 0.3, 1.25),
                   "Prop_Silk_Rail", folder, tags)
        for i, s in enumerate((-85.0, 0.0, 85.0)):
            bx, by = at(0.0, s)
            spawn_fitted(FAB_PONCHO, bx, by, z + 100.0, yaw, "Prop_Silk_Poncho{}".format(i), folder, tags, height=110.0)
    elif key == "Shelf":
        spawn_fitted(FAB_SHELF, x, y, z, yaw, "Prop_Shelf", folder, tags, height=170.0)
    elif key == "Outfit":
        # The sample outfit stands on the Meshy dress form (the model made for the tailoring house) instead of a posed villager.
        spawn_fitted(DRESS_FORM, x, y, z, yaw, "Prop_Outfit", folder, tags, height=165.0)


def build_spots():
    placed = 0
    for key, prompt, (dx, dy) in SPOTS:
        x, y = SQUARE[0] + dx, SQUARE[1] + dy
        label = "Spot_" + key
        if not is_open_ground(x, y, label):
            continue
        z = ground_z(x, y)
        yaw = yaw_to((x, y), SQUARE)
        # The spot itself: a flat marker disc the outline lights up; it hides once used.
        spawn_interactable(CYLINDER, unreal.Vector(x, y, z), 0.0, unreal.Vector(1.2, 1.2, 0.02), 1.0, label,
                           "FestivalSpots", "Đặt " + prompt.lower(), key, ("Interact_FestivalSpot",), radius=150.0)
        build_spot_props(key, x, y, z, yaw)
        placed += 1
    return placed


def build_reeling_pot():
    # In front of the reeling workshop, on the side facing the square.
    face = yaw_to(REELING_WORKSHOP, SQUARE)
    x = REELING_WORKSHOP[0] + math.cos(math.radians(face)) * 420.0
    y = REELING_WORKSHOP[1] + math.sin(math.radians(face)) * 420.0
    if not is_open_ground(x, y, "Station_ReelingPot"):
        x, y = REELING_WORKSHOP[0] - 450.0, REELING_WORKSHOP[1]
    z = ground_z(x, y)
    # The Meshy reeling pot model (stove, pot and frame in one), fitted to 130 cm.
    spawn_interactable(REEL_POT, unreal.Vector(x, y, z), face, 130.0, 0.0, "Station_ReelingPot",
                       "Reeling", "Ươm tơ (nồi nước nóng)", "ReelingPot", ("Interact_ReelingPot",))


def rebuild_props(keys):
    """Replaces only the arranging-spot props for `keys` (the Outfit stand and everything else stay)."""
    level_subsystem.set_current_level_by_name(SUBLEVEL_PATH.split("/")[-1])
    removed = 0
    for actor in actor_subsystem.get_all_level_actors():
        tags = [str(t) for t in actor.tags]
        if any("FestivalProp_" + k in tags for k in keys):
            actor_subsystem.destroy_actor(actor)
            removed += 1
    for key, prompt, (dx, dy) in SPOTS:
        if key not in keys:
            continue
        x, y = SQUARE[0] + dx, SQUARE[1] + dy
        z = ground_z(x, y)
        build_spot_props(key, x, y, z, yaw_to((x, y), SQUARE))
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)  # maps and the Fab meshes they use
    level_subsystem.set_current_level_by_name("PersistentLevel")
    print("CHAPTER2_PROPS removed={} spawned={}".format(removed, len(spawned)))


def main():
    if globals().get("STAGE") == "props":
        rebuild_props(globals().get("PROP_KEYS") or ("Table", "Chairs", "FlowerBasket", "Loom", "SilkCloth"))
        return
    ensure_sublevel()
    level_subsystem.set_current_level_by_name(SUBLEVEL_PATH.split("/")[-1])
    removed = clear_previous_build()
    sources = build_material_sources()
    build_gate_station()
    build_hang_station()
    spots = build_spots()
    build_reeling_pot()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    level_subsystem.set_current_level_by_name("PersistentLevel")
    count = sum(1 for a in actor_subsystem.get_all_level_actors() if "Chapter2" in [str(t) for t in a.tags])
    print("CHAPTER2_BUILD removed={} placed={} sources={} spots={}".format(removed, count, sources, spots))
    for line in skipped:
        print("SKIPPED", line)


main()
