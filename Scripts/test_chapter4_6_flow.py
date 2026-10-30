import unreal


# Plays Chapters 4-6 in a running PIE session (map Chapter4to6_MiniGameTest, or /Game/Mountain after
# build_chapter4_6_village.py) through the real interaction path and UVillageStorySubsystem's debug hooks.
# Run one stage per call so the game ticks in between (the sunset needs a few seconds before its card opens):
#   STAGE = "d4" | "d4close" | "d5" | "d5close" (straight into the festival morning) | "fest" | "fest_end" | "jump_festival"
#   or "jump4" / "jump5" / "jump6" (festival) to skip ahead. Crafts are finished with debug_auto_craft (a perfect run).
# Set STAGE before exec, e.g. exec("STAGE='d4'\n" + open(path).read()).

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("Start Play In Editor first")
game_instance = unreal.GameplayStatics.get_game_instance(world)
story = next(o for o in unreal.ObjectIterator(unreal.VillageStorySubsystem) if o.get_outer() == game_instance)
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)


def tagged(tag):
    return [a for a in actors if a.actor_has_tag(tag)]


def station(key):
    """The Interact_Craft / Gather / Dialogue actor whose ItemID is `key`."""
    for a in actors:
        if (a.actor_has_tag("Interact_Craft") or a.actor_has_tag("Interact_Gather") or a.actor_has_tag("Interact_Dialogue")) \
                and str(a.get_editor_property("ItemID")) == key:
            return a
    raise RuntimeError("No station " + key)


def state(label):
    print("STATE", label, "|", story.debug_describe()[:420])


def interact(actor):
    actor.call_method("Interact", (pawn,))


def talk(npc_tag, choices=()):
    interact(tagged(npc_tag)[0])
    picks = list(choices)
    for _ in range(80):
        screen = story.debug_describe().split("screen=")[1]
        if not screen.startswith("dialogue"):
            break
        if " [1] " in screen:
            print("  CHOICES", screen.split("dialogue: ")[1][:170])
            story.debug_choose(picks.pop(0) if picks else 0)
        else:
            print("  LINE", screen.split("dialogue: ")[1].split(" flags=")[0][:110])
            story.debug_advance_dialogue()
            story.debug_advance_dialogue()
    state("after " + npc_tag)


def finish_dialogue(choices=()):
    """Plays out a dialogue that a craft or an action opened by itself."""
    picks = list(choices)
    for _ in range(80):
        screen = story.debug_describe().split("screen=")[1]
        if not screen.startswith("dialogue"):
            break
        if " [1] " in screen:
            story.debug_choose(picks.pop(0) if picks else 0)
        else:
            story.debug_advance_dialogue()
            story.debug_advance_dialogue()


def craft(key):
    interact(station(key))
    story.debug_auto_craft()
    finish_dialogue()
    state("craft " + key)


def gather(key, times):
    for source in [a for a in actors if a.actor_has_tag("Interact_Gather") and str(a.get_editor_property("ItemID")) == key]:
        for _ in range(3):
            if times <= 0:
                return
            interact(source)
            times -= 1


STAGE = globals().get("STAGE", "d4")
print("STAGE", STAGE)
if STAGE.startswith("jump") and STAGE[4:].isdigit():
    story.debug_jump_to_day(int(STAGE[4:]))
    state(STAGE)
elif STAGE == "d4":
    talk("NPC_Chief")  # Q18 -> Q19
    talk("NPC_ThreadDyer")  # Q19.MeetDyer, gathering opens
    gather("DYE_MATERIAL_001", 3)
    gather("DYE_WATER_001", 1)
    state("gathered")
    talk("NPC_ThreadDyer", [0])  # Q20.ChooseColor: red
    craft("Dye")
    talk("NPC_WeaverTailor")  # Q21.TalkWeaver
    craft("Weave")  # opens the story dialogue itself
    talk("NPC_Chief")  # Q22 -> EndDay:D4
elif STAGE == "d4close":
    story.debug_close_day_end()
    state("day 5 morning")
elif STAGE == "d5":
    talk("NPC_WeaverTailor", [0, 1, 0, 3, 0, 0])  # intro, then type / material / color / pattern / accessory / confirm
    state("designed")
    craft("Cut")
    craft("Sew")
    talk("NPC_Chief", [2])  # festival news (choice C)
    talk("NPC_SilkMaster")  # STORY_SILK
    talk("NPC_Chief", [0])  # rest -> EndDay:D5
elif STAGE == "d5close":
    story.debug_close_day_end()
    state("festival morning (the preparation days were cut)")
elif STAGE == "jump_festival":
    story.debug_jump_to_day(9)
    state("festival morning")
elif STAGE == "fest":
    talk("NPC_Chief")  # Q40 -> Q41
    for sign in ("SIGN_MARKET", "SIGN_CRAFT", "SIGN_CULTURE"):
        interact(station(sign))
        finish_dialogue()
    state("explored")
    talk("NPC_Chief", [0])  # challenge accepted -> material choice dialogue opens
    finish_dialogue([0])
    state("materials chosen")
    craft("ChDye")
    craft("ChWeave")
    craft("ChTailor")
    craft("ChStage")  # Q43 completes -> FinalScore -> FEST_SUCCESS dialogue
    state("final score")
elif STAGE == "fest_end":
    story.debug_close_day_end()
    state("the end")

