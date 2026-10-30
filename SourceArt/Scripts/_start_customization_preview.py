import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'lobby.AutoCustomize 1')
print('PLAY BEFORE', level.is_in_play_in_editor())
if not level.is_in_play_in_editor():
    level.editor_request_begin_play()
print('PLAY AFTER REQUEST', level.is_in_play_in_editor())
