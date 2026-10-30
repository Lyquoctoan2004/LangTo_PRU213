import math

import unreal


# Builds /Game/SilkVillage/Maps/Chapter2_MiniGameTest: a flat test map with every Chapter 2 station
# within a few steps of the player start. UVillageStorySubsystem recognises the map name and starts
# straight on Day 2 (32 cocoons in the bag, Quest 02 and Quest 03 running), so each mini-game can be
# played without going through Chapter 1. Run build_chapter1_story.py and build_chapter2_story.py
# first (story tables), then this. Re-running rebuilds the map from scratch.
TEST_MAP = "/Game/SilkVillage/Maps/Chapter2_MiniGameTest"
STORY_MAP = "/Game/Mountain"  # the test map borrows its game mode, so the same pawn (with BPC_Inventory) spawns
FOLDER = "MiniGameTest"

INTERACTABLE_BP = "/Game/SilkVillage/Blueprints/BP_VillageInteractable"
NPC_BP = "/Game/SilkVillage/NPC/BP_VillageNPC"
NPC_ANIM_DIR = "/Game/SilkVillage/NPC/Anim"
NPC_MESHES = {
    "Elder": "/Game/MeshyImports/Import_20261022_153724/Elder_in_Traditional_Attire_Import0002",
    "Blue": "/Game/MeshyImports/Import_20261022_153551/Man_in_Blue_Traditional_Attire_Import0001",
}
OUTLINE_MATERIAL = "/Game/SilkVillage/Chapter1/M_InteractOutline"  # made by build_chapter1_village.py

CUBE = "/Game/LevelPrototyping/Meshes/SM_Cube"
CYLINDER = "/Game/LevelPrototyping/Meshes/SM_Cylinder"
BEAM = "/Game/MS/3D/Worn_Wooden_Beam_tgnhcaafa/SM_Worn_Wooden_Beam_tgnhcaafa_00_N1"
BARREL = "/Game/MS/3D/Wooden_Barrel_tmgmfeefa/SM_Wooden_Barrel_tmgmfeefa_00_N1"
BOARDS = "/Game/MS/3D/Old_Wood_Boards_ricaS/SM_Old_Wood_Boards_ricaS_00_N1"
ROPE = "/Game/MS/3D/Rope_Coil_vcsoddfs/SM_Rope_Coil_vcsoddfs_00_N1"
BAMBOO = "/Game/Bam_boo/FBX_AS01_BambusaVulgaris"
FLAG = "/Game/MedievalVillage/Meshes/Mesh_Components/SM_Flag_01"
PENNANT = "/Game/MedievalVillage/Meshes/Mesh_Components/SM_Flag_Small01"
FLOWER = "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_03_1_RY"
CLOTH = "/Game/MedievalVillage/Materials/Meshspecific/Flags/MI_Flag_01"

# Layout (cm). The player starts at the south edge looking north (+Y) over everything.
START = (0.0, -1500.0)
TRAY = (-1300.0, -100.0)
SOURCE_ROW = (-1200.0, -800.0)  # first material source; the rest follow along +X
RANGE_GAP = 40.0  # clear ground between two neighbouring InteractionRanges
GATE = (0.0, 250.0)
SQUARE = (0.0, 1300.0)
POT = (1000.0, 300.0)
NPCS = [  # id, mesh key, position, display name, role, prompt
    ("NPC_Chief", "Elder", (-350.0, 950.0), "Bà Lò Thị Xuân", "Trưởng làng", "Nói chuyện với bà Xuân (Trưởng làng)"),
    ("NPC_SilkMaster", "Blue", (-800.0, 100.0), "Ông Lường Văn Khánh", "Nghệ nhân nuôi tằm & ươm tơ",
     "Nói chuyện với ông Khánh (nuôi tằm)"),
]
# Material sources in a row in front of the gate: objective key, mesh, scale, prompt (stock 3 each).
SOURCES = [
    ("Bamboo", BAMBOO, 0.5, "Chặt tre"), ("Bamboo", BAMBOO, 0.5, "Chặt tre"),
    ("Cloth", FLAG, 0.5, "Lấy vải màu"), ("Cloth", FLAG, 0.5, "Lấy vải màu"),
    ("Flower", FLOWER, 1.0, "Hái hoa"), ("Flower", FLOWER, 1.0, "Hái hoa"),
    ("Flower", FLOWER, 1.0, "Hái hoa"), ("Flower", FLOWER, 1.0, "Hái hoa"),
    ("Rope", ROPE, 1.4, "Lấy dây thừng"), ("Rope", ROPE, 1.4, "Lấy dây thừng"),
    ("WoodDeco", BOARDS, 0.8, "Lấy đồ gỗ trang trí"),
]
SPOTS = [("Table", "Đặt bàn trưng bày lụa", (-350.0, 250.0)), ("Chairs", "Đặt ghế cho khách", (350.0, 250.0)),
         ("FlowerBasket", "Đặt giỏ hoa", (0.0, 500.0))]

eal = unreal.EditorAssetLibrary
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def story_game_mode():
    level_subsystem.load_level(STORY_MAP)
    settings = unreal.GameplayStatics.get_actor_of_class(editor_subsystem.get_editor_world(), unreal.WorldSettings)
    mode = settings.get_editor_property("default_game_mode") if settings else None
    print("TEST_LEVEL game mode from Mountain:", mode.get_name() if mode else "project default")
    return mode


def place(actor, label, sub, tags=()):
    actor.set_actor_label(label)
    actor.set_folder_path("{}/{}".format(FOLDER, sub))
    actor.tags = [unreal.Name(t) for t in tags]
    return actor


def mesh(path, xyz, yaw, scale, label, sub, tags=(), material=None):
    actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*xyz), unreal.Rotator(0, 0, yaw))
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        actor.static_mesh_component.set_material(0, unreal.load_asset(material))
    return place(actor, label, sub, tags)


def post(x, y, height, label, sub, tags=()):
    return mesh(BEAM, (x, y, height * 0.5), 0.0, (1.0, 1.0, height / 240.0), label, sub, tags)


def cloth_line(a, b, height, label, sub, tags=(), sag=50.0):
    count = max(2, int(math.dist(a, b) / 45.0))
    yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    for i in range(1, count):
        t = i / count
        xyz = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, height - sag * 4.0 * t * (1.0 - t))
        mesh(PENNANT, xyz, yaw, (1.4, 1.4, 1.4), "{}_{:02d}".format(label, i), sub, tags, CLOTH)


def interactable(path, xyz, scale, mesh_z, label, sub, prompt, item_id, tags, radius=180.0):
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(INTERACTABLE_BP).generated_class(),
                                                   unreal.Vector(*xyz), unreal.Rotator(0, 0, 0))
    actor.set_editor_property("ItemID", unreal.Name(item_id))  # no DT_ItemData row -> FallbackPrompt
    actor.set_editor_property("FallbackPrompt", unreal.Text(prompt))
    pickup = actor.get_component_by_class(unreal.StaticMeshComponent)
    pickup.set_static_mesh(unreal.load_asset(path))
    pickup.set_relative_location(unreal.Vector(0, 0, mesh_z), False, False)
    pickup.set_relative_scale3d(unreal.Vector(*scale))
    walk_through(actor)
    fit_range(actor, radius)
    return place(actor, label, sub, tags)


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


def npc(npc_id, key, pos, name, role, prompt):
    skel = unreal.load_asset(NPC_MESHES[key])
    bounds = skel.get_bounds()
    z = -(bounds.origin.z - bounds.box_extent.z)
    yaw = math.degrees(math.atan2(START[1] - pos[1], START[0] - pos[0]))
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(NPC_BP).generated_class(),
                                                   unreal.Vector(pos[0], pos[1], z), unreal.Rotator(0, 0, yaw))
    idle = unreal.load_asset("{}/AS_NPC_Idle_{}".format(NPC_ANIM_DIR, key))
    actor.set_editor_property("ItemID", unreal.Name(npc_id))
    actor.set_editor_property("FallbackPrompt", unreal.Text(prompt))
    actor.set_editor_property("DisplayName", unreal.Text(name))
    actor.set_editor_property("DisplayRole", unreal.Text(role))
    actor.set_editor_property("IdleAnimation", idle)
    actor.set_editor_property("TalkAnimation", unreal.load_asset("{}/AS_NPC_Talk_{}".format(NPC_ANIM_DIR, key)))
    for sm in actor.get_components_by_class(unreal.StaticMeshComponent):
        sm.set_visibility(False)
        sm.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for sphere in actor.get_components_by_class(unreal.SphereComponent):
        sphere.set_sphere_radius(250.0)
    comp = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    comp.set_skeletal_mesh_asset(skel)
    comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    comp.set_editor_property("animation_data", unreal.SingleAnimationPlayData(anim_to_play=idle, saved_looping=True, saved_playing=True))
    comp.set_collision_profile_name("Trigger")
    for prim in actor.get_components_by_class(unreal.PrimitiveComponent):
        if prim.get_name() == "BodyBlock":
            prim.set_collision_profile_name("Trigger")
    return place(actor, npc_id, "NPC", ("NPC", npc_id, "Interactable"))


def build_environment():
    mesh(CUBE, (0.0, 0.0, -20.0), 0.0, (80.0, 80.0, 0.2), "Floor", "Env")
    sun = actor_subsystem.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, -45, 30))
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    place(sun, "Sun", "Env")
    for cls, label in ((unreal.SkyLight, "SkyLight"), (unreal.SkyAtmosphere, "SkyAtmosphere"), (unreal.ExponentialHeightFog, "Fog")):
        place(actor_subsystem.spawn_actor_from_class(cls, unreal.Vector(0, 0, 500)), label, "Env")
    sky = actor_subsystem.get_all_level_actors()
    for a in sky:
        if isinstance(a, unreal.SkyLight):
            a.light_component.set_editor_property("real_time_capture", True)
    start = actor_subsystem.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(START[0], START[1], 100.0), unreal.Rotator(0, 0, 90))
    place(start, "PlayerStart", "Env")
    if eal.does_asset_exist(OUTLINE_MATERIAL):
        volume = actor_subsystem.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 300))
        volume.set_editor_property("unbound", True)
        settings = volume.get_editor_property("settings")
        settings.set_editor_property("weighted_blendables", unreal.WeightedBlendables(
            array=[unreal.WeightedBlendable(weight=1.0, object=unreal.load_asset(OUTLINE_MATERIAL))]))
        volume.set_editor_property("settings", settings)
        place(volume, "PP_InteractOutline", "Env")
    else:
        print("TEST_LEVEL no outline material; run build_chapter1_village.py once for station outlines")


def build_stations():
    # Quest 02: one silkworm tray (opens the cocoon harvest on Day 2).
    walk_through(mesh(CYLINDER, (TRAY[0], TRAY[1], 0.0), 0.0, (0.25, 0.25, 0.6), "SilkTray_Stand", "Cocoon"))
    interactable(CYLINDER, (TRAY[0], TRAY[1], 0.0), (1.1, 1.1, 0.06), 60.0, "SilkTray", "Cocoon",
                 "Thu hoạch kén", "SilkTray", ("Interact_SilkwormTray",))

    # Quest 03: material sources in a row.
    # Spaced by their own fitted ranges so no two triggers touch.
    x, prev_range = SOURCE_ROW[0], None
    for i, (key, path, s, prompt) in enumerate(SOURCES):
        actor = interactable(path, (x, SOURCE_ROW[1], 0.0), (s, s, s), 0.0, "Deco_{}_{}".format(key, i), "Materials",
                             prompt, key, ("Interact_DecoMaterial",), radius=110.0)
        r = interaction_range(actor).get_unscaled_sphere_radius()
        if prev_range is not None:
            x += prev_range + r + RANGE_GAP
            actor.set_actor_location(unreal.Vector(x, SOURCE_ROW[1], 0.0), False, False)
        prev_range = r

    # Gate: posts + lintel, the station, and decorations revealed when it is done.
    for side, dx in (("L", -300.0), ("R", 300.0)):
        post(GATE[0] + dx, GATE[1], 420.0, "Gate_Post" + side, "Gate")
    mesh(BEAM, (GATE[0], GATE[1], 400.0), 0.0, (1.2, 1.2, 680.0 / 240.0), "Gate_Lintel", "Gate").set_actor_rotation(
        unreal.Rotator(0, 90, 0), False)
    interactable(FLAG, (GATE[0] + 150.0, GATE[1] - 200.0, 0.0), (0.5, 0.5, 0.5), 0.0, "Station_Gate", "Gate",
                 "Trang trí cổng lễ hội", "FestivalGate", ("Interact_FestivalGate",))
    cloth_line((GATE[0] - 300.0, GATE[1]), (GATE[0] + 300.0, GATE[1]), 330.0, "GateDecor_Cloth", "Gate", ("FestivalDecor_Gate",))
    for side, dx in (("L", -360.0), ("R", 360.0)):
        mesh(FLOWER, (GATE[0] + dx, GATE[1], 0.0), 0.0, (1.2, 1.2, 1.2), "GateDecor_Flower" + side, "Gate", ("FestivalDecor_Gate",))

    # Square: corner posts, hang station, decorations revealed when it is done.
    corners = [(SQUARE[0] + sx * 500.0, SQUARE[1] + sy * 500.0) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for i, (x, y) in enumerate(corners):
        post(x, y, 360.0, "Square_Post{}".format(i), "Square")
    interactable(BARREL, (SQUARE[0] + 200.0, SQUARE[1] - 250.0, 0.0), (0.8, 0.8, 0.8), 0.0, "Station_Hang", "Square",
                 "Treo đèn, vải, hoa", "FestivalHang", ("Interact_FestivalHang",))
    cloth_line(corners[0], corners[2], 340.0, "HangDecor_ClothA", "Square", ("FestivalDecor_Hang",), sag=80.0)
    cloth_line(corners[1], corners[3], 340.0, "HangDecor_ClothB", "Square", ("FestivalDecor_Hang",), sag=80.0)
    for i, (x, y) in enumerate(corners):
        mesh(CYLINDER, (x, y, 250.0), 0.0, (0.3, 0.3, 0.4), "HangDecor_Lantern{}".format(i), "Square", ("FestivalDecor_Hang",), CLOTH)

    # Arranging spots with their hidden props.
    for key, prompt, (dx, dy) in SPOTS:
        x, y = SQUARE[0] + dx, SQUARE[1] + dy
        interactable(CYLINDER, (x, y, 0.0), (1.2, 1.2, 0.02), 1.0, "Spot_" + key, "Spots", prompt, key,
                     ("Interact_FestivalSpot",), radius=150.0)
        tags = ("FestivalProp", "FestivalProp_" + key)
        if key == "Table":
            for i, dxx in enumerate((-70.0, 70.0)):
                mesh(BARREL, (x + dxx, y, 0.0), 0.0, (0.7, 0.7, 0.7), "Prop_Table_Leg{}".format(i), "Props", tags)
            mesh(BOARDS, (x, y, 75.0), 90.0, (1.0, 1.0, 1.0), "Prop_Table_Top", "Props", tags)
        elif key == "Chairs":
            for i, dxx in enumerate((-90.0, 0.0, 90.0)):
                mesh(BARREL, (x + dxx, y, 0.0), 0.0, (0.45, 0.45, 0.45), "Prop_Chair{}".format(i), "Props", tags)
        else:
            mesh(BARREL, (x, y, 0.0), 0.0, (0.55, 0.55, 0.4), "Prop_Basket", "Props", tags)
            mesh(FLOWER, (x, y, 40.0), 0.0, (0.9, 0.9, 0.9), "Prop_Basket_Flowers", "Props", tags)

    # Quest 04: reeling pot on a stove, reel frame beside it.
    walk_through(mesh(CYLINDER, (POT[0], POT[1], 0.0), 0.0, (0.9, 0.9, 0.35), "Reel_Stove", "Reeling"))
    interactable(BARREL, (POT[0], POT[1], 0.0), (0.75, 0.75, 0.55), 35.0, "Station_ReelingPot", "Reeling",
                 "Ươm tơ (nồi nước nóng)", "ReelingPot", ("Interact_ReelingPot",))
    for i, dy in enumerate((-50.0, 50.0)):
        post(POT[0] + 110.0, POT[1] + dy, 150.0, "Reel_Post{}".format(i), "Reeling")


def interaction_range(actor):
    return next((s for s in actor.get_components_by_class(unreal.SphereComponent) if s.get_name() == "InteractionRange"), None)


def report_overlaps():
    """Two InteractionRanges that touch make E pick an unpredictable station; list any that do."""
    ranges = []
    for actor in actor_subsystem.get_all_level_actors():
        sphere = interaction_range(actor)
        if sphere:
            loc = sphere.get_world_location()
            ranges.append((actor.get_actor_label(), loc.x, loc.y, sphere.get_scaled_sphere_radius()))
    clashes = ["{} <-> {}".format(a[0], b[0]) for i, a in enumerate(ranges) for b in ranges[i + 1:]
               if math.hypot(a[1] - b[1], a[2] - b[2]) < a[3] + b[3]]
    print("TEST_LEVEL interaction ranges={} overlapping={}".format(len(ranges), clashes or "none"))


def main():
    game_mode = story_game_mode()
    if eal.does_asset_exist(TEST_MAP):
        level_subsystem.load_level(STORY_MAP)  # make sure the old test map is not the open one
        eal.delete_asset(TEST_MAP)  # generated by this script only
    if not level_subsystem.new_level(TEST_MAP):
        raise RuntimeError("Could not create " + TEST_MAP)
    if game_mode:
        settings = unreal.GameplayStatics.get_actor_of_class(editor_subsystem.get_editor_world(), unreal.WorldSettings)
        settings.set_editor_property("default_game_mode", game_mode)
    build_environment()
    build_stations()
    for spec in NPCS:
        npc(*spec)
    report_overlaps()
    level_subsystem.save_current_level()
    count = len([a for a in actor_subsystem.get_all_level_actors() if str(a.get_folder_path()).startswith(FOLDER)])
    print("TEST_LEVEL built {} actors={}".format(TEST_MAP, count))


main()
