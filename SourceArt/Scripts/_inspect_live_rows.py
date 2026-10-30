import unreal

for text in unreal.ObjectIterator(unreal.TextBlock):
    path = text.get_path_name()
    if '/Engine/Transient.UnrealEdEngine_0:GameInstance_' not in path:
        continue
    value = str(text.get_text())
    if value in ['Secondary Color', 'Eye Color', 'Cloth', 'Internal', 'Body Type 1', 'Wear', 'Skin Tone', 'Aged']:
        parent = text.get_parent()
        print('TEXT', value, 'PATH', path, 'PARENT', parent, 'VIS', text.get_visibility())
        outer = text.get_outer()
        print('OUTER', outer, 'OUTER CLASS', outer.get_class().get_name() if outer else None)
