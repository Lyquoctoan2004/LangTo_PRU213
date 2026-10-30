import math

import unreal


# Builds /Game/SilkVillage/Maps/Chapter3_MiniGameTest: a flat test map with every Chapter 3 station and NPC
# within a few steps of the player start. UVillageStorySubsystem recognises the map name and starts on the
# morning of Day 3 (Day 1-2 done, 50 cocoons and a strand of raw silk in the bag, Quest 14 running), so the
# whole day can be played without Chapters 1-2. Run build_chapter3_story.py first (story tables), then this.
# Re-running rebuilds the map from scratch.
TEST_MAP = "/Game/SilkVillage/Maps/Chapter3_MiniGameTest"
STORY_MAP = "/Game/Mountain"  # the test map borrows its game mode, so the same pawn (with BPC_Inventory) spawns
FOLDER = "MiniGameTest3"

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

# Layout (cm). The player starts at the south edge looking north (+Y).
START = (0.0, -1500.0)
POT = (-300.0, 300.0)
WHEEL = (1000.0, 600.0)
NPCS = [  # id, mesh key, position, display name, role, prompt
    ("NPC_Chief", "Elder", (-350.0, 1300.0), "Bà Lò Thị Xuân", "Trưởng làng", "Nói chuyện với bà Xuân (Trưởng làng)"),
    ("NPC_SilkMaster", "Blue", (-900.0, 300.0), "Ông Lường Văn Khánh", "Nghệ nhân nuôi tằm & ươm tơ",
     "Nói chuyện với ông Khánh (nuôi tằm)"),
    ("NPC_ThreadDyer", "Highland", (1000.0, 1100.0), "Cô Tòng Thị Mai", "Nghệ nhân se tơ & nhuộm", "Nói chuyện với cô Mai (se tơ)"),
    ("NPC_WeaverTailor", "Crimson", (1600.0, 1100.0), "Cô Quàng Thị Lụa", "Nghệ nhân dệt & may", "Nói chuyện với cô Lụa (dệt)"),
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
    # Reeling pot on a stove, reel frame beside it (Day 3: pick the cocoons, find the thread, reel the batch).
    walk_through(mesh(CYLINDER, (POT[0], POT[1], 0.0), 0.0, (0.9, 0.9, 0.35), "Reel_Stove", "Reeling"))
    interactable(BARREL, (POT[0], POT[1], 0.0), (0.75, 0.75, 0.55), 35.0, "Station_ReelingPot", "Reeling",
                 "Ươm tơ (nồi nước nóng)", "ReelingPot", ("Interact_ReelingPot",))
    for i, dy in enumerate((-50.0, 50.0)):
        post(POT[0] + 110.0, POT[1] + dy, 150.0, "Reel_Post{}".format(i), "Reeling")

    # Spinning wheel beside cô Mai (same look as build_chapter3_village.py).
    wx, wy = WHEEL
    interactable(BARREL, (wx, wy, 0.0), (0.8, 0.8, 0.7), 0.0, "Station_SpinningWheel", "Spinning",
                 "Se tơ (guồng quay)", "SpinningWheel", ("Interact_SpinningWheel",))
    for i, dy in enumerate((-70.0, 70.0)):
        post(wx + 110.0, wy + dy, 150.0, "Spin_Post{}".format(i), "Spinning")
    mesh(CYLINDER, (wx + 110.0, wy, 120.0), 0.0, (0.18, 0.18, 1.4), "Spin_Spindle", "Spinning").set_actor_rotation(
        unreal.Rotator(90, 0, 0), False)
    mesh(CYLINDER, (wx + 110.0, wy, 80.0), 0.0, (0.55, 0.55, 0.08), "Spin_Wheel", "Spinning")


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
