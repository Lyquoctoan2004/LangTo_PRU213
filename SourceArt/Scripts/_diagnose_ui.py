import unreal

ui = unreal.load_asset('/Game/UI/DynamicallyGeneratedGUI_DGGUI/DynamicallyGeneratedGUI_DGGUI')
print('UI STATUS', unreal.MutableSampleEditorHelpers.get_property_text(ui, 'Status'))
print('FILTER PIN DEFAULTS', unreal.MutableSampleEditorHelpers.list_pin_defaults(ui, 'Secondary Color'), unreal.MutableSampleEditorHelpers.list_pin_defaults(ui, 'Body Type 1'))
graph = unreal.find_object(ui, 'CreateParameterWidget')
nodes = {x.get_name(): x for x in unreal.MutableSampleGraphUtils.get_objects_in_package(ui) if x.get_outer() == graph}
print('FILTER EXISTS', 'UIFilter_Branch' in nodes)
if 'UIFilter_Branch' in nodes:
    print('BRANCH', unreal.MutableSampleGraphUtils.dump_pins(nodes['UIFilter_Branch']))
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
print('IN PLAY', level.is_in_play_in_editor())
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
print('WORLD', world)
if world:
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if 'CharacterActor' in actor.get_name():
            for comp in actor.get_components_by_class(unreal.CustomizableSkeletalComponent):
                instance = comp.get_editor_property('CustomizableObjectInstance')
                if instance:
                    print('INSTANCE', instance.get_current_state(), instance.get_customizable_object())
                    break
for w in unreal.ObjectIterator(unreal.UserWidget):
    if '/Engine/Transient.UnrealEdEngine_0:GameInstance_' in w.get_path_name() and w.get_class().get_name().startswith('DynamicallyGeneratedGUI'):
        print('LIVE UI', w, w.get_class())
