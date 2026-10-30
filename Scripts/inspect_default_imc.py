import unreal

imc = unreal.load_object(None, "/Game/Input/IMC_Default.IMC_Default")
data = imc.get_editor_property("DefaultKeyMappings")
print(data)
