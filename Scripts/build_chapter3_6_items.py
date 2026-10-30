import json

import unreal


# Adds the Chapter 3-6 inventory items (from CỐT TRUYỆN/ITEM SYSTEM) to DT_ItemData and points every item
# that has a Meshy model at its static mesh. Workstation props (loom, dye pot, dress form...) are level
# props, not inventory items, so they get no row. The mesh goes into S_ItemData's "ItemMesh" field
# (Soft Object Reference to Static Mesh), which UShopEditorTools::AddStructSoftObjectField adds when missing.
# Safe to re-run: missing rows are added, meshes are always re-applied.
ITEM_TABLE = "/Game/SilkVillage/Data/DT_ItemData"
ITEM_STRUCT = "/Game/SilkVillage/Data/S_ItemData.S_ItemData"
MESH_ROOT = "/Game/SilkVillage/Meshes"
ICON_ROOT = "/Game/SilkVillage/UI/Icons/Items"


def item(item_id, name, description, item_type, rarity, price, icon=None):
    return {
        "Name": item_id,
        "ItemID": item_id,
        "ItemName": name,
        "Description ": description,
        "ItemType": item_type,
        "Rarity": rarity,
        "Price": price,
        "ItemIcon": "{0}/{1}.{1}".format(ICON_ROOT, icon) if icon else "None",
    }


NEW_ITEMS = [
    # Chapter 3 – The First Silk
    item("MAT_RAW_SILK_001", "Tơ thô", "Tơ kéo từ kén tại xưởng ươm tơ. Nguyên liệu cho công đoạn se sợi.", "Material", "Common", 12),
    # Chapter 4 – The Colors of the Village
    item("MAT_PURPLE_BERRY_001", "Quả tím", "Quả rừng màu tím, mọc nhiều hơn sau mưa. Nguyên liệu tạo thuốc nhuộm tím.", "Material", "Common", 3),
    item("MAT_TREE_BARK_001", "Vỏ cây", "Vỏ của một số loài cây đặc biệt. Dùng tạo thuốc nhuộm nâu và chế tạo.", "Material", "Common", 2),
    item("MAT_RED_THREAD_001", "Sợi tơ đỏ", "Sợi tơ đã nhuộm đỏ, dùng để dệt vải đỏ.", "Material", "Uncommon", 18),
    item("TOOL_WATER_GOURD_001", "Bình chứa nước", "Bình hồ lô dùng lấy nước suối cho việc nhuộm và chăm tằm.", "Tool", "Common", 20),
    item("TOOL_WEAVING_SHUTTLE_001", "Thoi dệt", "Thoi gỗ đưa sợi qua khung dệt.", "Tool", "Common", 25, "T_Item_WeavingShuttle_png"),
    # Chapter 5 – Your Own Outfit
    item("TOOL_SCISSORS_001", "Kéo may", "Kéo sắt của nhà may, dùng cắt vải theo mẫu.", "Tool", "Common", 30),
    item("ACC_SILK_SCARF_001", "Khăn lụa", "Khăn lụa thêu hoa văn truyền thống. Festival Design +3%.", "Accessory", "Uncommon", 35),
    item("ACC_BELT_001", "Thắt lưng truyền thống", "Thắt lưng vải dệt có tua. Festival Design +5%.", "Accessory", "Uncommon", 40),
    item("ACC_BRACELET_001", "Vòng tay dệt", "Vòng tay bện từ sợi tơ nhiều màu. Festival Design +2%.", "Accessory", "Common", 15),
    # Chapter 6 – The Silk Festival
    item("MAT_GOLDEN_COCOON_001", "Kén vàng", "Chiếc kén hiếm từ nhiệm vụ The Last Cocoon. Tạo ra Golden Silk.", "Material", "Legendary", 0),
    item("TOOL_MASTER_SCISSORS_001", "Kéo bậc thầy", "Phần thưởng Master's Test. Tailoring Error -15%.", "Tool", "Epic", 0),
    item("PAT_ANCIENT_FRAGMENT_001", "Mảnh hoa văn cổ", "Tìm thấy trong căn nhà cũ. Ghép lại thành Ancient Pattern.", "Pattern", "Rare", 0),
    item("ACC_VILLAGE_SEAL_001", "Con dấu làng", "Trao cho người hoàn thành nhiều Story Hidden Quest. Mở đặc quyền tại lễ hội.", "Accessory", "Legendary", 0),
]

# DT_ItemData row -> (chapter folder, Meshy model id). Rows that already existed reuse the matching model.
ITEM_MESHES = {
    "DEC_BAMBOO_001": ("Chapter2", "DEC_BAMBOO_001"),
    "DEC_COLOR_CLOTH_001": ("Chapter2", "DEC_COLOR_CLOTH_001"),
    "DEC_FLOWER_001": ("Chapter2", "DEC_FLOWER_001"),
    "DEC_WOOD_ORNAMENT_001": ("Chapter2", "DEC_WOOD_ORNAMENT_001"),
    "ITM_SILK_BANNER_001": ("Chapter2", "ITM_SILK_BANNER_001"),
    "MAT_COCOON_001": ("Chapter3", "ITM_COCOON_001"),
    "MAT_SILK_THREAD_001": ("Chapter3", "ITM_SILK_THREAD_001"),
    "MAT_RAW_SILK_001": ("Chapter3", "ITM_RAW_SILK_001"),
    "MAT_PURPLE_BERRY_001": ("Chapter4", "ITM_PURPLE_BERRY_001"),
    "MAT_TREE_BARK_001": ("Chapter4", "ITM_TREE_BARK_001"),
    "MAT_RED_THREAD_001": ("Chapter4", "ITM_RED_THREAD_001"),
    "FAB_SILK_001": ("Chapter4", "ITM_SILK_CLOTH_001"),
    "TOOL_WATER_GOURD_001": ("Chapter4", "TOOL_WATER_GOURD_001"),
    "TOOL_WEAVING_SHUTTLE_001": ("Chapter4", "TOOL_WEAVING_SHUTTLE_001"),
    "TOOL_NEEDLE_001": ("Chapter5", "TOOL_NEEDLE_THREAD_001"),
    "TOOL_SCISSORS_001": ("Chapter5", "TOOL_SCISSORS_001"),
    "ACC_SILK_SCARF_001": ("Chapter5", "ACC_SILK_SCARF_001"),
    "ACC_BELT_001": ("Chapter5", "ACC_BELT_001"),
    "ACC_BRACELET_001": ("Chapter5", "ACC_BRACELET_001"),
    "MAT_GOLDEN_COCOON_001": ("Chapter6", "ITM_GOLDEN_COCOON_001"),
    "TOOL_MASTER_SCISSORS_001": ("Chapter6", "TOOL_MASTER_SCISSORS_001"),
    "PAT_ANCIENT_FRAGMENT_001": ("Chapter6", "ITM_PATTERN_FRAGMENT_001"),
    "ACC_VILLAGE_SEAL_001": ("Chapter6", "ITM_VILLAGE_SEAL_001"),
    # DEC_ROPE_001 keeps the project's existing rope coil mesh.
    "DEC_ROPE_001": None,
}
ROPE_MESH = "/Game/MS/3D/Rope_Coil_vcsoddfs/SM_Rope_Coil_vcsoddfs_00_N1.SM_Rope_Coil_vcsoddfs_00_N1"

eal = unreal.EditorAssetLibrary
dtl = unreal.DataTableFunctionLibrary


def mesh_path(row_name):
    source = ITEM_MESHES[row_name]
    if source is None:
        return ROPE_MESH
    chapter, model_id = source
    path = "{0}/{1}/{2}/SM_{2}".format(MESH_ROOT, chapter, model_id)
    if not eal.does_asset_exist(path):
        raise RuntimeError("Missing mesh {}; run import_meshy_items.py first".format(path))
    return "{0}.SM_{1}".format(path, model_id)


def main():
    if hasattr(unreal, "ShopEditorTools"):
        status = unreal.ShopEditorTools.add_struct_soft_object_field(ITEM_STRUCT, "ItemMesh", "/Script/Engine.StaticMesh")
        print("ITEM_MESH_FIELD", status)
        if status.startswith("ERROR"):
            raise RuntimeError(status)
        if status == "ADDED":
            eal.save_loaded_asset(unreal.load_asset(ITEM_STRUCT), only_if_is_dirty=False)

    table = unreal.load_asset(ITEM_TABLE)
    rows = json.loads(dtl.export_data_table_to_json_string(table))
    have = {r["Name"] for r in rows}
    added = [r for r in NEW_ITEMS if r["Name"] not in have]
    rows += added

    mesh_key = next((k for k in rows[0] if k.strip() == "ItemMesh"), None)
    if mesh_key:
        for r in rows:
            if r["Name"] in ITEM_MESHES:
                r[mesh_key] = mesh_path(r["Name"])
    else:
        print("MESH_FIELD_MISSING: add an 'ItemMesh' (Static Mesh, Soft Object Reference) field to S_ItemData")

    if not dtl.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)):
        raise RuntimeError("Could not update " + ITEM_TABLE)
    eal.save_loaded_asset(table)

    result = json.loads(dtl.export_data_table_to_json_string(table))
    print("ADDED_ROWS", [r["Name"] for r in added])
    print("ITEM_ROWS", len(result))
    if mesh_key:
        print("ROWS_WITH_MESH", sum(1 for r in result if r.get(mesh_key) not in (None, "", "None")))


main()
