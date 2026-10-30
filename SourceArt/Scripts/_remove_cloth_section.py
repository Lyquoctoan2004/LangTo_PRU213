import unreal

ui = unreal.load_asset('/Game/UI/DynamicallyGeneratedGUI_DGGUI/DynamicallyGeneratedGUI_DGGUI')
graph = unreal.find_object(ui, 'CreateParameterWidget')
utils = unreal.MutableSampleGraphUtils
helpers = unreal.MutableSampleEditorHelpers
nodes = {x.get_name(): x for x in utils.get_objects_in_package(ui) if x.get_outer() == graph}
assert 'UIFilter_EqualsAgedClothes' not in nodes

def call(name, owner, function, x, y):
    cls = unreal.load_class(None, '/Script/BlueprintGraph.K2Node_CallFunction')
    node = utils.create_graph_node(graph, cls, x, y)
    assert node
    node.rename(name)
    ref = '(MemberParent="/Script/CoreUObject.Class\'/Script/' + owner + '\'",MemberName="' + function + '")'
    assert utils.set_property_text(node, 'FunctionReference', ref)
    utils.reconstruct_node(node)
    return node

aged_clothes = call('UIFilter_EqualsAgedClothes', 'Engine.KismetStringLibrary', 'EqualEqual_StrStr', -1020, 740)
all_filtered = call('UIFilter_OrCloth', 'Engine.KismetMathLibrary', 'BooleanOR', -180, 460)
assert helpers.set_pin_default(ui, aged_clothes.get_name(), 'B', 'Aged Clothes')
assert utils.link_pins(nodes['K2Node_FunctionEntry_0'], 'ParamName', aged_clothes, 'A')
assert utils.break_pin_links(nodes['UIFilter_OrAll'], 'ReturnValue')
assert utils.link_pins(nodes['UIFilter_OrAll'], 'ReturnValue', all_filtered, 'A')
assert utils.link_pins(aged_clothes, 'ReturnValue', all_filtered, 'B')
assert utils.link_pins(all_filtered, 'ReturnValue', nodes['UIFilter_Branch'], 'Condition')
ui.modify()
unreal.BlueprintEditorLibrary.compile_blueprint(ui)
print('STATUS', helpers.get_property_text(ui, 'Status'))
assert helpers.get_property_text(ui, 'Status') == 'BS_UpToDate'
assert unreal.EditorAssetLibrary.save_loaded_asset(ui, False)
