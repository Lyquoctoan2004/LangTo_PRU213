import os

import unreal


SOURCE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceArt", "Audio"))

AUDIO_ASSETS = [
    ("am thanh hoan thanh giai doan.mp3", "/Game/Audio/SFX/UI", "SFX_StageComplete", False, False),
    ("cut.mp3", "/Game/Audio/SFX/Gameplay", "SFX_Cut", False, False),
    ("equip.mp3", "/Game/Audio/SFX/UI", "SFX_Equip", False, False),
    ("faild.mp3", "/Game/Audio/SFX/UI", "SFX_Fail", False, False),
    ("gió.mp3", "/Game/Audio/Ambience", "AMB_Wind", True, True),
    ("item drop do.mp3", "/Game/Audio/SFX/Gameplay", "SFX_ItemDrop", False, False),
    ("nguoi choi click vao do vat.mp3", "/Game/Audio/SFX/UI", "SFX_ItemClick", False, False),
    ("open.mp3", "/Game/Audio/SFX/UI", "SFX_Open", False, False),
    ("quay to.mp3", "/Game/Audio/SFX/Gameplay", "SFX_Spin", False, False),
    ("sound cho tam an .mp3", "/Game/Audio/SFX/Gameplay", "SFX_FeedSilkworm", False, False),
    ("tieng buoc chan tren co.mp3", "/Game/Audio/SFX/Gameplay", "SFX_FootstepGrass", False, False),
    ("tieng buoc chan.mp3", "/Game/Audio/SFX/Gameplay", "SFX_Footsteps", False, False),
    ("tieng cho sua chieu ve ket thuc ngay.mp3", "/Game/Audio/Ambience", "AMB_EveningDog", True, True),
    ("tieng khung det.mp3", "/Game/Audio/Ambience", "AMB_Loom", True, True),
    ("tieng la xao xac.mp3", "/Game/Audio/Ambience", "AMB_Leaves", True, True),
    ("tieng on ao chim hot.mp3", "/Game/Audio/Ambience", "AMB_Birds", True, True),
    ("tieng thu thap do.mp3", "/Game/Audio/SFX/Gameplay", "SFX_CollectItem", False, False),
    (os.path.join("nhac game", "chill 2.mp3"), "/Game/Audio/Music", "MUS_Chill_02", True, True),
    (os.path.join("nhac game", "lang que soi dong.mp3"), "/Game/Audio/Music", "MUS_LivelyVillage", True, True),
    (os.path.join("nhac game", "nhac buoi trua.mp3"), "/Game/Audio/Music", "MUS_Noon", True, True),
    (os.path.join("nhac game", "nhac chao menu .mp3"), "/Game/Audio/Music", "MUS_MainMenu", True, True),
    (os.path.join("nhac game", "nhac nen cam dong.mp3"), "/Game/Audio/Music", "MUS_Emotional", True, True),
    (os.path.join("nhac game", "nhac ngay le hoi.mp3"), "/Game/Audio/Music", "MUS_Festival", True, True),
]


tasks = []
for relative_file, destination, asset_name, _, _ in AUDIO_ASSETS:
    source_file = os.path.join(SOURCE_ROOT, relative_file)
    if not os.path.isfile(source_file):
        raise RuntimeError("Missing source audio: " + source_file)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_file)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

imported = []
for (_, destination, asset_name, looping, streaming), task in zip(AUDIO_ASSETS, tasks):
    asset_path = destination + "/" + asset_name
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        raise RuntimeError("Unreal did not create " + asset_path)

    asset.set_editor_property("looping", looping)
    if streaming:
        asset.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    imported.append(asset_path)

probe_path = "/Game/Audio/_ImportProbe/SFX_ImportProbe"
if unreal.EditorAssetLibrary.does_asset_exist(probe_path):
    unreal.EditorAssetLibrary.delete_asset(probe_path)

print("IMPORTED_AUDIO_COUNT=%d" % len(imported))
for path in imported:
    print(path)
