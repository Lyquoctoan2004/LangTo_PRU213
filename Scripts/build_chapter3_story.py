import json

import unreal


# Loads the Chapter 3 script (SourceArt/Chapter3/Story/*.json) on top of Chapters 1-2 into the DataTables
# read by UVillageStorySubsystem, and makes sure the silk items exist. Later chapters' talk rules go first so
# they win over earlier chapters' fallbacks. Safe to re-run; it always rebuilds from the JSON.
# Run the C++ build first (new story actions: EndDay:n, StartQuestIf, ResetObjective).
STORY_DIR = "/Game/SilkVillage/Story"
SOURCE_ROOT = unreal.Paths.project_dir() + "SourceArt/"
CHAPTERS = ["Chapter1", "Chapter2", "Chapter3"]
TABLES = [
    # (asset, json, newest chapter's rows go first)
    ("DT_VillageDialogue", "dialogue.json", False),
    ("DT_VillageTalkRules", "talk_rules.json", True),
    ("DT_VillageQuests", "quests.json", False),
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


# Normally added by build_chapter3_6_items.py; kept here so Chapter 3 never runs without them.
SILK_ITEMS = [
    item("MAT_RAW_SILK_001", "Tơ thô", "Tơ kéo từ kén tại xưởng ươm tơ. Nguyên liệu cho công đoạn se sợi.", 12),
    item("MAT_SILK_THREAD_001", "Sợi tơ", "Sợi tơ đã se, đủ chắc để dệt thành vải.", 15),
]

eal = unreal.EditorAssetLibrary
dtl = unreal.DataTableFunctionLibrary


def load_rows(chapter, source):
    with open("{}{}/Story/{}".format(SOURCE_ROOT, chapter, source), encoding="utf-8") as f:
        return json.load(f)


def main():
    for name, source, newest_first in TABLES:
        table = unreal.load_asset("{}/{}".format(STORY_DIR, name))
        if not table:
            raise RuntimeError("{} is missing; run build_chapter1_story.py first".format(name))
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
    added = [r for r in SILK_ITEMS if r["Name"] not in have]
    if added:
        if not dtl.fill_data_table_from_json_string(items, json.dumps(current + added, ensure_ascii=False)):
            raise RuntimeError("Could not add silk items")
        eal.save_loaded_asset(items)
    print("ITEM_ADDED", [r["Name"] for r in added])


main()
