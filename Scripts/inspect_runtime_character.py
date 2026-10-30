import unreal

editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor_subsystem.get_game_world()
if not world:
    world = editor_subsystem.get_editor_world()
print("WORLD {}".format(world.get_path_name() if world else "None"))
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if world else []
for actor in actors:
    name = actor.get_name()
    class_name = actor.get_class().get_name()
    if "Character" not in name and "Character" not in class_name and "ThirdPerson" not in name:
        continue
    print("ACTOR {} CLASS {}".format(name, class_name))
    for component in actor.get_components_by_class(unreal.SkeletalMeshComponent):
        mesh = component.get_editor_property("skeletal_mesh_asset")
        anim_class = component.get_editor_property("anim_class")
        leader = component.get_editor_property("leader_pose_component")
        skeleton = mesh.get_editor_property("skeleton") if mesh else None
        print("  MESH_COMPONENT {} mesh={} skeleton={} anim={} leader={}".format(component.get_name(), mesh.get_path_name() if mesh else "None", skeleton.get_path_name() if skeleton else "None", anim_class.get_path_name() if anim_class else "None", leader.get_name() if leader else "None"))
    for component in actor.get_components_by_class(unreal.GroomComponent):
        groom = component.get_editor_property("groom_asset")
        binding = component.get_editor_property("binding_asset")
        print("  GROOM {} asset={} binding={}".format(component.get_name(), groom.get_path_name() if groom else "None", binding.get_path_name() if binding else "None"))
