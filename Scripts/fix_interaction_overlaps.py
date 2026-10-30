import math
import re

import unreal


# Two InteractionRange spheres that touch make E pick an unpredictable station, so ranges of DIFFERENT kinds (a mulberry bush
# beside a festival sign, a dye-water barrel inside a cloth source, an NPC beside a station) must not touch. Pass 1 shrinks
# both ranges just enough to clear by 30 cm (never below 90 cm). Pass 2 moves what still touches - only the Chapter 4-6 stations
# (tag Chapter4_6) and the plain mulberry bushes / festival material sources - to the nearest open ground. Ranges of the SAME
# kind (bush next to bush) do the same thing when pressed, so they are left alone. NPCs, spots and trays are never moved.
# Run after build_chapter4_6_village.py, then run build_meshy_props.py again so the props follow their stations.
MOVABLE_TAG = "Chapter4_6"
MARGIN = 30.0
MIN_RANGE = 90.0
RING_STEP = 120.0
RING_MAX = 1500.0
ANGLES = 16

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def kind(label):
    label = label.replace("MulberryExp", "Mulberry").replace("SilkTrayExp", "SilkTray")
    return re.sub(r"(_\d+|\d+)$", "", label)


def range_of(actor):
    for sphere in actor.get_components_by_class(unreal.SphereComponent):
        if sphere.get_name() == "InteractionRange":
            return sphere
    return None


def ground(x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 3000), unreal.Vector(x, y, -3000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    name = t[9].get_actor_label() if t[9] else ""
    ok = t[4].z < 250.0 and (isinstance(t[9], unreal.Landscape) or name.startswith(("taDvf", "SM_Nordic_Beach_Rocky")))
    return t[4].z if ok else None


def main():
    entries = []
    for actor in actor_subsystem.get_all_level_actors():
        sphere = range_of(actor)
        if sphere:
            entries.append({"actor": actor, "label": actor.get_actor_label(), "kind": kind(actor.get_actor_label()),
                            "r": sphere.get_scaled_sphere_radius(), "movable": actor.actor_has_tag(MOVABLE_TAG) or actor.get_actor_label().startswith(("Mulberry_", "Deco_"))})

    def position(e):
        p = e["actor"].get_actor_location()
        return p.x, p.y

    def conflicts(e, x, y):
        for o in entries:
            if o is e or o["kind"] == e["kind"]:
                continue
            ox, oy = position(o)
            if math.hypot(ox - x, oy - y) < e["r"] + o["r"] + MARGIN:
                return True
        return False

    # Pass 1: where two ranges of different kinds touch, shrink both in proportion (never below MIN_RANGE) so they just clear.
    shrunk = []
    for i, a in enumerate(entries):
        for b in entries[i + 1:]:
            if a["kind"] == b["kind"]:
                continue
            (ax, ay), (bx, by) = position(a), position(b)
            room = math.hypot(ax - bx, ay - by) - MARGIN
            if a["r"] + b["r"] <= room:
                continue
            factor = room / (a["r"] + b["r"])
            for e in (a, b):
                new_r = max(MIN_RANGE, e["r"] * factor)
                if new_r < e["r"] - 0.5:
                    sphere = range_of(e["actor"])
                    sphere.set_sphere_radius(sphere.get_unscaled_sphere_radius() * new_r / e["r"])
                    e["r"] = new_r
            shrunk.append("{} <> {}".format(a["label"], b["label"]))

    # Pass 2: what still touches (ranges already at their minimum) moves the bush / material source / new station.
    moved, stuck = [], []
    for e in [e for e in entries if e["movable"]]:
        x, y = position(e)
        if not conflicts(e, x, y):
            continue
        found = None
        radius = RING_STEP
        while radius <= RING_MAX and not found:
            for k in range(ANGLES):
                a = 2.0 * math.pi * k / ANGLES
                nx, ny = x + math.cos(a) * radius, y + math.sin(a) * radius
                gz = ground(nx, ny)
                if gz is not None and not conflicts(e, nx, ny):
                    found = (nx, ny, gz)
                    break
            radius += RING_STEP
        if found:
            e["actor"].set_actor_location(unreal.Vector(found[0], found[1], found[2]), False, False)
            moved.append("{} moved {:.0f} cm".format(e["label"], math.hypot(found[0] - x, found[1] - y)))
        else:
            stuck.append(e["label"])
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.EditorLevelLibrary.save_all_dirty_levels()
    print("OVERLAP_FIX shrunk_pairs={} moved={} stuck={}".format(len(shrunk), len(moved), stuck))
    for line in moved:
        print("  ", line)


main()
