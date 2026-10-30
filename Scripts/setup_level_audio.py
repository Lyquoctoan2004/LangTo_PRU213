import unreal


AUDIO_TAG = "CodexAutoAudio"


def remove_previous_audio():
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actor_subsystem.get_all_level_actors():
        if AUDIO_TAG in [str(tag) for tag in actor.tags]:
            actor_subsystem.destroy_actor(actor)


def add_ambient_sound(label, asset_path, volume):
    sound = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not sound:
        raise RuntimeError("Missing sound asset: " + asset_path)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actor_subsystem.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.tags = [unreal.Name(AUDIO_TAG)]

    component = actor.get_component_by_class(unreal.AudioComponent)
    component.set_sound(sound)
    component.set_editor_property("volume_multiplier", volume)
    component.set_editor_property("allow_spatialization", False)
    component.set_editor_property("auto_activate", True)
    return actor


def configure_map(map_path, sounds):
    if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
        raise RuntimeError("Could not load map: " + map_path)
    remove_previous_audio()
    for label, asset_path, volume in sounds:
        add_ambient_sound(label, asset_path, volume)
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("Could not save map: " + map_path)
    print("CONFIGURED_AUDIO=" + map_path)


configure_map(
    "/Game/Lobby/Lobby",
    [
        ("Audio_MenuMusic", "/Game/Audio/Music/MUS_MainMenu", 0.45),
    ],
)

configure_map(
    "/Game/Mountain",
    [
        ("Audio_GameplayMusic", "/Game/Audio/Music/MUS_LivelyVillage", 0.32),
        ("Audio_Birds", "/Game/Audio/Ambience/AMB_Birds", 0.12),
        ("Audio_Wind", "/Game/Audio/Ambience/AMB_Wind", 0.08),
    ],
)

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Mountain")
print("LEVEL_AUDIO_SETUP_COMPLETE")
