import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_game_world()
print("GAME WORLD", world.get_path_name() if world else None)
if world:
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    print("PC", pc.get_class().get_path_name() if pc else None)
    if pc:
        print("show_mouse_cursor", pc.get_editor_property("show_mouse_cursor"))
        pawn = pc.get_controlled_pawn()
        print("PAWN", pawn.get_class().get_path_name() if pawn else None)
    gm = unreal.GameplayStatics.get_game_mode(world)
    print("GM", gm.get_class().get_path_name() if gm else None)

for path in ["/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"]:
    cls = unreal.load_class(None, path)
    cdo = unreal.get_default_object(cls)
    print(path, "CDO show_mouse_cursor", cdo.get_editor_property("show_mouse_cursor"))
    print("parent", cls.get_super_class().get_path_name() if hasattr(cls, "get_super_class") else "?")
