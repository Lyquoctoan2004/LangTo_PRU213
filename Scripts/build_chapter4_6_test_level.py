import math

import unreal


# Builds /Game/SilkVillage/Maps/Chapter4to6_MiniGameTest: a flat test map with every Chapter 4-6 station and NPC.
# UVillageStorySubsystem recognises the map name and starts on the morning of Day 4 (Days 1-3 done, thread in the
# bag, Quest 18 running); scripts/test_chapter4_6_flow.py can jump to Day 5, the free days or the festival with
# debug_jump_to_day(5 / 6 / 9). Run build_chapter4_6_story.py first (story tables), then this.
# Re-running rebuilds the map from scratch.
TEST_MAP = "/Game/SilkVillage/Maps/Chapter4to6_MiniGameTest"
STORY_MAP = "/Game/Mountain"  # the test map borrows its game mode, so the same pawn (with BPC_Inventory) spawns
FOLDER = "MiniGameTest4to6"

INTERACTABLE_BP = "/Game/SilkVillage/Blueprints/BP_VillageInteractable"
NPC_BP = "/Game/SilkVillage/NPC/BP_VillageNPC"
NPC_ANIM_DIR = "/Game/SilkVillage/NPC/Anim"
NPC_MESHES = {
    "Elder": "/Game/MeshyImports/Import_20261022_153724/Elder_in_Traditional_Attire_Import0002",
    "Blue": "/Game/MeshyImports/Import_20261022_153551/Man_in_Blue_Traditional_Attire_Import0001",
    "Highland": "/Game/MeshyImports/Import_20261022_153747/Vibrant_Highland_Heritage_Import0003",
    "Crimson": "/Game/MeshyImports/Import_20261022_153843/The_Crimson_Sash_Import0004",
}
OUTLINE_MATERIAL = "/Game/SilkVillage/Chapter1/M_InteractOutline"  # made by build_chapter1_village.py

CUBE = "/Game/LevelPrototyping/Meshes/SM_Cube"
CYLINDER = "/Game/LevelPrototyping/Meshes/SM_Cylinder"
BEAM = "/Game/MS/3D/Worn_Wooden_Beam_tgnhcaafa/SM_Worn_Wooden_Beam_tgnhcaafa_00_N1"
BARREL = "/Game/MS/3D/Wooden_Barrel_tmgmfeefa/SM_Wooden_Barrel_tmgmfeefa_00_N1"
FLOWER = "/Game/OWD_Plants_Pack/Plants/Meshes/Variation/SM_Plant_03_1_RY"
BOARDS = "/Game/MS/3D/Old_Wood_Boards_ricaS/SM_Old_Wood_Boards_ricaS_00_N1"

# Layout (cm). The player starts at the south edge looking north (+Y); stations sit in rows 500 cm apart.
START = (0.0, -1500.0)
NPCS = [  # id, mesh key, position, display name, role, prompt
    ("NPC_Chief", "Elder", (0.0, 1700.0), "Bà Lò Thị Xuân", "Trưởng làng", "Nói chuyện với bà Xuân (Trưởng làng)"),
    ("NPC_SilkMaster", "Blue", (-1900.0, 900.0), "Ông Lường Văn Khánh", "Nghệ nhân nuôi tằm & ươm tơ",
     "Nói chuyện với ông Khánh (nuôi tằm)"),
    ("NPC_ThreadDyer", "Highland", (1000.0, 1700.0), "Cô Tòng Thị Mai", "Nghệ nhân se tơ & nhuộm", "Nói chuyện với cô Mai (se tơ)"),
    ("NPC_WeaverTailor", "Crimson", (-1000.0, 1700.0), "Cô Quàng Thị Lụa", "Nghệ nhân dệt & may", "Nói chuyện với cô Lụa (dệt)"),
]
# (key, mesh, scale, position, tag, prompt)
STATIONS = [
    # Chapter 4-5 crafts
    ("Dye", BARREL, (0.9, 0.9, 0.7), (1000.0, 1100.0), "Interact_Craft", "Nồi nhuộm"),
    ("Weave", BARREL, (0.9, 0.9, 0.7), (-1000.0, 1100.0), "Interact_Craft", "Khung dệt"),
    ("Cut", BARREL, (0.7, 0.7, 0.55), (-500.0, 1100.0), "Interact_Craft", "Bàn cắt vải"),
    ("Sew", BARREL, (0.7, 0.7, 0.55), (0.0, 1100.0), "Interact_Craft", "Khung may"),
    # Chapter 6 practice hall
    ("PracCare", BARREL, (0.7, 0.7, 0.55), (-1250.0, 400.0), "Interact_Craft", "Luyện nuôi tằm"),
    ("PracReel", BARREL, (0.7, 0.7, 0.55), (-750.0, 400.0), "Interact_Craft", "Luyện ươm tơ"),
    ("PracSpin", BARREL, (0.7, 0.7, 0.55), (-250.0, 400.0), "Interact_Craft", "Luyện se tơ"),
    ("PracDye", BARREL, (0.7, 0.7, 0.55), (250.0, 400.0), "Interact_Craft", "Luyện nhuộm"),
    ("PracWeave", BARREL, (0.7, 0.7, 0.55), (750.0, 400.0), "Interact_Craft", "Luyện dệt"),
    ("PracTailor", BARREL, (0.7, 0.7, 0.55), (1250.0, 400.0), "Interact_Craft", "Luyện may"),
    # Festival stage (the Master's Challenge)
    ("ChDye", BARREL, (0.8, 0.8, 0.65), (-750.0, -300.0), "Interact_Craft", "Bàn nhuộm sân khấu"),
    ("ChWeave", BARREL, (0.8, 0.8, 0.65), (-250.0, -300.0), "Interact_Craft", "Khung dệt sân khấu"),
    ("ChTailor", BARREL, (0.8, 0.8, 0.65), (250.0, -300.0), "Interact_Craft", "Bàn may sân khấu"),
    ("ChStage", CYLINDER, (1.6, 1.6, 0.18), (750.0, -300.0), "Interact_Craft", "Sàn diễn"),
    # Gathering (Chapter 4 dye materials, Chapter 6 lost pattern) and the festival signs
    ("DYE_MATERIAL_001", FLOWER, (1.4, 1.4, 1.4), (1800.0, 300.0), "Interact_Gather", "Hái nguyên liệu nhuộm (lá, vỏ cây, đất màu)"),
    ("DYE_MATERIAL_001", FLOWER, (1.4, 1.4, 1.4), (2300.0, 300.0), "Interact_Gather", "Hái nguyên liệu nhuộm (lá, vỏ cây, đất màu)"),
    ("DYE_WATER_001", BARREL, (0.55, 0.55, 0.45), (1800.0, 900.0), "Interact_Gather", "Lấy nước suối"),
    ("PAT_ANCIENT_FRAGMENT_001", BOARDS, (0.45, 0.45, 0.45), (-2300.0, 300.0), "Interact_Gather", "Nhặt mảnh hoa văn cổ"),
    ("SIGN_MARKET", BEAM, (0.6, 0.6, 0.5), (-1800.0, -300.0), "Interact_Dialogue", "Chợ lụa (Silk Market)"),
    ("SIGN_CRAFT", BEAM, (0.6, 0.6, 0.5), (1800.0, -300.0), "Interact_Dialogue", "Khu nghệ nhân (Crafting Area)"),
    ("SIGN_CULTURE", BEAM, (0.6, 0.6, 0.5), (2300.0, -300.0), "Interact_Dialogue", "Khu văn hóa (Cultural Area)"),
]

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


def mesh(path, xyz, yaw, scale, label, sub, tags=()):
    actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*xyz), unreal.Rotator(0, 0, yaw))
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return place(actor, label, sub, tags)


def post(x, y, height, label, sub):
    return mesh(BEAM, (x, y, height * 0.5), 0.0, (1.0, 1.0, height / 240.0), label, sub)


def walk_through(actor):
    """Models use the Trigger preset: the player walks through; InteractionRange drives the E prompt."""
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        comp.set_collision_profile_name("Trigger")
        comp.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
    return actor


def interactable(path, xyz, scale, mesh_z, label, sub, prompt, item_id, tags, radius=200.0):
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(INTERACTABLE_BP).generated_class(),
                                                   unreal.Vector(*xyz), unreal.Rotator(0, 0, 0))
    actor.set_editor_property("ItemID", unreal.Name(item_id))  # no DT_ItemData row -> FallbackPrompt
    actor.set_editor_property("FallbackPrompt", unreal.Text(prompt))
    pickup = actor.get_component_by_class(unreal.StaticMeshComponent)
    pickup.set_static_mesh(unreal.load_asset(path))
    pickup.set_relative_location(unreal.Vector(0, 0, mesh_z), False, False)
    pickup.set_relative_scale3d(unreal.Vector(*scale))
    walk_through(actor)
    for sphere in actor.get_components_by_class(unreal.SphereComponent):
        sphere.set_sphere_radius(radius)
    return place(actor, label, sub, tags)


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
    for a in actor_subsystem.get_all_level_actors():
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
    for key, path, scale, pos, tag, prompt in STATIONS:
        interactable(path, (pos[0], pos[1], 0.0), scale, 0.0, "Station_" + key, "Stations", prompt, key, (tag,), radius=170.0)
    # Two posts behind the dye pot and the loom, like the village frames.
    for x, y in ((1000.0, 1100.0), (-1000.0, 1100.0)):
        for dy in (-70.0, 70.0):
            post(x + 110.0, y + dy, 150.0, "Frame_Post", "Stations")


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
    level_subsystem.save_current_level()
    count = len([a for a in actor_subsystem.get_all_level_actors() if str(a.get_folder_path()).startswith(FOLDER)])
    print("TEST_LEVEL built {} actors={}".format(TEST_MAP, count))


main()

