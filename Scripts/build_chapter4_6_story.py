import json

import unreal


# Loads the story of Chapters 1-6 (SourceArt/ChapterN/Story/*.json) into the DataTables read by
# UVillageStorySubsystem: dialogue, talk rules (newest chapter first, so later chapters win over earlier
# fallbacks), quests, and the Chapter 4-6 tables DT_VillageCrafts (timing mini-games) and DT_VillageDays
# (end-of-day cards). Also adds the new inventory items. Safe to re-run; it always rebuilds from the JSON.
# This supersedes build_chapter3_story.py. Run it after the C++ build (new row structs FVillageCraftRow / FVillageDayRow).
STORY_DIR = "/Game/SilkVillage/Story"
SOURCE_ROOT = unreal.Paths.project_dir() + "SourceArt/"
CHAPTERS = ["Chapter1", "Chapter2", "Chapter3", "Chapter4", "Chapter5", "Chapter6"]
TABLES = [
    # (asset, struct, json file, newest chapter's rows go first)
    ("DT_VillageDialogue", "/Script/MutableSample.VillageDialogueRow", "dialogue.json", False),
    ("DT_VillageTalkRules", "/Script/MutableSample.VillageTalkRuleRow", "talk_rules.json", True),
    ("DT_VillageQuests", "/Script/MutableSample.VillageQuestRow", "quests.json", False),
    ("DT_VillageCrafts", "/Script/MutableSample.VillageCraftRow", "crafts.json", False),
    ("DT_VillageDays", "/Script/MutableSample.VillageDayRow", "days.json", False),
]
ITEM_TABLE = "/Game/SilkVillage/Data/DT_ItemData"


def item(item_id, name, description, item_type="Material", rarity="Common", price=0):
    return {
        "Name": item_id,
        "ItemID": item_id,
        "ItemName": name,
        "Description ": description,
        "ItemType": item_type,
        "Rarity": rarity,
        "Price": price,
    }


# Rows that may be missing; existing rows are never touched.
NEW_ITEMS = [
    item("MAT_RAW_SILK_001", "Tơ thô", "Tơ kéo từ kén tại xưởng ươm tơ. Nguyên liệu cho công đoạn se sợi.", price=12),
    item("MAT_SILK_THREAD_001", "Sợi tơ", "Sợi tơ đã se, đủ chắc để dệt thành vải.", price=15),
    item("DYE_MATERIAL_001", "Nguyên liệu nhuộm", "Lá, vỏ cây và đất màu hái quanh làng. Ba phần cùng một phần nước thì nhuộm được một màu.", price=2),
    item("DYE_WATER_001", "Nước suối", "Nước suối đầu nguồn, trong và mát. Dùng pha thuốc nhuộm.", price=1),
    item("MAT_COLORED_THREAD_001", "Sợi tơ nhuộm màu", "Sợi tơ đã nhuộm màu, sẵn sàng lên khung dệt.", rarity="Uncommon", price=22),
    item("FAB_SILK_001", "Vải lụa", "Tấm vải lụa dệt từ sợi tơ nhuộm màu.", rarity="Uncommon", price=45),
    item("OUTFIT_FIRST_001", "Trang phục đầu tiên", "Bộ trang phục đầu tiên do chính bạn thiết kế, cắt và may.", item_type="Outfit", rarity="Rare", price=0),
    item("PAT_ANCIENT_FRAGMENT_001", "Mảnh hoa văn cổ", "Tìm thấy trong căn nhà cũ. Ghép lại thành Ancient Pattern.", item_type="Pattern", rarity="Rare", price=0),
]

eal = unreal.EditorAssetLibrary
dtl = unreal.DataTableFunctionLibrary


def ensure_table(name, struct_path):
    path = "{}/{}".format(STORY_DIR, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    struct = unreal.load_object(None, struct_path)
    if not struct:
        raise RuntimeError("Row struct {} not found; build the C++ first".format(struct_path))
    factory = unreal.DataTableFactory()
    factory.struct = struct
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, STORY_DIR, unreal.DataTable, factory)


def load_rows(chapter, source):
    try:
        with open("{}{}/Story/{}".format(SOURCE_ROOT, chapter, source), encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return []


def main():
    for name, struct_path, source, newest_first in TABLES:
        table = ensure_table(name, struct_path)
        per_chapter = [load_rows(chapter, source) for chapter in CHAPTERS]
        if newest_first:
            per_chapter.reverse()
        rows = [row for chapter_rows in per_chapter for row in chapter_rows]
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
            raise RuntimeError("Could not add items")
        eal.save_loaded_asset(items)
    print("ITEM_ADDED", [r["Name"] for r in added])


main()
