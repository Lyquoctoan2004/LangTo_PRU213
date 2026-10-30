import unreal

ui_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/UI/DynamicallyGeneratedGUI_DGGUI/DynamicallyGeneratedGUI_DGGUI')
widget = unreal.get_default_object(ui_class)
for name in ['Color Cloth and Shoes', 'Aged Clothes', 'Eye Color', 'Body Type 1']:
    result = widget.call_method('CreateParameterWidget', args=(name, None, 0))
    print('FILTER RESULT', name, result)
    assert result == (None, False)
co = unreal.load_asset('/Game/Character/CO_Character')
for i in range(co.get_parameter_count()):
    name = co.get_parameter_name(i)
    meta = co.get_parameter_ui_metadata(name)
    if meta.get_editor_property('UISectionName') == 'Cloth':
        print('CLOTH PARAM', name, meta.get_editor_property('ObjectFriendlyName'))
