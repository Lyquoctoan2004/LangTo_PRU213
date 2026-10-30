import unreal


# Clears the spruce patch between the silkworm house and the stream (east workshop quarter).
# Edits the persistent Mountain level: back it up first (Backup_Before_EastArea/).
# STAGE = "trees"      remove trees in ZONE (PCG instances + placed spruce/hornbeam actors)
# STAGE = "footprints" remove foliage (grass, rocks, bushes) inside FOOTPRINTS
ZONE = (41800.0, 44200.0, -39400.0, -35200.0)  # X min, X max, Y min, Y max
TREE_PCG_ACTOR = "PCG Tool Working Actor_1"  # AreaTool that scatters spruce_small_05/07 here
TREE_MESH_KEYS = ("spruce", "hornbeam")
# (centre X, centre Y, half X, half Y) of the new house plots, filled in after the trees are gone.
FOOTPRINTS = globals().get("FOOTPRINTS", [])

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def in_box(x, y, box):
    return box[0] <= x <= box[1] and box[2] <= y <= box[3]


def remove_instances(component, test):
    doomed = [i for i in range(component.get_instance_count())
              if test(component.get_instance_transform(i, True).translation)]
    if doomed:
        component.modify()
        component.remove_instances(sorted(doomed, reverse=True))
    return len(doomed)


def clear_trees():
    removed_pcg = removed_actors = 0
    for actor in eas.get_all_level_actors():
        if actor.get_name() == TREE_PCG_ACTOR:
            # Keep the generated forest outside ZONE; stop load-time regeneration from regrowing it.
            for pcg in actor.get_components_by_class(unreal.PCGComponent):
                pcg.modify()
                pcg.set_editor_property("generation_trigger", unreal.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND)
            for ism in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                removed_pcg += remove_instances(ism, lambda t: in_box(t.x, t.y, ZONE))
        elif isinstance(actor, unreal.StaticMeshActor):
            mesh = actor.static_mesh_component.static_mesh
            loc = actor.get_actor_location()
            if mesh and in_box(loc.x, loc.y, ZONE) and any(k in mesh.get_name().lower() for k in TREE_MESH_KEYS):
                eas.destroy_actor(actor)
                removed_actors += 1
    print("TREES_REMOVED pcg_instances={} placed_actors={}".format(removed_pcg, removed_actors))


def clear_footprints():
    boxes = [(cx - hx, cx + hx, cy - hy, cy + hy) for cx, cy, hx, hy in FOOTPRINTS]
    removed = 0
    for actor in eas.get_all_level_actors():
        if actor.get_class().get_name() != "InstancedFoliageActor" and not actor.get_actor_label().startswith("PCG"):
            continue
        for ism in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            removed += remove_instances(ism, lambda t: any(in_box(t.x, t.y, b) for b in boxes))
    print("FOOTPRINT_FOLIAGE_REMOVED", removed)


STAGE = globals().get("STAGE", "trees")
clear_trees() if STAGE == "trees" else clear_footprints()
print("SAVED", unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False))
