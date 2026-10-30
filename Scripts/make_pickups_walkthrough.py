import unreal


# Every visible model uses the Trigger collision preset (overlap only, never blocks), so the player
# walks through all of them; InteractionRange (the sphere every BP_ItemPickup has) drives the E prompt.
# Covers BP_ItemPickup's inherited SM_Pickup (all item pickups, BP_VillageInteractable bushes, trays and
# Chapter 2 stations) and BP_VillageNPC's NPCMesh / BodyBlock. Trigger has no physics collision, so this
# refuses to run on a pickup that simulates physics (it would fall through the floor). Safe to re-run.
TARGETS = [
    ("/Game/SilkVillage/Blueprints/BP_ItemPickup", ["SM_Pickup"]),
    ("/Game/SilkVillage/NPC/BP_VillageNPC", ["NPCMesh", "BodyBlock"]),
]


def describe(comp):
    return "enabled={} profile={} pawn={} physics={}".format(
        comp.get_collision_enabled(), comp.get_collision_profile_name(),
        comp.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN), comp.is_simulating_physics())


def components(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    return {str(lib.get_variable_name(lib.get_data(h))): lib.get_object(lib.get_data(h))
            for h in sub.k2_gather_subobject_data_for_blueprint(bp)}


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("Stop Play In Editor first")
    for path, names in TARGETS:
        bp = unreal.load_asset(path)
        comps = components(bp)
        for name in names:
            comp = comps.get(name)
            if not comp:
                raise RuntimeError("{} not found in {}: {}".format(name, path, sorted(comps)))
            if comp.is_simulating_physics():
                raise RuntimeError("{}.{} simulates physics; Trigger would drop it through the floor".format(path, name))
            before = describe(comp)
            comp.set_collision_profile_name("Trigger")
            comp.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
            print("MODEL_COLLISION {}.{} before {} | after {}".format(path.split("/")[-1], name, before, describe(comp)))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    trigger = components(unreal.load_asset(TARGETS[0][0])).get("InteractionRange")
    print("MODEL_COLLISION BP_ItemPickup.InteractionRange (trigger) {}".format(describe(trigger)))


main()
