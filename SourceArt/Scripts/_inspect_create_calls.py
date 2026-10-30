import unreal

ui = unreal.load_asset('/Game/UI/DynamicallyGeneratedGUI_DGGUI/DynamicallyGeneratedGUI_DGGUI')
graph = unreal.find_object(ui, 'EventGraph')
nodes = {x.get_name(): x for x in unreal.MutableSampleGraphUtils.get_objects_in_package(ui) if x.get_outer() == graph}
for name in ['K2Node_CallFunction_46', 'K2Node_CallFunction_66', 'K2Node_CallFunction_50', 'K2Node_CallFunction_93']:
    print(name, unreal.MutableSampleGraphUtils.dump_properties(nodes[name])[:350])
