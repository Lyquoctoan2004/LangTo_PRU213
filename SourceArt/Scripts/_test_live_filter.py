import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
actor = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if 'CharacterActor' in a.get_name())
comp = actor.get_components_by_class(unreal.CustomizableSkeletalComponent)[0]
instance = comp.get_editor_property('CustomizableObjectInstance')
ui = next(w for w in unreal.ObjectIterator(unreal.UserWidget) if '/Engine/Transient.UnrealEdEngine_0:GameInstance_' in w.get_path_name() and w.get_class().get_name().startswith('DynamicallyGeneratedGUI'))
print('STATE', instance.get_current_state())
for name in ['Secondary Color', 'Body Type 1', 'Eye Color']:
    try:
        result = ui.call_method('CreateParameterWidget', args=(name, instance, -1))
        print('RESULT', name, result)
    except Exception as error:
        print('ERROR', name, str(error))
