import unreal

for widget in unreal.ObjectIterator(unreal.UserWidget):
    path = widget.get_path_name()
    if '/Engine/Transient.UnrealEdEngine_0:GameInstance_8.' not in path:
        continue
    if widget.get_class().get_name().startswith('Color_DGGUI'):
        props = unreal.MutableSampleGraphUtils.dump_properties(widget)
        lines = [line for line in props.splitlines() if any(k in line.lower() for k in ['param', 'color'])]
        print('COLOR WIDGET', widget.get_name(), 'PARENT', widget.get_parent().get_name() if widget.get_parent() else None, 'PROPS', lines[:14])
for text in unreal.ObjectIterator(unreal.TextBlock):
    path = text.get_path_name()
    if '/Engine/Transient.UnrealEdEngine_0:GameInstance_8.' in path and 'SectionText' in path:
        print('SECTION TEXT', str(text.get_text()), path)
