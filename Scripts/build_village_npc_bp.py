import unreal


# BP_VillageNPC derives from BP_ItemPickup because the player's NearbyPickups/CurrentInteractable
# are typed BP_ItemPickup: inheriting gives the InteractionRange overlap and the E-key flow.
GU = unreal.MutableSampleGraphUtils
EH = unreal.MutableSampleEditorHelpers
BEL = unreal.BlueprintEditorLibrary

PICKUP_PATH = "/Game/SilkVillage/Blueprints/BP_ItemPickup"
NPC_BP_PATH = "/Game/SilkVillage/NPC/BP_VillageNPC"
INTERACTABLE_BP_PATH = "/Game/SilkVillage/Blueprints/BP_VillageInteractable"
INTERFACE_CLASS = "/Game/SilkVillage/Blueprints/Interfaces/BPI_Interactable.BPI_Interactable_C"

K2_EVENT = unreal.load_class(None, "/Script/BlueprintGraph.K2Node_Event")
K2_CALL = unreal.load_class(None, "/Script/BlueprintGraph.K2Node_CallFunction")
K2_GET = unreal.load_class(None, "/Script/BlueprintGraph.K2Node_VariableGet")
K2_CAST = unreal.load_class(None, "/Script/BlueprintGraph.K2Node_DynamicCast")
K2_SELF = unreal.load_class(None, "/Script/BlueprintGraph.K2Node_Self")


def graph_nodes(graph):
    return [o for o in EH.get_nested_objects(graph) if isinstance(o, unreal.EdGraphNode) and o.get_outer() == graph]


def link(a, a_pin, b, b_pin):
    if not GU.link_pins(a, a_pin, b, b_pin):
        raise RuntimeError("Link failed {}.{} -> {}.{}\n{}\n{}".format(
            a.get_name(), a_pin, b.get_name(), b_pin, GU.dump_pins(a), GU.dump_pins(b)))


def call_node(graph, owner_class, function, x, y):
    node = GU.create_graph_node(graph, K2_CALL, x, y)
    GU.set_property_text(node, "FunctionReference",
                         "(MemberParent=\"/Script/CoreUObject.Class'{}'\",MemberName=\"{}\")".format(owner_class, function))
    GU.reconstruct_node(node)
    return node


def var_node(graph, name, x, y):
    node = GU.create_graph_node(graph, K2_GET, x, y)
    GU.set_property_text(node, "VariableReference", "(MemberName=\"{}\",bSelfContext=True)".format(name))
    GU.reconstruct_node(node)
    return node


def has_variable(bp, name):
    return 'VarName="{}"'.format(name) in GU.get_property_text(bp, "NewVariables")


def add_fallback_prompt_to_pickup():
    """GetInteractionText returns FallbackPrompt when ItemID has no DT_ItemData row."""
    bp = unreal.load_asset(PICKUP_PATH)
    if not has_variable(bp, "FallbackPrompt"):
        BEL.add_member_variable(bp, "FallbackPrompt", BEL.get_basic_type_by_name("text"))
        BEL.set_blueprint_variable_instance_editable(bp, "FallbackPrompt", True)
        BEL.compile_blueprint(bp)
    graph = BEL.find_graph(bp, "GetInteractionText")
    for node in graph_nodes(graph):
        pins = GU.dump_pins(node)
        if node.get_class().get_name() == "K2Node_FunctionResult" and "RowNotFound" in pins:
            if "InteractionText [in] text -> ;" not in pins and "FallbackPrompt" in pins:
                return "already wired"
            getter = var_node(graph, "FallbackPrompt", int(GU.get_property_text(node, "NodePosX") or 0) - 250,
                              int(GU.get_property_text(node, "NodePosY") or 0) + 80)
            link(getter, "FallbackPrompt", node, "InteractionText")
            BEL.compile_blueprint(bp)
            unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
            return "wired"
    raise RuntimeError("RowNotFound result node not found in GetInteractionText")


def ensure_npc_blueprint():
    if unreal.EditorAssetLibrary.does_asset_exist(NPC_BP_PATH):
        return unreal.load_asset(NPC_BP_PATH)
    parent = unreal.load_asset(PICKUP_PATH).generated_class()
    return BEL.create_blueprint_asset_with_parent(NPC_BP_PATH, parent)


def ensure_components(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    by_name = {str(lib.get_variable_name(lib.get_data(h))): h for h in handles}
    root = by_name["DefaultSceneRoot"]

    def add(name, cls):
        if name in by_name:
            return lib.get_object(lib.get_data(by_name[name]))
        handle, reason = sub.add_new_subobject(
            unreal.AddNewSubobjectParams(parent_handle=root, new_class=cls, blueprint_context=bp))
        if not lib.is_handle_valid(handle):
            raise RuntimeError("Could not add {}: {}".format(name, reason))
        sub.rename_subobject(handle, unreal.Text(name))
        return lib.get_object(lib.get_data(handle))

    mesh = add("NPCMesh", unreal.SkeletalMeshComponent)
    # Meshy characters face +Y; turning the mesh makes the actor's +X the NPC's facing.
    mesh.set_editor_property("relative_rotation", unreal.Rotator(0, 0, -90))
    mesh.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    mesh.set_collision_profile_name("Trigger")  # overlap only: never blocks or lifts the player
    body = add("BodyBlock", unreal.CapsuleComponent)
    # NPCs are walk-through; only InteractionRange (overlap) matters. Kept as a disabled
    # capsule so switching blocking back on is one profile change.
    body.set_editor_property("relative_location", unreal.Vector(0, 0, 150))
    body.set_capsule_size(34.0, 150.0)
    body.set_collision_profile_name("Trigger")
    # Nothing on an NPC is floor: no stepping up onto it and no standing on its head.
    unwalkable = unreal.WalkableSlopeOverride()
    unwalkable.set_editor_property("walkable_slope_behavior", unreal.WalkableSlopeBehavior.WALKABLE_SLOPE_UNWALKABLE)
    for prim in (mesh, body):
        prim.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
        prim.set_walkable_slope_override(unwalkable)


def ensure_variables(bp):
    anim_type = BEL.get_object_reference_type(unreal.AnimSequence.static_class())
    text_type = BEL.get_basic_type_by_name("text")
    # DisplayName / DisplayRole feed the nameplate UVillageStorySubsystem floats above the NPC.
    for name, pin_type in (("IdleAnimation", anim_type), ("TalkAnimation", anim_type),
                           ("TalkDuration", BEL.get_basic_type_by_name("real")),
                           ("DisplayName", text_type), ("DisplayRole", text_type)):
        if not has_variable(bp, name):
            BEL.add_member_variable(bp, name, pin_type)
            BEL.set_blueprint_variable_instance_editable(bp, name, True)


def build_interact_override(bp):
    graph = BEL.find_event_graph(bp)
    for node in graph_nodes(graph):  # everything in this child graph is generated here
        GU.remove_node(node)

    event = GU.create_graph_node(graph, K2_EVENT, 0, 0)
    GU.set_property_text(event, "EventReference",
                         "(MemberParent=\"/Script/Engine.BlueprintGeneratedClass'{}'\",MemberName=\"Interact\")".format(INTERFACE_CLASS))
    GU.set_property_text(event, "bOverrideFunction", "True")
    GU.reconstruct_node(event)

    # Turn to face whoever pressed E (yaw only).
    cast = GU.create_graph_node(graph, K2_CAST, 250, 0)
    GU.set_property_text(cast, "TargetType", "/Script/CoreUObject.Class'/Script/Engine.Actor'")
    GU.reconstruct_node(cast)
    link(event, "then", cast, "execute")
    link(event, "Interactor", cast, "Object")
    self_loc = call_node(graph, "/Script/Engine.Actor", "K2_GetActorLocation", 250, 250)
    other_loc = call_node(graph, "/Script/Engine.Actor", "K2_GetActorLocation", 550, 250)
    link(cast, "AsActor", other_loc, "self")
    look = call_node(graph, "/Script/Engine.KismetMathLibrary", "FindLookAtRotation", 800, 250)
    link(self_loc, "ReturnValue", look, "Start")
    link(other_loc, "ReturnValue", look, "Target")
    brk = call_node(graph, "/Script/Engine.KismetMathLibrary", "BreakRotator", 1050, 250)
    link(look, "ReturnValue", brk, "InRot")
    make = call_node(graph, "/Script/Engine.KismetMathLibrary", "MakeRotator", 1250, 250)
    link(brk, "Yaw", make, "Yaw")
    face = call_node(graph, "/Script/Engine.Actor", "K2_SetActorRotation", 1500, 0)
    link(cast, "then", face, "execute")
    link(make, "ReturnValue", face, "NewRotation")

    # Dialogue, quest and talk/idle animation are handled in C++ (UVillageStorySubsystem).
    forward_to_story(graph, face, cast, 1800)


def forward_to_story(graph, after, cast, x):
    """after.then -> UVillageStoryLibrary::HandleInteract(self, Interactor as Actor)."""
    me = GU.create_graph_node(graph, K2_SELF, x, 250)
    handle = call_node(graph, "/Script/MutableSample.VillageStoryLibrary", "HandleInteract", x + 250, 0)
    link(after, "then", handle, "execute")
    link(me, "self", handle, "Target")
    link(cast, "AsActor", handle, "Interactor")


def build_interactable_blueprint():
    """Bushes and silkworm trays: BP_ItemPickup child whose Interact goes to the story."""
    if unreal.EditorAssetLibrary.does_asset_exist(INTERACTABLE_BP_PATH):
        bp = unreal.load_asset(INTERACTABLE_BP_PATH)
    else:
        bp = BEL.create_blueprint_asset_with_parent(INTERACTABLE_BP_PATH, unreal.load_asset(PICKUP_PATH).generated_class())
    BEL.compile_blueprint(bp)
    graph = BEL.find_event_graph(bp)
    for node in graph_nodes(graph):
        GU.remove_node(node)
    event = GU.create_graph_node(graph, K2_EVENT, 0, 0)
    GU.set_property_text(event, "EventReference",
                         "(MemberParent=\"/Script/Engine.BlueprintGeneratedClass'{}'\",MemberName=\"Interact\")".format(INTERFACE_CLASS))
    GU.set_property_text(event, "bOverrideFunction", "True")
    GU.reconstruct_node(event)
    cast = GU.create_graph_node(graph, K2_CAST, 250, 0)
    GU.set_property_text(cast, "TargetType", "/Script/CoreUObject.Class'/Script/Engine.Actor'")
    GU.reconstruct_node(cast)
    link(event, "then", cast, "execute")
    link(event, "Interactor", cast, "Object")
    forward_to_story(graph, cast, cast, 550)
    BEL.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    return bp


def main():
    print("PICKUP_PROMPT", add_fallback_prompt_to_pickup())
    bp = ensure_npc_blueprint()
    ensure_components(bp)
    ensure_variables(bp)
    BEL.compile_blueprint(bp)
    build_interact_override(bp)
    BEL.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("TalkDuration", 4.0)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    print("NPC_BP status={} nodes={}".format(GU.get_property_text(bp, "Status"),
                                             len(graph_nodes(BEL.find_event_graph(bp)))))
    item_bp = build_interactable_blueprint()
    print("INTERACTABLE_BP status={} nodes={}".format(GU.get_property_text(item_bp, "Status"),
                                                      len(graph_nodes(BEL.find_event_graph(item_bp)))))


main()
