import unreal


# Props and stations should appear as the story reaches them, not all be on show from the first minute. This tags each one
# ShowFlag_<story flag>; UVillageStorySubsystem hides tagged actors (and their collision) at chapter start and shows them
# the moment that flag is set (and hides them again if it is cleared). Idempotent: it replaces earlier ShowFlag_ tags.
# Run it last, after build_chapter2/3/4_6_village.py, fix_interaction_overlaps.py and build_meshy_props.py, on /Game/Mountain.
# RULES: first match wins; (label prefix | folder prefix | mesh name) -> flag.
RULES = [
    # Chapter 2: festival materials once Day 2 starts; the arranging spots once the decorations are hung; the reeling pot on Day 2.
    ("label", "Deco_", "Day2.Started"),
    ("label", "Spot_", "Obj.Q11.HangDecorations"),
    ("label", "Station_ReelingPot", "Day2.Started"),
    # Chapter 3: the spinning wheel and its spools on Day 3.
    ("label", "Station_SpinningWheel", "Day3.Started"),
    ("label", "Spin_Spool", "Day3.Started"),
    # Chapter 4: dyeing and weaving.
    ("label", "Gather_", "Gather.On"),
    ("label", "Station_Dye", "Day4.Started"),
    ("label", "Station_Weave", "Day4.Started"),
    # Chapter 5: the tailoring house, and the accessories once the outfit is designed.
    ("label", "Station_Cut", "Day5.Started"),
    ("label", "Station_Sew", "Day5.Started"),
    ("folder", "MeshyProps/Stage", "Festival.Day"),
    ("label", "AccessoriesTable", "Out.Designed"),
    ("mesh", "SM_ACC_", "Out.Designed"),
    ("mesh", "SM_TOOL_", "Day5.Started"),
    # Chapter 6: the festival stage, signs, display and prizes on festival day.
    ("label", "Station_Ch", "Festival.Day"),
    ("label", "Sign_SIGN_", "Festival.Day"),  # the three festival signs only, not Chapter 1's countdown board (Sign_FestivalCountdown)
    ("label", "StageModel_", "Festival.Day"),
    ("label", "Pedestal_", "Festival.Day"),
]

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def mesh_name(actor):
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    return comp.static_mesh.get_name() if comp and comp.static_mesh else ""


def flag_for(actor):
    label, folder, mesh = actor.get_actor_label(), str(actor.get_folder_path()), mesh_name(actor)
    for kind, key, flag in RULES:
        if (kind == "label" and label.startswith(key)) or (kind == "folder" and folder.startswith(key)) \
                or (kind == "mesh" and mesh.startswith(key) and actor.actor_has_tag("MeshyProp")):
            return flag
    return None


def main():
    gated = {}
    for actor in actor_subsystem.get_all_level_actors():
        flag = flag_for(actor)
        tags = [t for t in actor.tags if not str(t).startswith("ShowFlag_")]
        if flag:
            tags.append(unreal.Name("ShowFlag_" + flag))
            gated[flag] = gated.get(flag, 0) + 1
        if len(tags) != len(actor.tags) or flag:
            actor.tags = tags
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.EditorLevelLibrary.save_all_dirty_levels()  # sublevel actors (Chapter4_6_Village, Meshy_Props) are saved with their own level
    print("UNLOCK_GATES", sum(gated.values()), "actors gated:", ", ".join("{}={}".format(k, v) for k, v in sorted(gated.items())))


main()
