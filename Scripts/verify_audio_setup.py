import unreal


expected_assets = [
    "/Game/Audio/SFX/UI/SFX_StageComplete",
    "/Game/Audio/SFX/Gameplay/SFX_Cut",
    "/Game/Audio/SFX/UI/SFX_Equip",
    "/Game/Audio/SFX/UI/SFX_Fail",
    "/Game/Audio/Ambience/AMB_Wind",
    "/Game/Audio/SFX/Gameplay/SFX_ItemDrop",
    "/Game/Audio/SFX/UI/SFX_ItemClick",
    "/Game/Audio/SFX/UI/SFX_Open",
    "/Game/Audio/SFX/Gameplay/SFX_Spin",
    "/Game/Audio/SFX/Gameplay/SFX_FeedSilkworm",
    "/Game/Audio/SFX/Gameplay/SFX_FootstepGrass",
    "/Game/Audio/SFX/Gameplay/SFX_Footsteps",
    "/Game/Audio/Ambience/AMB_EveningDog",
    "/Game/Audio/Ambience/AMB_Loom",
    "/Game/Audio/Ambience/AMB_Leaves",
    "/Game/Audio/Ambience/AMB_Birds",
    "/Game/Audio/SFX/Gameplay/SFX_CollectItem",
    "/Game/Audio/Music/MUS_Chill_02",
    "/Game/Audio/Music/MUS_LivelyVillage",
    "/Game/Audio/Music/MUS_Noon",
    "/Game/Audio/Music/MUS_MainMenu",
    "/Game/Audio/Music/MUS_Emotional",
    "/Game/Audio/Music/MUS_Festival",
]

missing = [path for path in expected_assets if not unreal.EditorAssetLibrary.does_asset_exist(path)]
if missing:
    raise RuntimeError("Missing imported audio assets: " + repr(missing))

for path in ("/Game/Lobby/Lobby", "/Game/Mountain"):
    unreal.EditorLoadingAndSavingUtils.load_map(path)
    actors = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if "CodexAutoAudio" in [str(tag) for tag in actor.tags]:
            component = actor.get_component_by_class(unreal.AudioComponent)
            sound = component.get_editor_property("sound")
            actors.append((actor.get_actor_label(), sound.get_path_name(), component.get_editor_property("volume_multiplier")))
    print("AUDIO_ACTORS %s %s" % (path, actors))

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Mountain")
print("VERIFIED_AUDIO_ASSETS=%d" % len(expected_assets))
