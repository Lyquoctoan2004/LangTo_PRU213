import unreal

imc = unreal.load_object(None, "/Game/Input/IMC_MouseLook.IMC_MouseLook")
print("IMC", imc)
mappings = imc.get_editor_property("mappings")
for m in mappings:
    action = m.get_editor_property("action")
    key = m.get_editor_property("key")
    triggers = m.get_editor_property("triggers")
    modifiers = m.get_editor_property("modifiers")
    print("MAPPING action={} key={} triggers={} modifiers={}".format(
        action.get_name() if action else None, key, triggers, modifiers))

imc2 = unreal.load_object(None, "/Game/Input/IMC_Default.IMC_Default")
print("IMC_Default", imc2)
mappings2 = imc2.get_editor_property("mappings")
for m in mappings2:
    action = m.get_editor_property("action")
    key = m.get_editor_property("key")
    triggers = m.get_editor_property("triggers")
    modifiers = m.get_editor_property("modifiers")
    print("MAPPING2 action={} key={} triggers={} modifiers={}".format(
        action.get_name() if action else None, key, triggers, modifiers))
