import sys
import unreal

PATHS = [
    "/Game/SilkVillage/UI/WBP_CoinHUD.WBP_CoinHUD",
    "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter",
    "/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController",
]
GU = unreal.MutableSampleGraphUtils
EH = unreal.MutableSampleEditorHelpers

for path in PATHS:
    bp = unreal.load_object(None, path)
    print("=" * 30, path)
    for obj in EH.get_nested_objects(bp):
        if not isinstance(obj, unreal.EdGraphNode):
            continue
        cls = obj.get_class().get_name()
        if cls == "EdGraphNode_Comment":
            continue
        extra = ""
        if cls in ("K2Node_CallFunction", "K2Node_VariableSet", "K2Node_VariableGet", "K2Node_Event", "K2Node_CustomEvent", "K2Node_InputKey", "K2Node_EnhancedInputAction", "K2Node_ComponentBoundEvent"):
            for prop in ("FunctionReference", "VariableReference", "EventReference", "CustomFunctionName", "InputKey", "InputAction", "DelegatePropertyName"):
                t = GU.get_property_text(obj, prop)
                if t:
                    extra += " {}={}".format(prop, t)
        print("NODE {} | {} | graph={}{}".format(obj.get_name(), cls, obj.get_outer().get_name(), extra))
        for line in GU.dump_pins(obj).splitlines():
            if "-> " in line and not line.endswith("-> "):
                print("    " + line)
    for d in EH.list_pin_defaults(bp, ""):
        if any(k in d for k in ("Cursor", "Mouse", "InputMode", "Capture", "Hide")):
            print("  DEFAULT", d)
