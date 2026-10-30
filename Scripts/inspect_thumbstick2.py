import unreal

bp = unreal.load_object(None, "/Game/Input/Touch/UI_Thumbstick.UI_Thumbstick")
eg = unreal.BlueprintEditorLibrary.find_event_graph(bp)
print("EVENTGRAPH", eg)
attrs = [m for m in dir(eg) if "node" in m.lower()]
print("ATTRS", attrs)
try:
    nodes = eg.get_editor_property("nodes") if eg else []
    print("NODE COUNT", len(nodes))
    for node in nodes:
        cls = node.get_class().get_name()
        title = ""
        try:
            title = node.get_node_title(unreal.ENodeTitleType.FULL_TITLE)
        except Exception as e:
            title = str(e)
        print("NODE", cls, "|", title)
except Exception as e:
    print("ERR2", e)
