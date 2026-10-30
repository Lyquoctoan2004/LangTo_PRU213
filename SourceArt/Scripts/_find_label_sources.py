import unreal

co = unreal.load_asset('/Game/Character/CO_Character')
for i in range(co.get_parameter_count()):
    name = co.get_parameter_name(i)
    meta = co.get_parameter_ui_metadata(name)
    friendly = str(meta.get_editor_property('ObjectFriendlyName'))
    if 'Color' in name or 'Color' in friendly or 'Type 1' in name:
        if any(t in name.lower() or t in friendly.lower() for t in ['secondary', 'eye color', 'body type 1']):
            print('PARAM', i, name, 'FRIENDLY', friendly, 'SECTION', meta.get_editor_property('UISectionName'))
