import unreal

bp = unreal.load_object(None, "/Game/Input/Touch/UI_Thumbstick.UI_Thumbstick")
print("BP", bp)

gen_class = bp.generated_class()
print("GEN CLASS", gen_class)

cdo = unreal.get_default_object(gen_class)
print("CDO", cdo)

# list properties
for prop_name in dir(cdo):
    if prop_name.startswith("_"):
        continue

# Try BlueprintEditorLibrary
try:
    graphs = unreal.BlueprintEditorLibrary.get_graphs_of_blueprint(bp) if hasattr(unreal.BlueprintEditorLibrary, "get_graphs_of_blueprint") else None
    print("GRAPHS", graphs)
except Exception as e:
    print("ERR", e)

print([m for m in dir(unreal.BlueprintEditorLibrary) if "graph" in m.lower() or "node" in m.lower()])
