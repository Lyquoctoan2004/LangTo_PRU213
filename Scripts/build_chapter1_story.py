import json

import unreal


# Loads the Chapter 1 script (SourceArt/Chapter1/Story/*.json) into the DataTables read by
# UVillageStorySubsystem, and adds the mulberry leaf item. Re-run after editing the JSON.
STORY_DIR = "/Game/SilkVillage/Story"
SOURCE_DIR = unreal.Paths.project_dir() + "SourceArt/Chapter1/Story/"
TABLES = [
    ("DT_VillageDialogue", "/Script/MutableSample.VillageDialogueRow", "dialogue.json"),
    ("DT_VillageTalkRules", "/Script/MutableSample.VillageTalkRuleRow", "talk_rules.json"),
    ("DT_VillageQuests", "/Script/MutableSample.VillageQuestRow", "quests.json"),
]
ITEM_TABLE = "/Game/SilkVillage/Data/DT_ItemData"
LEAF_ROW = {
    "Name": "MAT_MULBERRY_LEAF_001",
    "ItemID": "MAT_MULBERRY_LEAF_001",
    "ItemName": "Lá dâu",
    "Description ": "Lá dâu tươi hái ở vườn dâu, thức ăn của tằm.",
    "ItemType": "Material",
    "Rarity": "Common",
    "Price": 2,
    "ItemIcon": "/Script/Engine.Texture2D'/Game/SilkVillage/UI/Icons/Items/T_Item_MulberryLeaf_png.T_Item_MulberryLeaf_png'",
}

eal = unreal.EditorAssetLibrary
dtl = unreal.DataTableFunctionLibrary


def ensure_table(name, struct_path):
    path = "{}/{}".format(STORY_DIR, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataTableFactory()
    factory.struct = unreal.load_object(None, struct_path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, STORY_DIR, unreal.DataTable, factory)


def main():
    for name, struct_path, source in TABLES:
        table = ensure_table(name, struct_path)
        with open(SOURCE_DIR + source, encoding="utf-8") as f:
            rows = json.load(f)
        if not dtl.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)):
            raise RuntimeError("Could not fill " + name)
        eal.save_loaded_asset(table)
        print("STORY_TABLE {} rows={}".format(name, len(dtl.get_data_table_row_names(table))))

    items = unreal.load_asset(ITEM_TABLE)
    current = json.loads(dtl.export_data_table_to_json_string(items))
    if not any(r["Name"] == LEAF_ROW["Name"] for r in current):
        current.append(LEAF_ROW)
        if not dtl.fill_data_table_from_json_string(items, json.dumps(current, ensure_ascii=False)):
            raise RuntimeError("Could not add mulberry leaf item")
        eal.save_loaded_asset(items)
    print("ITEM_ROWS", [str(n) for n in dtl.get_data_table_row_names(items)])


main()
