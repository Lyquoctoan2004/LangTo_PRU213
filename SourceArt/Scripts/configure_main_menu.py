import unreal
import uuid


MENU_PATH = '/Game/Lobby/UIElements/BasicMenu'
menu = unreal.EditorAssetLibrary.load_asset(MENU_PATH)
helpers = unreal.MutableSampleEditorHelpers
graph_utils = unreal.MutableSampleGraphUtils
objects = helpers.get_nested_objects(menu)
widgets = {obj.get_name(): obj for obj in objects if isinstance(obj, unreal.Widget)}
assert 'SettingsButton' not in widgets, 'The menu is already configured.'
created_widgets = []
tree = unreal.find_object(menu, 'WidgetTree')
canvas = widgets['CanvasPanel_44']
main = widgets['MainMenuContainer']
font = widgets['TextBlock_177'].get_editor_property('font').copy()
button_style = widgets['EditCharacterButton'].get_editor_property('widget_style').copy()


def make_widget(cls, name, variable=False):
    obj = unreal.new_object(cls, outer=tree, name=name)
    created_widgets.append(obj)
    if variable:
        assert helpers.set_property_text(obj, 'bIsVariable', 'True')
    return obj


def label(name, text, size=22):
    obj = make_widget(unreal.TextBlock, name)
    obj.set_text(text)
    info = font.copy()
    info.size = size
    obj.set_font(info)
    obj.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(1, 1, 1, 1)))
    return obj


def button(name, text):
    obj = make_widget(unreal.Button, name, True)
    obj.set_style(button_style)
    box = make_widget(unreal.SizeBox, name + 'Size')
    box.set_height_override(52)
    obj.add_child(box)
    content = label(name + 'Label', text)
    content.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(0.12, 0.055, 0.02, 1)))
    assert helpers.set_property_text(content, 'Justification', 'Center')
    box.add_child(content)
    content.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    content.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    return obj


menu.modify()
for name in ['LoadInfiniteButton', 'LoadShowcaseButton', 'OptionContainer']:
    widgets[name].set_visibility(unreal.SlateVisibility.COLLAPSED)
widgets['TextBlock_177'].set_text('Start Game')
widgets['TextBlock_4'].set_text('Exit')
for name in ['TextBlock_177', 'TextBlock_4']:
    widgets[name].set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(0.12, 0.055, 0.02, 1)))
    assert helpers.set_property_text(widgets[name], 'Justification', 'Center')
widgets['TextBlock_162'].set_text('MAIN MENU')
title_font = font.copy()
title_font.size = 28
widgets['TextBlock_162'].set_font(title_font)
for name in ['SizeBox_0', 'SizeBox_5']:
    widgets[name].set_height_override(52)

settings_button = button('SettingsButton', 'Settings')
for obj in [widgets['TextBlock_162'], widgets['EditCharacterButton'], widgets['ExitButton']]:
    main.remove_child(obj)
for obj in [widgets['TextBlock_162'], widgets['EditCharacterButton'], settings_button, widgets['ExitButton']]:
    slot = main.add_child_to_vertical_box(obj)
    slot.set_padding(unreal.Margin(bottom=14 if isinstance(obj, unreal.Button) else 24))
main.slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0.045, 0.24), maximum=unreal.Vector2D(0.32, 0.8)))
main.slot.set_offsets(unreal.Margin())

panel = make_widget(unreal.Border, 'SettingsPanel', True)
panel.set_brush_color(unreal.LinearColor(0.015, 0.022, 0.035, 0.96))
panel.set_padding(unreal.Margin(left=24, top=24, right=24, bottom=24))
panel.set_visibility(unreal.SlateVisibility.COLLAPSED)
panel_slot = canvas.add_child_to_canvas(panel)
panel_slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0.045, 0.20), maximum=unreal.Vector2D(0.37, 0.62)))
panel_slot.set_offsets(unreal.Margin())
panel_slot.set_z_order(20)
settings = make_widget(unreal.VerticalBox, 'SettingsContent')
panel.add_child(settings)
settings.add_child_to_vertical_box(label('SettingsTitle', 'SETTINGS', 28)).set_padding(unreal.Margin(bottom=24))
settings.add_child_to_vertical_box(label('BrightnessTitle', 'Brightness', 22)).set_padding(unreal.Margin(bottom=8))
settings.add_child_to_vertical_box(label('BrightnessHint', 'Adjust the screen brightness.', 14)).set_padding(unreal.Margin(bottom=18))
slider_box = make_widget(unreal.SizeBox, 'BrightnessSliderSize')
slider_box.set_height_override(38)
settings.add_child_to_vertical_box(slider_box).set_padding(unreal.Margin(bottom=6))
slider = make_widget(unreal.Slider, 'BrightnessSlider', True)
slider.set_min_value(1.2)
slider.set_max_value(3.2)
slider.set_value(2.2)
slider.set_step_size(0.05)
slider.set_slider_bar_color(unreal.LinearColor(0.18, 0.21, 0.26, 1))
slider.set_slider_handle_color(unreal.LinearColor(0.95, 0.66, 0.28, 1))
slider_box.add_child(slider)
settings.add_child_to_vertical_box(label('BrightnessRange', 'Drag left to dim, right to brighten.', 14)).set_padding(unreal.Margin(bottom=28))
back_button = button('SettingsBackButton', 'Back')
settings.add_child_to_vertical_box(back_button)
guid_map = helpers.get_property_text(menu, 'WidgetVariableNameToGuidMap')
entries = guid_map[1:-1]
for obj in created_widgets:
    entries += ',("' + obj.get_name() + '", ' + uuid.uuid4().hex.upper() + ')'
assert helpers.set_property_text(menu, 'WidgetVariableNameToGuidMap', '(' + entries + ')')
unreal.BlueprintEditorLibrary.compile_blueprint(menu)
print('Main menu widgets constructed.')
