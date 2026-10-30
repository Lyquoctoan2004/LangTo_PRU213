import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
print('IN PLAY BEFORE', level.is_in_play_in_editor())
if level.is_in_play_in_editor():
    level.editor_request_end_play()
