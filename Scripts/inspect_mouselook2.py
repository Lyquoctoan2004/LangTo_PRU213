import unreal

imc = unreal.load_object(None, "/Game/Input/IMC_MouseLook.IMC_MouseLook")
cls = imc.get_class()
for candidate in ["mappings", "Mappings", "key_mappings", "KeyMappings", "DefaultKeyMappings", "default_key_mappings"]:
    try:
        v = imc.get_editor_property(candidate)
        print(candidate, "=>", v)
    except Exception as e:
        print(candidate, "ERR", e)
