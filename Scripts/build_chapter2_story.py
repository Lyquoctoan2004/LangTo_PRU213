import json

import unreal


# Loads the Chapter 2 script (SourceArt/Chapter2/Story/*.json) on top of Chapter 1 into the DataTables
# read by UVillageStorySubsystem, and adds the festival decoration items. Chapter 2 talk rules go first
# so they win over Chapter 1's "Day1.Complete" fallback. Safe to re-run; it always rebuilds from the JSON.
# From Chapter 3 on use build_chapter3_story.py instead (it loads Chapters 1-3; this script would drop Chapter 3's rows).
STORY_DIR = "/Game/SilkVillage/Story"
SOURCE_ROOT = unreal.Paths.project_dir() + "SourceArt/"
TABLES = [
    # (asset, struct, json, chapter 2 rows go first)
    ("DT_VillageDialogue", "/Script/MutableSample.VillageDialogueRow", "dialogue.json", False),
    ("DT_VillageTalkRules", "/Script/MutableSample.VillageTalkRuleRow", "talk_rules.json", True),
    ("DT_VillageQuests", "/Script/MutableSample.VillageQuestRow", "quests.json", False),
]
ITEM_TABLE = "/Game/SilkVillage/Data/DT_ItemData"


def item(item_id, name, description, price):
    return {
        "Name": item_id,
        "ItemID": item_id,
        "ItemName": name,
        "Description ": description,
        "ItemType": "Material",
        "Rarity": "Common",
        "Price": price,
    }


NEW_ITEMS = [
    item("DEC_BAMBOO_001", "Tre", "Cây tre non dùng dựng khung cổng và giàn treo lễ hội.", 3),
    item("DEC_COLOR_CLOTH_001", "Vải màu", "Dải vải nhiều màu để căng lên cổng và dọc lối đi.", 4),
    item("DEC_FLOWER_001", "Hoa", "Hoa rừng hái quanh làng để cài lên cổng và giỏ hoa.", 1),
    item("DEC_ROPE_001", "Dây thừng", "Dây thừng bện chắc, dùng treo đèn và dải vải.", 2),
    item("DEC_WOOD_ORNAMENT_001", "Đồ gỗ trang trí", "Đồ gỗ chạm khắc nhỏ dùng trang trí sân lễ hội.", 5),
    item("ITM_SILK_BANNER_001", "Cờ lụa truyền thống", "Cờ lụa do dân làng tặng. Dùng trang trí thêm khu lễ hội ở Chương 6.", 0),
]

eal = unreal.EditorAssetLibrary
dtl = unreal.DataTableFunctionLibrary


def load_rows(chapter, source):
    with open("{}{}/Story/{}".format(SOURCE_ROOT, chapter, source), encoding="utf-8") as f:
        return json.load(f)


def main():
    for name, _struct_path, source, chapter2_first in TABLES:
        table = unreal.load_asset("{}/{}".format(STORY_DIR, name))
        if not table:
            raise RuntimeError("{} is missing; run build_chapter1_story.py first".format(name))
        ch1 = load_rows("Chapter1", source)
        ch2 = load_rows("Chapter2", source)
        rows = ch2 + ch1 if chapter2_first else ch1 + ch2
        names = [r["Name"] for r in rows]
        dupes = sorted({n for n in names if names.count(n) > 1})
        if dupes:
            raise RuntimeError("Duplicate row names in {}: {}".format(name, dupes))
        if not dtl.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)):
            raise RuntimeError("Could not fill " + name)
        eal.save_loaded_asset(table)
        print("STORY_TABLE {} rows={}".format(name, len(dtl.get_data_table_row_names(table))))

    items = unreal.load_asset(ITEM_TABLE)
    current = json.loads(dtl.export_data_table_to_json_string(items))
    have = {r["Name"] for r in current}
    added = [r for r in NEW_ITEMS if r["Name"] not in have]
    if added:
        if not dtl.fill_data_table_from_json_string(items, json.dumps(current + added, ensure_ascii=False)):
            raise RuntimeError("Could not add decoration items")
        eal.save_loaded_asset(items)
    print("ITEM_ROWS", [str(n) for n in dtl.get_data_table_row_names(items)])


main()
