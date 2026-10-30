import unreal

map_path = "/Game/Mountain"
unreal.EditorLoadingAndSavingUtils.load_map(map_path)
actor_class = unreal.load_class(None, "/Script/MutableSample.CustomizedCharacterStart")
if not actor_class:
    raise RuntimeError("CustomizedCharacterStart class is not loaded")

actors = unreal.EditorLevelLibrary.get_all_level_actors()
existing = [actor for actor in actors if actor.get_class() == actor_class]
if existing:
    start = existing[0]
    action = "already existed"
else:
    player_starts = [actor for actor in actors if actor.get_class().get_name() == "PlayerStart"]
    location = player_starts[0].get_actor_location() if player_starts else unreal.Vector(0.0, 0.0, 200.0)
    rotation = player_starts[0].get_actor_rotation() if player_starts else unreal.Rotator(0.0, 0.0, 0.0)
    start = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
    start.set_actor_label("CustomizedCharacterStart")
    action = "created"

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Failed to save /Game/Mountain")

location = start.get_actor_location()
print("CUSTOMIZED_START {} at X={} Y={} Z={}".format(action, location.x, location.y, location.z))
