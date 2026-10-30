import unreal

ui = unreal.load_asset('/Game/UI/DynamicallyGeneratedGUI_DGGUI/DynamicallyGeneratedGUI_DGGUI')
graph = unreal.find_object(ui, 'CreateParameterWidget')
utils = unreal.MutableSampleGraphUtils
helpers = unreal.MutableSampleEditorHelpers
nodes = {x.get_name(): x for x in utils.get_objects_in_package(ui) if x.get_outer() == graph}
assert 'UIFilter_EqualsEyeColor' not in nodes
secondary = nodes['UIFilter_EqualsSecondary']
assert helpers.set_pin_default(ui, secondary.get_name(), 'B', 'Color Cloth and Shoes')
secondary.rename('UIFilter_EqualsColorClothShoes')

def call(name, owner, function, x, y):
    cls = unreal.load_class(None, '/Script/BlueprintGraph.K2Node_CallFunction')
    node = utils.create_graph_node(graph, cls, x, y)
    assert node
    node.rename(name)
    ref = '(MemberParent="/Script/CoreUObject.Class\'/Script/' + owner + '\'",MemberName="' + function + '")'
    assert utils.set_property_text(node, 'FunctionReference', ref)
    utils.reconstruct_node(node)
    return node

eye = call('UIFilter_EqualsEyeColor', 'Engine.KismetStringLibrary', 'EqualEqual_StrStr', -1020, 580)
all_filtered = call('UIFilter_OrAll', 'Engine.KismetMathLibrary', 'BooleanOR', -450, 400)
assert helpers.set_pin_default(ui, eye.get_name(), 'B', 'Eye Color')
assert utils.link_pins(nodes['K2Node_FunctionEntry_0'], 'ParamName', eye, 'A')
assert utils.break_pin_links(nodes['UIFilter_Or'], 'ReturnValue')
assert utils.link_pins(nodes['UIFilter_Or'], 'ReturnValue', all_filtered, 'A')
assert utils.link_pins(eye, 'ReturnValue', all_filtered, 'B')
assert utils.link_pins(all_filtered, 'ReturnValue', nodes['UIFilter_Branch'], 'Condition')
ui.modify()
unreal.BlueprintEditorLibrary.compile_blueprint(ui)
print('STATUS', helpers.get_property_text(ui, 'Status'))
print('DEFAULTS', helpers.list_pin_defaults(ui, 'Color Cloth and Shoes'), helpers.list_pin_defaults(ui, 'Eye Color'))
assert helpers.get_property_text(ui, 'Status') == 'BS_UpToDate'
assert unreal.EditorAssetLibrary.save_loaded_asset(ui, False)
