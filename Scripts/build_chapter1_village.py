import math

import unreal


# Chapter 1 layout lives in its own always-loaded sublevel of Mountain so it can be
# rebuilt (or removed) without touching the hand-made persistent level.
MAP_PATH = "/Game/Mountain"
SUBLEVEL_PATH = "/Game/SilkVillage/Maps/Chapter1_Village"
ROOT_FOLDER = "Chapter1"

NPC_MESHES = {
    "Elder": "/Game/MeshyImports/Import_20261022_153724/Elder_in_Traditional_Attire_Import0002",
    "Blue": "/Game/MeshyImports/Import_20261022_153551/Man_in_Blue_Traditional_Attire_Import0001",
    "Highland": "/Game/MeshyImports/Import_20261022_153747/Vibrant_Highland_Heritage_Import0003",
    "Crimson": "/Game/MeshyImports/Import_20261022_153843/The_Crimson_Sash_Import0004",
}
# Meshy imports face +Y in Unreal; yaw offset applied so "face" points at the target.
# BP_VillageNPC already turns its mesh by -90, so only the plain villager actors need this.
NPC_FORWARD_YAW = 90.0
NPC_BP = "/Game/SilkVillage/NPC/BP_VillageNPC"
INTERACTABLE_BP = "/Game/SilkVillage/Blueprints/BP_VillageInteractable"
NPC_ANIM_DIR = "/Game/SilkVillage/NPC/Anim"  # AS_NPC_Idle_<Key> / AS_NPC_Talk_<Key> from build_npc_animations.py
# Bà Lò Thị Xuân, ông Lường Văn Khánh, cô Tòng Thị Mai, cô Quàng Thị Lụa (see NPC_ROLES_CHAPTER1.md).
NPC_PROMPTS = {
    "NPC_Chief": "Nói chuyện với bà Xuân (Trưởng làng)",
    "NPC_SilkMaster": "Nói chuyện với ông Khánh (nuôi tằm)",
    "NPC_ThreadDyer": "Nói chuyện với cô Mai (se tơ & nhuộm)",
    "NPC_WeaverTailor": "Nói chuyện với cô Lụa (dệt & may)",
}
# Nameplate above the head: (DisplayName, DisplayRole).
NPC_NAMEPLATES = {
    "NPC_Chief": ("Bà Lò Thị Xuân", "Trưởng làng"),
    "NPC_SilkMaster": ("Ông Lường Văn Khánh", "Nghệ nhân nuôi tằm & ươm tơ"),
    "NPC_ThreadDyer": ("Cô Tòng Thị Mai", "Nghệ nhân se tơ & nhuộm"),
    "NPC_WeaverTailor": ("Cô Quàng Thị Lụa", "Nghệ nhân dệt & may"),
}

BEAM = "/Game/MS/3D/Worn_Wooden_Beam_tgnhcaafa/SM_Worn_Wooden_Beam_tgnhcaafa_00_N1"  # 24 x 24 x 240 cm, pivot centered
PLANE = "/Game/LevelPrototyping/Meshes/SM_Plane"  # 100 x 100 cm
FLAG_BP = "/Game/MedievalVillage/Meshes/Blueprints/BP_Flag"
PENNANT = "/Game/MedievalVillage/Meshes/Mesh_Components/SM_Flag_Small01"
# SM_Flag_Small01 ships with WorldGridMaterial; the village flag instances give it colored cloth.
PENNANT_MATERIALS = [
    "/Game/MedievalVillage/Materials/Meshspecific/Flags/MI_Flag_01",
    "/Game/MedievalVillage/Materials/Meshspecific/Flags/MI_Flag_02",
    "/Game/MedievalVillage/Materials/Meshspecific/Flags/MI_RealFlags",
]
CYLINDER = "/Game/LevelPrototyping/Meshes/SM_Cylinder"  # 100 x 100 x 100 cm, pivot at base
BARREL = "/Game/MS/3D/Wooden_Barrel_tmgmfeefa/SM_Wooden_Barrel_tmgmfeefa_00_N1"
MULBERRY = "/Game/OWD_Plants_Pack/Plants/Meshes/SM_Plant_03_1_G"

# Runtime fonts cannot drive a TextRender, so the Vietnamese sign is a painted texture.
SIGN_SOURCE = "SourceArt/Chapter1/T_FestivalSign.png"
ASSET_DIR = "/Game/SilkVillage/Chapter1"
SIGN_TEXTURE = ASSET_DIR + "/T_FestivalSign"
SIGN_MATERIAL = ASSET_DIR + "/M_FestivalSign"
# Interactables near the player render to custom depth with this stencil (UVillageStorySubsystem);
# the post-process material outlines those pixels. Needs r.CustomDepth=3 (DefaultEngine.ini).
OUTLINE_MATERIAL = ASSET_DIR + "/M_InteractOutline"
OUTLINE_STENCIL = 252
OUTLINE_HLSL = """
float2 uv = GetDefaultSceneTextureUV(Parameters, 25);
float2 texel = View.BufferSizeAndInvSize.zw * Width;
float centre = abs(SceneTextureLookup(uv, 25, false).r - Stencil) < 0.5;
float edge = 0;
for (int i = 0; i < 8; i++)
{
    float a = i * 0.785398;
    float s = abs(SceneTextureLookup(uv + float2(cos(a), sin(a)) * texel, 25, false).r - Stencil) < 0.5;
    edge = max(edge, s);
}
return saturate(edge - centre) + centre * Fill + Dummy.r * 0;
"""

# Key points (cm). Player arrives from CustomizedCharacterStart (41316, -44860) heading +Y.
# Everything sits in the open meadow X 40000-43300, Y -43500..-39300; trees start past Y -39300.
GATE = (41700.0, -43500.0)
SQUARE = (42100.0, -41300.0)
SILK_HOUSE = (41200.0, -39270.0)  # existing Nha_Nuoi_Tam, about 7 x 10 m
SILK_TRAYS = [(41300.0, -39780.0), (41150.0, -39830.0), (41000.0, -39780.0)]
GARDEN = (42020.0, -39780.0)
# East workshop quarter behind the silkworm house (see HOUSE_COPIES / clear_east_area.py).
DYE_YARD = (43800.0, -37800.0)  # cô Mai: stream bank beside her house
WEAVE_YARD = (42700.0, -37900.0)  # cô Lụa: in front of her workshop, facing the shared yard

# ID, mesh, role, position, look-at point, interactable
NPCS = [
    ("NPC_Chief", "Elder", "VillageChief", (42050.0, -42150.0), GATE, True),
    ("NPC_SilkMaster", "Blue", "Silkworm+Reeling", (41550.0, -39650.0), SQUARE, True),
    ("NPC_ThreadDyer", "Highland", "Spinning+Dyeing", DYE_YARD, SQUARE, True),
    ("NPC_WeaverTailor", "Crimson", "Weaving+Sewing", WEAVE_YARD, SQUARE, True),
    ("Villager_01", "Highland", "Background", (42300.0, -41100.0), (42550.0, -40850.0), False),
    ("Villager_02", "Crimson", "Background", (41650.0, -40750.0), (41500.0, -40500.0), False),
    ("Villager_03", "Blue", "Background", (42600.0, -36900.0), (42850.0, -37050.0), False),
    ("Villager_04", "Elder", "Background", (42850.0, -37050.0), (42600.0, -36900.0), False),
]

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
spawned = []  # ground traces ignore what this build placed


def ensure_sublevel():
    if not world.get_path_name().startswith(MAP_PATH):
        raise RuntimeError("Open {} before running this script".format(MAP_PATH))
    for streaming in unreal.EditorLevelUtils.get_levels(world):
        if streaming.get_outer().get_path_name().startswith(SUBLEVEL_PATH):
            return streaming
    if unreal.EditorAssetLibrary.does_asset_exist(SUBLEVEL_PATH):
        streaming = unreal.EditorLevelUtils.add_level_to_world(world, SUBLEVEL_PATH, unreal.LevelStreamingAlwaysLoaded)
    else:
        streaming = unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded, SUBLEVEL_PATH, False)
    return streaming.get_loaded_level()


def clear_previous_build():
    removed = 0
    for actor in actor_subsystem.get_all_level_actors():
        if str(actor.get_folder_path()).startswith(ROOT_FOLDER) and SUBLEVEL_PATH.split("/")[-1] in actor.get_path_name():
            actor_subsystem.destroy_actor(actor)
            removed += 1
    return removed


skipped = []


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
    actor.tags = [unreal.Name(t) for t in ("Chapter1",) + tuple(tags)]
    spawned.append(actor)
    return actor


def spawn_mesh(path, location, rotation, scale, label, folder, tags=()):
    actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    actor.set_actor_scale3d(scale)
    return place(actor, label, folder, tags)


def spawn_post(x, y, height, label, folder):
    z = ground_z(x, y)
    scale_z = height / 240.0
    return spawn_mesh(BEAM, unreal.Vector(x, y, z + height * 0.5 - 20.0), unreal.Rotator(0, 0, 0),
                      unreal.Vector(1.0, 1.0, scale_z), label, folder)


def pennant_line(a, b, height, label, folder):
    """Row of small cloth pennants between two posts, sagging like a hung string."""
    za, zb = ground_z(*a) + height, ground_z(*b) + height
    length = math.dist(a, b)
    count = max(2, int(length / 45.0))
    yaw = yaw_to(a, b)
    materials = [unreal.load_asset(m) for m in PENNANT_MATERIALS]
    for i in range(1, count):
        t = i / count
        sag = 60.0 * 4.0 * t * (1.0 - t)
        loc = unreal.Vector(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, za + (zb - za) * t - sag)
        actor = spawn_mesh(PENNANT, loc, unreal.Rotator(0, 0, yaw), unreal.Vector(1.4, 1.4, 1.4),
                           "{}_{:02d}".format(label, i), folder)
        actor.static_mesh_component.set_material(0, materials[i % len(materials)])


def build_gate():
    half = 300.0
    left, right = (GATE[0] - half, GATE[1]), (GATE[0] + half, GATE[1])
    spawn_post(*left, 420.0, "Gate_PostL", "Gate")
    spawn_post(*right, 420.0, "Gate_PostR", "Gate")
    top_z = max(ground_z(*left), ground_z(*right)) + 400.0
    spawn_mesh(BEAM, unreal.Vector(GATE[0], GATE[1], top_z), unreal.Rotator(0, 90, 0),
               unreal.Vector(1.2, 1.2, (2 * half + 80.0) / 240.0), "Gate_Lintel", "Gate")
    for side, (x, y) in (("L", left), ("R", right)):
        flag = actor_subsystem.spawn_actor_from_class(unreal.load_asset(FLAG_BP).generated_class(),
                                                      unreal.Vector(x, y, top_z + 20.0), unreal.Rotator(0, 0, 90))
        place(flag, "Gate_Flag" + side, "Gate")
    pennant_line(left, right, 330.0, "Gate_Cloth", "Gate")


def ensure_sign_material():
    if not unreal.EditorAssetLibrary.does_asset_exist(SIGN_TEXTURE):
        task = unreal.AssetImportTask()
        task.filename = unreal.Paths.project_dir() + SIGN_SOURCE
        task.destination_path = ASSET_DIR
        task.automated = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if unreal.EditorAssetLibrary.does_asset_exist(SIGN_MATERIAL):
        return unreal.load_asset(SIGN_MATERIAL)
    mel = unreal.MaterialEditingLibrary
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        SIGN_MATERIAL.split("/")[-1], ASSET_DIR, unreal.Material, unreal.MaterialFactoryNew())
    sample = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, 0)
    sample.texture = unreal.load_asset(SIGN_TEXTURE)
    mel.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    rough.r = 0.85
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    material.set_editor_property("two_sided", True)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def ensure_outline_material():
    if unreal.EditorAssetLibrary.does_asset_exist(OUTLINE_MATERIAL):
        return unreal.load_asset(OUTLINE_MATERIAL)
    mel = unreal.MaterialEditingLibrary
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        OUTLINE_MATERIAL.split("/")[-1], ASSET_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    stencil = mel.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -900, 200)
    stencil.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    scene = mel.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -900, -150)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -650, -150)
    rgb.set_editor_property("r", True); rgb.set_editor_property("g", True); rgb.set_editor_property("b", True)
    mel.connect_material_expressions(scene, "Color", rgb, "")
    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -600, 150)
    custom.set_editor_property("code", OUTLINE_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for name in ("Dummy", "Width", "Stencil", "Fill"):
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(stencil, "Color", custom, "Dummy")
    for i, (name, value) in enumerate((("Width", 3.0), ("Stencil", float(OUTLINE_STENCIL)), ("Fill", 0.12))):
        param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 400 + i * 90)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", value)
        mel.connect_material_expressions(param, "", custom, name)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "OutlineColor")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.82, 0.3, 1.0))
    lerp = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -300, 0)
    mel.connect_material_expressions(rgb, "", lerp, "A")
    mel.connect_material_expressions(color, "", lerp, "B")
    mel.connect_material_expressions(custom, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_outline_volume():
    volume = actor_subsystem.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(SQUARE[0], SQUARE[1], 500.0))
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("weighted_blendables", unreal.WeightedBlendables(
        array=[unreal.WeightedBlendable(weight=1.0, object=ensure_outline_material())]))
    volume.set_editor_property("settings", settings)
    place(volume, "PP_InteractOutline", "FX")


def build_festival_sign():
    x, y = SQUARE[0], SQUARE[1] - 450.0
    for dx, label in ((-115.0, "Sign_PostL"), (115.0, "Sign_PostR")):
        spawn_post(x + dx, y, 270.0, label, "FestivalSquare")
    z = ground_z(x, y)
    # Plane rolled upright, then turned 180 so the texture reads left-to-right from the road (-Y).
    sign = spawn_mesh(PLANE, unreal.Vector(x, y - 14.0, z + 185.0), unreal.Rotator(90, 0, 180),
                      unreal.Vector(2.0, 1.0, 1.0), "Sign_FestivalCountdown", "FestivalSquare", ("FestivalSign",))
    sign.static_mesh_component.set_material(0, ensure_sign_material())
    # Cloth strips around the square
    posts = [(SQUARE[0] - 700, SQUARE[1] - 700), (SQUARE[0] + 700, SQUARE[1] - 700),
             (SQUARE[0] + 700, SQUARE[1] + 700), (SQUARE[0] - 700, SQUARE[1] + 700)]
    for i, p in enumerate(posts):
        spawn_post(p[0], p[1], 360.0, "Square_Post{}".format(i), "FestivalSquare")
    for i in range(4):
        pennant_line(posts[i], posts[(i + 1) % 4], 330.0, "Square_Cloth{}".format(i), "FestivalSquare")
    # Road cloth from gate to square
    road = [(GATE[0] - 300, GATE[1] + 600), (SQUARE[0] - 700, SQUARE[1] - 1400)]
    road_r = [(GATE[0] + 300, GATE[1] + 600), (SQUARE[0] + 700, SQUARE[1] - 1400)]
    for i, (a, b) in enumerate((road, road_r)):
        spawn_post(a[0], a[1], 320.0, "Road_PostA{}".format(i), "Road")
        spawn_post(b[0], b[1], 320.0, "Road_PostB{}".format(i), "Road")
        pennant_line(a, b, 290.0, "Road_Cloth{}".format(i), "Road")


def spawn_interactable(mesh, location, yaw, mesh_scale, mesh_z, label, folder, prompt, item_id, tags):
    """BP_VillageInteractable: its inherited SM_Pickup shows the mesh, E goes to the story subsystem."""
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(INTERACTABLE_BP).generated_class(), location,
                                                   unreal.Rotator(0, 0, yaw))
    actor.set_editor_property("ItemID", unreal.Name(item_id))  # no DT_ItemData row -> FallbackPrompt
    actor.set_editor_property("FallbackPrompt", unreal.Text(prompt))
    pickup = actor.get_component_by_class(unreal.StaticMeshComponent)
    pickup.set_static_mesh(unreal.load_asset(mesh))
    pickup.set_relative_location(unreal.Vector(0, 0, mesh_z), False, False)
    pickup.set_relative_scale3d(mesh_scale)
    for sphere in actor.get_components_by_class(unreal.SphereComponent):
        sphere.set_sphere_radius(180.0)
    return place(actor, label, folder, tags)


def build_silkworm_station():
    # Nong tằm: flat round trays on low stands beside the silkworm house. Traces start
    # at 400 cm so they find the ground under the eaves, not the roof.
    for i, (x, y) in enumerate(SILK_TRAYS):
        z = ground_z(x, y, top=400.0)
        spawn_mesh(CYLINDER, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0), unreal.Vector(0.25, 0.25, 0.6),
                   "SilkTray_Stand{}".format(i), "SilkwormHouse")
        spawn_interactable(CYLINDER, unreal.Vector(x, y, z), 0.0, unreal.Vector(1.1, 1.1, 0.06), 60.0,
                           "SilkTray_{}".format(i), "SilkwormHouse", "Chăm sóc nong tằm", "SilkTray",
                           ("Interact_SilkwormTray",))
    # Vườn dâu: 3 x 3 mulberry bushes, each tagged as a leaf source.
    n = 0
    for row in range(3):
        for col in range(3):
            x = GARDEN[0] - 220 + row * 220
            y = GARDEN[1] - 220 + col * 220
            label = "Mulberry_{:02d}".format(n)
            n += 1
            if not is_open_ground(x, y, label):
                continue
            spawn_interactable(MULBERRY, unreal.Vector(x, y, ground_z(x, y)), (row * 37 + col * 71) % 360,
                               unreal.Vector(1.0, 1.0, 1.0), 0.0, label, "MulberryGarden", "Hái lá dâu", "Mulberry",
                               ("Interact_MulberryBush",))


def build_workshop_dressing():
    # Dye vats on the bank side (+X) of the dyer.
    for i, (dx, dy) in enumerate(((180, 120), (230, -60), (150, -220))):
        x, y = DYE_YARD[0] + dx, DYE_YARD[1] + dy
        if is_open_ground(x, y, "DyeVat_{}".format(i)):
            spawn_mesh(BARREL, unreal.Vector(x, y, ground_z(x, y)), unreal.Rotator(0, 0, i * 40),
                       unreal.Vector(1.6, 1.6, 1.4), "DyeVat_{}".format(i), "DyeYard")
    # Drying line for dyed / woven cloth, between the two workshop houses.
    a, b = (WEAVE_YARD[0] + 250, WEAVE_YARD[1] - 350), (WEAVE_YARD[0] + 250, WEAVE_YARD[1] + 350)
    if is_open_ground(*a, "Weave_PostA") and is_open_ground(*b, "Weave_PostB"):
        spawn_post(a[0], a[1], 260.0, "Weave_PostA", "WeaveYard")
        spawn_post(b[0], b[1], 260.0, "Weave_PostB", "WeaveYard")
        pennant_line(a, b, 230.0, "Weave_Cloth", "WeaveYard")


# Copies of houses already in Mountain, placed on flat spots found by trace scans:
# (source label, source group label or None for a single actor, new pivot XY, yaw delta, base Z
#  offset above ground, label prefix). Base offsets are measured from the originals.
HOUSE_COPIES = [
    # Small wooden house (GroupActor_4, 67 meshes, 18 x 9 m) on the rock plateau above the gate.
    ("GroupActor_4", "house_01", (39750.0, -43000.0), 90.0, -20.0, "House_Plateau"),
    # Second silkworm-style hall on the riverbank slab: the Silk Reeling Workshop unlocked after Day 1.
    ("Meshy_AI_Rustic_Clay_Shelter_1005072823_texture", None, (43500.0, -40500.0), -93.0, 160.0, "Workshop_Reeling"),
    # East workshop quarter (spruce cleared by clear_east_area.py): cô Mai by the stream, cô Lụa's workshop.
    ("GroupActor_4", "house_01", (43400.0, -36600.0), 90.0, -20.0, "House_MaiDyer"),
    ("midpoly_town_house_01", None, (42350.0, -38300.0), 0.0, 0.0, "Workshop_LuaWeaver"),
]


def copy_house(source_label, member_folder, pivot_xy, yaw_delta, base_offset, prefix):
    actors = actor_subsystem.get_all_level_actors()
    source = next(a for a in actors if a.get_actor_label() == source_label)
    if member_folder:
        origin, extent = source.get_actor_bounds(False)
        members = [a for a in actors if isinstance(a, unreal.StaticMeshActor)
                   and str(a.get_folder_path()) == member_folder
                   and abs(a.get_actor_location().x - origin.x) <= extent.x + 50
                   and abs(a.get_actor_location().y - origin.y) <= extent.y + 50]
        bottom = min(m.get_actor_bounds(False)[0].z - m.get_actor_bounds(False)[1].z for m in members)
        new_z = ground_z(*pivot_xy) + base_offset + (source.get_actor_location().z - bottom)
    else:
        members = [source]
        new_z = ground_z(*pivot_xy) + base_offset
    old_pivot = unreal.Transform(source.get_actor_location(), unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1))
    new_pivot = unreal.Transform(unreal.Vector(pivot_xy[0], pivot_xy[1], new_z), unreal.Rotator(0, 0, yaw_delta),
                                 unreal.Vector(1, 1, 1))
    to_new = unreal.MathLibrary.compose_transforms(unreal.MathLibrary.invert_transform(old_pivot), new_pivot)
    for i, member in enumerate(members):
        src = member.static_mesh_component
        world_t = unreal.MathLibrary.compose_transforms(member.get_actor_transform(), to_new)
        copy = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, world_t.translation, world_t.rotation.rotator())
        copy.set_actor_scale3d(world_t.scale3d)
        dst = copy.static_mesh_component
        dst.set_static_mesh(src.static_mesh)
        for slot, material in enumerate(src.get_editor_property("override_materials")):
            if material:
                dst.set_material(slot, material)
        dst.set_collision_profile_name(src.get_collision_profile_name())
        place(copy, "{}_{:02d}".format(prefix, i), "Houses", (prefix,))
    return len(members)


def spawn_npc(npc_id, mesh_key, role, pos, look_at, interactable):
    mesh = unreal.load_asset(NPC_MESHES[mesh_key])
    bounds = mesh.get_bounds()
    feet_offset = bounds.origin.z - bounds.box_extent.z
    z = ground_z(*pos, top=800.0) - feet_offset  # below tree canopies over the meadow
    idle = unreal.load_asset("{}/AS_NPC_Idle_{}".format(NPC_ANIM_DIR, mesh_key))
    if interactable:
        # BP_VillageNPC: idle by default, E turns it toward the player and plays talk.
        actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(NPC_BP).generated_class(),
                                                       unreal.Vector(pos[0], pos[1], z),
                                                       unreal.Rotator(0, 0, yaw_to(pos, look_at)))
        actor.set_editor_property("ItemID", unreal.Name(npc_id))  # no DT_ItemData row -> FallbackPrompt
        actor.set_editor_property("FallbackPrompt", unreal.Text(NPC_PROMPTS[npc_id]))
        actor.set_editor_property("DisplayName", unreal.Text(NPC_NAMEPLATES[npc_id][0]))
        actor.set_editor_property("DisplayRole", unreal.Text(NPC_NAMEPLATES[npc_id][1]))
        actor.set_editor_property("IdleAnimation", idle)
        actor.set_editor_property("TalkAnimation", unreal.load_asset("{}/AS_NPC_Talk_{}".format(NPC_ANIM_DIR, mesh_key)))
        for sm in actor.get_components_by_class(unreal.StaticMeshComponent):  # inherited pickup mesh
            sm.set_visibility(False)
            sm.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        for sphere in actor.get_components_by_class(unreal.SphereComponent):
            sphere.set_sphere_radius(250.0)
    else:
        actor = actor_subsystem.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(pos[0], pos[1], z),
                                                       unreal.Rotator(0, 0, yaw_to(pos, look_at) - NPC_FORWARD_YAW))
    comp = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    comp.set_skeletal_mesh_asset(mesh)
    comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    comp.set_editor_property("animation_data", unreal.SingleAnimationPlayData(anim_to_play=idle, saved_looping=True,
                                                                               saved_playing=True))
    comp.set_update_animation_in_editor(True)
    tags = ["NPC", npc_id, "Role_" + role]
    if interactable:
        tags.append("Interactable")
    place(actor, npc_id, "NPC" if interactable else "Villagers", tags)
    # NPCs are walk-through: only the inherited InteractionRange overlap stays. Blocking collision
    # lifted the player onto NPCs (limb bodies and capsule tops acted as stairs/floor).
    comp.set_collision_profile_name("Trigger")
    if interactable:
        for prim in actor.get_components_by_class(unreal.PrimitiveComponent):
            if prim.get_name() == "BodyBlock":
                prim.set_collision_profile_name("Trigger")
    unwalkable = unreal.WalkableSlopeOverride()
    unwalkable.set_editor_property("walkable_slope_behavior", unreal.WalkableSlopeBehavior.WALKABLE_SLOPE_UNWALKABLE)
    for prim in actor.get_components_by_class(unreal.PrimitiveComponent):
        prim.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
        prim.set_walkable_slope_override(unwalkable)
    return actor


def main():
    level = ensure_sublevel()
    level_subsystem.set_current_level_by_name(SUBLEVEL_PATH.split("/")[-1])
    removed = clear_previous_build()
    build_gate()
    build_festival_sign()
    build_silkworm_station()
    build_workshop_dressing()
    build_outline_volume()
    for spec in HOUSE_COPIES:
        print("HOUSE_COPY {} meshes={}".format(spec[-1], copy_house(*spec)))
    npcs = [spawn_npc(*spec) for spec in NPCS]
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    level_subsystem.set_current_level_by_name("PersistentLevel")
    count = sum(1 for a in actor_subsystem.get_all_level_actors() if "Chapter1" in [str(t) for t in a.tags])
    print("CHAPTER1_BUILD removed={} placed={} npcs={}".format(removed, count, len(npcs)))
    for a in npcs:
        l = a.get_actor_location()
        print("NPC {} at X={:.0f} Y={:.0f} Z={:.0f} yaw={:.0f}".format(a.get_actor_label(), l.x, l.y, l.z, a.get_actor_rotation().yaw))
    for line in skipped:
        print("SKIPPED", line)


main()
