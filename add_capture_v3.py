import unreal

editor_asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
char_bp = editor_asset_subsystem.load_asset("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter")

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles = subsystem.k2_gather_subobject_data_for_blueprint(char_bp)

# Check if MinimapCapture already exists
for h in handles:
    data = subsystem.k2_find_subobject_data_from_handle(h)
    data_obj = data[0] if isinstance(data, tuple) else data
    comp_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data_obj)
    if comp_obj and "MinimapCapture" in comp_obj.get_name():
        print("MinimapCapture component already exists!")
        break

root_handle = None
for h in handles:
    data = subsystem.k2_find_subobject_data_from_handle(h)
    data_obj = data[0] if isinstance(data, tuple) else data
    if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(data_obj):
        root_handle = h
        break

if not root_handle and len(handles) > 0:
    root_handle = handles[0]

params = unreal.AddNewSubobjectParams()
params.set_editor_property("parent_handle", root_handle)
params.set_editor_property("new_class", unreal.SceneCaptureComponent2D)
params.set_editor_property("blueprint_context", char_bp)

new_handle, fail_reason = subsystem.add_new_subobject(params)
is_valid = unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(new_handle)
print(f"Handle is valid: {is_valid}")

if is_valid:
    subsystem.rename_subobject(new_handle, "MinimapCapture")
    data = subsystem.k2_find_subobject_data_from_handle(new_handle)
    data_obj = data[0] if isinstance(data, tuple) else data
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data_obj)
    
    if comp:
        print(f"Configuring component: {comp}")
        rt_asset = editor_asset_subsystem.load_asset("/Game/SilkVillage/UI/RT_Minimap")
        comp.set_editor_property("texture_target", rt_asset)
        comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
        comp.set_editor_property("ortho_width", 4000.0)
        comp.set_relative_location(unreal.Vector(0, 0, 3000), False, False)
        comp.set_relative_rotation(unreal.Rotator(-90, 0, 0), False, False)
        comp.set_editor_property("absolute_rotation", True)
        print("Configured all properties successfully!")
        
    unreal.BlueprintEditorLibrary.compile_blueprint(char_bp)
    editor_asset_subsystem.save_asset("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter")
    print("SUCCESS: SAVED BP_ThirdPersonCharacter with MinimapCapture!")
