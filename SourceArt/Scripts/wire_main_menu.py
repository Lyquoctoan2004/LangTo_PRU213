import unreal
import re


menu = unreal.EditorAssetLibrary.load_asset('/Game/Lobby/UIElements/BasicMenu')
helpers = unreal.MutableSampleEditorHelpers
utils = unreal.MutableSampleGraphUtils
graph = unreal.find_object(menu, 'EventGraph')
guid_text = helpers.get_property_text(menu, 'WidgetVariableNameToGuidMap')
guids = dict(re.findall(r'\("([^"]+)",\s*([A-Fa-f0-9]{32})\)', guid_text))


def property_text(obj, name, value):
    assert helpers.set_property_text(obj, name, value), (obj, name)


def node(kind, name, x, y):
    cls = unreal.load_class(None, '/Script/BlueprintGraph.' + kind)
    obj = utils.create_graph_node(graph, cls, x, y)
    assert obj
    obj.rename(name)
    return obj


def call(name, owner, function, x, y, pure=False):
    obj = node('K2Node_CallFunction', name, x, y)
    property_text(obj, 'FunctionReference', '(MemberParent="/Script/CoreUObject.Class\'/Script/' + owner + '\'",MemberName="' + function + '")')
    property_text(obj, 'bDefaultsToPureFunc', 'True' if pure else 'False')
    utils.reconstruct_node(obj)
    return obj


def event(name, component, owner, delegate, signature, x, y):
    obj = node('K2Node_ComponentBoundEvent', name, x, y)
    property_text(obj, 'DelegatePropertyName', delegate)
    property_text(obj, 'DelegateOwnerClass', "/Script/CoreUObject.Class'/Script/UMG." + owner + "'")
    property_text(obj, 'ComponentPropertyName', component)
    property_text(obj, 'EventReference', '(MemberParent="/Script/CoreUObject.Package\'/Script/UMG\'",MemberName="' + signature + '")')
    property_text(obj, 'bInternalEvent', 'True')
    property_text(obj, 'CustomFunctionName', 'BndEvt__BasicMenu_' + component + '_' + name + '_' + signature)
    utils.reconstruct_node(obj)
    return obj


def getter(component, x, y, suffix):
    obj = node('K2Node_VariableGet', 'Menu_Get' + component + suffix, x, y)
    property_text(obj, 'VariableReference', '(MemberName="' + component + '",MemberGuid=' + guids[component] + ',bSelfContext=True)')
    utils.reconstruct_node(obj)
    return obj


def link(a, apin, b, bpin):
    assert utils.link_pins(a, apin, b, bpin), (a.get_name(), apin, b.get_name(), bpin)


def default(obj, pin, value):
    assert helpers.set_pin_default(menu, obj.get_name(), pin, value), (obj.get_name(), pin)


def visibility(component, value, name, x, y):
    obj = call(name, 'UMG.Widget', 'SetVisibility', x, y)
    reference = getter(component, x - 200, y + 110, name)
    link(reference, component, obj, 'self')
    default(obj, 'InVisibility', value)
    return obj


assert not unreal.find_object(graph, 'Menu_OpenSettings')
open_settings = event('Menu_OpenSettings', 'SettingsButton', 'Button', 'OnClicked', 'OnButtonClickedEvent__DelegateSignature', 0, 1200)
hide_main = visibility('MainMenuContainer', 'Collapsed', 'Menu_HideMain', 380, 1200)
show_settings = visibility('SettingsPanel', 'Visible', 'Menu_ShowSettings', 820, 1200)
link(open_settings, 'then', hide_main, 'execute')
link(hide_main, 'then', show_settings, 'execute')

back = event('Menu_ReturnFromSettings', 'SettingsBackButton', 'Button', 'OnClicked', 'OnButtonClickedEvent__DelegateSignature', 0, 1600)
hide_settings = visibility('SettingsPanel', 'Collapsed', 'Menu_HideSettings', 380, 1600)
show_main = visibility('MainMenuContainer', 'SelfHitTestInvisible', 'Menu_ShowMain', 820, 1600)
link(back, 'then', hide_settings, 'execute')
link(hide_settings, 'then', show_main, 'execute')

brightness = event('Menu_ChangeBrightness', 'BrightnessSlider', 'Slider', 'OnValueChanged', 'OnFloatValueChangedEvent__DelegateSignature', 0, 2050)
command = call('Menu_BuildGammaCommand', 'Engine.KismetStringLibrary', 'BuildString_Double', 380, 2180, True)
default(command, 'Prefix', 'gamma ')
link(brightness, 'Value', command, 'InDouble')
execute = call('Menu_ApplyBrightness', 'Engine.KismetSystemLibrary', 'ExecuteConsoleCommand', 820, 2050)
link(brightness, 'then', execute, 'execute')
link(command, 'ReturnValue', execute, 'Command')

unreal.BlueprintEditorLibrary.compile_blueprint(menu)
print('Menu Blueprint status:', helpers.get_property_text(menu, 'Status'))
assert helpers.get_property_text(menu, 'Status') == 'BS_UpToDate'
assert unreal.EditorAssetLibrary.save_loaded_asset(menu, False)
print('Three-button menu and live brightness control compiled and saved.')
