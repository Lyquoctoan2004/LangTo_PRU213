import json
import os
import struct
import sys

import unreal


# Imports every Meshy-generated model (MeshyModels/<Chapter>/<ItemID>/<ItemID>.glb) into
# /Game/SilkVillage/Meshes/<Chapter>/<ItemID>/ and renames its static mesh to SM_<ItemID>.
# Meshy exports everything ~190 cm long, so each GLB is first copied to Intermediate/MeshyImport with its
# root node scaled to the real-world size below (LOD build scale is ignored for Interchange glTF meshes).
# Pass chapter names (e.g. Chapter3) to import only those; with no arguments every chapter is imported.
# DEC_ROPE_001 is skipped on purpose: it uses the existing SM_Rope_Coil_vcsoddfs_00_N1 mesh.
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCE_ROOT = os.path.join(PROJECT_ROOT, "MeshyModels")
SCALED_ROOT = os.path.join(PROJECT_ROOT, "Intermediate", "MeshyImport")
DEST_ROOT = "/Game/SilkVillage/Meshes"
# Longest side in cm. Items not listed fall back to DEFAULT_SIZE by prefix.
SIZES = {
    "DEC_BAMBOO_001": 60, "DEC_COLOR_CLOTH_001": 35, "DEC_FLOWER_001": 35,
    "DEC_WOOD_ORNAMENT_001": 35, "ITM_SILK_BANNER_001": 60,
    "ITM_COCOON_001": 12, "ITM_RAW_SILK_001": 30, "ITM_SILK_THREAD_001": 15,
    "PROP_REELING_POT_001": 90, "PROP_SPINNING_WHEEL_001": 120,
    "ITM_PURPLE_BERRY_001": 15, "ITM_TREE_BARK_001": 25, "TOOL_WATER_GOURD_001": 35, "ITM_RED_THREAD_001": 15,
    "PROP_DYE_POT_001": 110, "PROP_DRYING_RACK_001": 200, "PROP_LOOM_001": 220,
    "TOOL_WEAVING_SHUTTLE_001": 30, "ITM_SILK_CLOTH_001": 60,
    "TOOL_SCISSORS_001": 20, "TOOL_NEEDLE_THREAD_001": 15, "ACC_SILK_SCARF_001": 30,
    "ACC_BELT_001": 35, "ACC_BRACELET_001": 8, "PROP_DRESS_FORM_001": 170,
    "ITM_PATTERN_FRAGMENT_001": 25, "ITM_VILLAGE_SEAL_001": 12,
    "ITM_GOLDEN_COCOON_001": 12, "TOOL_MASTER_SCISSORS_001": 20,
}
SKIP = {"DEC_ROPE_001"}
DEFAULT_SIZE = {"PROP": 150}
DEFAULT_ITEM_SIZE = 30

eal = unreal.EditorAssetLibrary


def size_for(item_id):
    return SIZES.get(item_id, DEFAULT_SIZE.get(item_id.split("_")[0], DEFAULT_ITEM_SIZE))


def write_scaled_glb(source_file, dest_file, longest_cm):
    with open(source_file, "rb") as f:
        data = f.read()
    json_len = struct.unpack_from("<I", data, 12)[0]
    gltf = json.loads(data[20:20 + json_len])
    rest = data[20 + json_len:]

    lo, hi = [float("inf")] * 3, [float("-inf")] * 3
    for mesh in gltf["meshes"]:
        for prim in mesh["primitives"]:
            acc = gltf["accessors"][prim["attributes"]["POSITION"]]
            lo = [min(a, b) for a, b in zip(lo, acc["min"])]
            hi = [max(a, b) for a, b in zip(hi, acc["max"])]
    scale = (longest_cm / 100.0) / max(h - l for l, h in zip(lo, hi))

    for scene in gltf["scenes"]:
        for index in scene["nodes"]:
            node = gltf["nodes"][index]
            if "matrix" in node:
                # Column-major 4x4; scale the three basis columns, keep the translation.
                node["matrix"] = [v * scale if i < 12 else v for i, v in enumerate(node["matrix"])]
            else:
                node["scale"] = [s * scale for s in node.get("scale", [1.0, 1.0, 1.0])]

    chunk = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    chunk += b" " * (-len(chunk) % 4)
    body = struct.pack("<II", len(chunk), 0x4E4F534A) + chunk + rest
    os.makedirs(os.path.dirname(dest_file), exist_ok=True)
    with open(dest_file, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


chapters = sys.argv[1:] or sorted(d for d in os.listdir(SOURCE_ROOT) if os.path.isdir(os.path.join(SOURCE_ROOT, d)))
items = []
for chapter in chapters:
    for item_id in sorted(os.listdir(os.path.join(SOURCE_ROOT, chapter))):
        source_file = os.path.join(SOURCE_ROOT, chapter, item_id, item_id + ".glb")
        if item_id not in SKIP and os.path.isfile(source_file):
            items.append((chapter, item_id, source_file))

tasks = []
for chapter, item_id, source_file in items:
    scaled_file = os.path.join(SCALED_ROOT, item_id + ".glb")
    write_scaled_glb(source_file, scaled_file, size_for(item_id))
    # Interchange nests a fresh <ItemID>/ subfolder on re-import, so start each item from an empty folder.
    folder = "{}/{}/{}".format(DEST_ROOT, chapter, item_id)
    if eal.does_directory_exist(folder):
        eal.delete_directory(folder)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", scaled_file)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

imported = []
for chapter, item_id, _ in items:
    folder = "{}/{}/{}".format(DEST_ROOT, chapter, item_id)
    target = "{}/SM_{}".format(folder, item_id)
    meshes = [p for p in eal.list_assets(folder, recursive=True, include_folder=False)
              if isinstance(eal.load_asset(p), unreal.StaticMesh)]
    if not meshes:
        raise RuntimeError("Unreal did not create a static mesh in " + folder)

    mesh_path = meshes[0].split(".")[0]
    if mesh_path != target and not eal.rename_asset(mesh_path, target):
        raise RuntimeError("Could not rename {} to {}".format(mesh_path, target))
    eal.save_directory(folder, only_if_is_dirty=False, recursive=True)
    box = eal.load_asset(target).get_bounding_box()
    size = box.max - box.min
    imported.append("{} {:.0f}x{:.0f}x{:.0f}cm".format(target, size.x, size.y, size.z))

print("IMPORTED_ITEM_MESH_COUNT=%d" % len(imported))
for line in imported:
    print(line)
