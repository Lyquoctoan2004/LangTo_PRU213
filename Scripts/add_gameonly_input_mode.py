import unreal

GU = unreal.MutableSampleGraphUtils
BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"

bp = unreal.load_object(None, BP_PATH)
eg = unreal.BlueprintEditorLibrary.find_event_graph(bp)

begin_play = None
for obj in unreal.MutableSampleEditorHelpers.get_nested_objects(eg):
    if isinstance(obj, unreal.EdGraphNode) and obj.get_outer() == eg:
        if obj.get_class().get_name() == "K2Node_Event" and "ReceiveBeginPlay" in GU.get_property_text(obj, "EventReference"):
            begin_play = obj
        if obj.get_class().get_name() == "K2Node_CallFunction" and "SetInputMode_GameOnly" in GU.get_property_text(obj, "FunctionReference") and obj.get_name().startswith("K2Node_CallFunction_GameOnlyFix"):
            raise RuntimeError("Fix already applied")
if not begin_play:
    raise RuntimeError("BeginPlay not found")

pins = GU.dump_pins(begin_play)
print("BEGINPLAY PINS BEFORE:\n" + pins)
next_node_pin = None
for line in pins.splitlines():
    if line.startswith("then [out]") and "-> " in line:
        target = line.split("-> ", 1)[1].strip().rstrip(";").split(";")[0].strip()
        if target:
            next_node_pin = target
print("NEXT", next_node_pin)

x = int(GU.get_property_text(begin_play, "NodePosX") or 0)
y = int(GU.get_property_text(begin_play, "NodePosY") or 0)

k2_call = unreal.load_class(None, "/Script/BlueprintGraph.K2Node_CallFunction")

set_mode = GU.create_graph_node(eg, k2_call, x + 250, y - 200)
GU.set_property_text(set_mode, "FunctionReference", "(MemberParent=\"/Script/CoreUObject.Class'/Script/UMG.WidgetBlueprintLibrary'\",MemberName=\"SetInputMode_GameOnly\")")
GU.reconstruct_node(set_mode)

get_pc = GU.create_graph_node(eg, k2_call, x, y - 120)
GU.set_property_text(get_pc, "FunctionReference", "(MemberParent=\"/Script/CoreUObject.Class'/Script/Engine.GameplayStatics'\",MemberName=\"GetPlayerController\")")
GU.reconstruct_node(get_pc)

print("SETMODE PINS:\n" + GU.dump_pins(set_mode))
print("GETPC PINS:\n" + GU.dump_pins(get_pc))

all_nodes = {o.get_name(): o for o in unreal.MutableSampleEditorHelpers.get_nested_objects(eg) if isinstance(o, unreal.EdGraphNode) and o.get_outer() == eg}

ok = True
if next_node_pin:
    node_name, pin_name = next_node_pin.split(".", 1)
    next_node = all_nodes[node_name]
    ok &= GU.break_pin_links(begin_play, "then")
    ok &= GU.link_pins(set_mode, "then", next_node, pin_name)
ok &= GU.link_pins(begin_play, "then", set_mode, "execute")
ok &= GU.link_pins(get_pc, "ReturnValue", set_mode, "PlayerController")
print("LINKED", ok)
if not ok:
    raise RuntimeError("Linking failed")

print("BEGINPLAY PINS AFTER:\n" + GU.dump_pins(begin_play))
print("SETMODE PINS AFTER:\n" + GU.dump_pins(set_mode))

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print("SAVED", unreal.EditorAssetLibrary.save_loaded_asset(bp, False))
