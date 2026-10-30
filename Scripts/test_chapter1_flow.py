import re

import unreal


# Plays Chapter 1 in a running PIE session through the real interaction path (BPI_Interactable
# Interact on the placed actors) and UVillageStorySubsystem's debug hooks. Run one stage per call
# so the game ticks in between:  STAGE = "start" | "talk" | "garden" | "tray" | "finish" | "dayend"
# Set STAGE before exec, e.g. exec("STAGE='talk'\n" + open(path).read()).

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
    # Same call the player's E key makes on BP_ItemPickup subclasses.
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


STAGE = globals().get("STAGE", "start")
print("STAGE", STAGE)
if STAGE == "start":
    state("begin")
    interact(tagged("NPC_Chief")[0])
    screenshot("ch1_dialogue")
elif STAGE == "talk":
    talk("NPC_Chief", [0, 0])  # continues the conversation opened in "start"
    talk("NPC_SilkMaster", [0])
    talk("NPC_ThreadDyer")
    talk("NPC_WeaverTailor")
elif STAGE == "garden":
    bushes = tagged("Interact_MulberryBush")
    print("BUSHES", len(bushes))
    for bush in bushes:
        interact(bush)
        interact(bush)
    interact(bushes[0])  # third pick on an empty bush -> regrow message
    state("after garden")
elif STAGE == "tray":
    interact(tagged("Interact_SilkwormTray")[0])
    screenshot("ch1_silkworm")
    state("tray open")
elif STAGE == "finish":
    for step in range(20):
        d = story.debug_describe()
        if "screen=silkworm" not in d:
            break
        s = dict(re.findall(r"([A-Za-z]+)(-?[\d.]+)", d.split("screen=silkworm ")[1].split(" flags=")[0]))
        hu, t, hm, c = float(s["Hu"]), float(s["T"]), float(s["Hm"]), float(s["C"])
        action = 1 if c < 45 else 2 if t > 30 or hm > 75 else 4 if t < 24 else 3 if hm < 55 else 0
        story.debug_silkworm_action(action)
        print("  ACTION", action, "->", story.debug_describe().split("screen=")[1][:80])
    state("after tray")
    talk("NPC_SilkMaster")
    talk("NPC_Chief")
elif STAGE == "dayend":
    state("day end")
    screenshot("ch1_dayend")
elif STAGE == "close":
    story.debug_close_day_end()
    state("closed")
