import unreal


# Plays Day 3 (Chapter 3) in a running PIE session on /Game/Mountain through the real interaction path
# (BPI_Interactable Interact on the placed actors) and UVillageStorySubsystem's debug hooks. Run one stage
# per call so the game ticks in between:
#   STAGE = "jump" | "morning" | "select" | "reel" | "feedback" | "mai" | "spin" | "chief" | "dayend" | "close"
# Set STAGE before exec, e.g. exec("STAGE='morning'\n" + open(path).read()).
# Needs build_chapter3_story.py and build_chapter3_village.py to have run.

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("Start Play In Editor on /Game/Mountain first")
game_instance = unreal.GameplayStatics.get_game_instance(world)
story = next(o for o in unreal.ObjectIterator(unreal.VillageStorySubsystem) if o.get_outer() == game_instance)
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)


def tagged(tag):
    return [a for a in actors if a.actor_has_tag(tag)]


def state(label):
    print("STATE", label, "|", story.debug_describe())


def interact(actor):
    actor.call_method("Interact", (pawn,))


def talk(npc_tag, choices=()):
    interact(tagged(npc_tag)[0])
    picks = list(choices)
    for _ in range(60):
        screen = story.debug_describe().split("screen=")[1]
        if not screen.startswith("dialogue"):
            break
        if " [1] " in screen:
            print("  CHOICES", screen.split("dialogue: ")[1][:200])
            story.debug_choose(picks.pop(0) if picks else 0)
        else:
            print("  LINE", screen.split("dialogue: ")[1].split(" flags=")[0][:160])
            story.debug_advance_dialogue()
            story.debug_advance_dialogue()
    state("after " + npc_tag)


def screenshot(name):
    unreal.SystemLibrary.execute_console_command(world, "HighResShot 1600x900 filename={}".format(name))


def press_timing(rounds):
    """Presses the open timing bar dead centre (perfect) for each round, like a patient player."""
    for _ in range(rounds):
        story.debug_timing_press(0.5)


STAGE = globals().get("STAGE", "jump")
print("STAGE", STAGE)
if STAGE == "jump":
    story.debug_jump_to_day(3)
    state("day 3 morning")
elif STAGE == "morning":
    talk("NPC_Chief")  # Q14 -> Day3.Started, Q15 begins
elif STAGE == "select":
    interact(tagged("Interact_ReelingPot")[0])  # REEL_SELECT dialogue: pick the first (good) cocoons
    for _ in range(10):
        if "screen=dialogue" not in story.debug_describe():
            break
        story.debug_advance_dialogue()
        story.debug_advance_dialogue()
        story.debug_choose(0)
    state("cocoons chosen")
elif STAGE == "reel":
    interact(tagged("Interact_ReelingPot")[0])  # opens the find-thread timing bar
    for _ in range(40):  # the thread bar retries until a press lands in the band; centre may need a few
        if "screen=timing" not in story.debug_describe():
            break
        press_timing(1)
    story.debug_finish_reeling(90.0)
    state("batch reeled")
elif STAGE == "feedback":
    talk("NPC_SilkMaster", [2])  # "Cháu muốn học công đoạn tiếp theo" -> Q16 + Q17 start
elif STAGE == "mai":
    talk("NPC_ThreadDyer")  # Q16.BringSilk
elif STAGE == "spin":
    interact(tagged("Interact_SpinningWheel")[0])
    press_timing(4)
    state("spun")
elif STAGE == "chief":
    talk("NPC_Chief")  # Q17 -> EndDay:3
elif STAGE == "dayend":
    state("day end")
    screenshot("ch3_dayend")
elif STAGE == "close":
    story.debug_close_day_end()
    state("closed")
