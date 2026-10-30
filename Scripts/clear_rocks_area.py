import json

import unreal


# Removes the scattered foliage rocks (SM_MossyForest* boulders and rocks) inside a box of the Mountain map, for a tidier
# village. They are foliage instances, not actors. Every removed instance is written to
# SourceArt/removed_rocks_<name>.json (mesh, location, rotation, scale) so a run can be undone by hand.
# Edit BOX (x0, x1, y0, y1 in cm) and NAME to clear another area.
BOX = (39200.0, 42200.0, -43500.0, -40500.0)  # the dirt patch and festival spots west of the square (run 2; run 1 was x 40300-42600, y -42200..-39900)
NAME = "west_of_square"
ROCK_KEYS = ("mossyforestboulder", "mossyforestrock")
OUT = unreal.Paths.project_dir() + "SourceArt/removed_rocks_{}.json".format(NAME)

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def in_box(p):
    return BOX[0] <= p.x <= BOX[1] and BOX[2] <= p.y <= BOX[3]


def clear():
    removed = []
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_class().get_name() != "InstancedFoliageActor":
            continue
        for comp in actor.get_components_by_class(unreal.FoliageInstancedStaticMeshComponent):
            mesh = comp.static_mesh
            if not mesh or not any(k in mesh.get_name().lower() for k in ROCK_KEYS):
                continue
            indices = []
            for i in range(comp.get_instance_count()):
                t = comp.get_instance_transform(i, True)
                if in_box(t.translation):
                    indices.append(i)
                    r = t.rotation.rotator()
                    removed.append({"mesh": mesh.get_path_name(), "loc": [t.translation.x, t.translation.y, t.translation.z],
                                    "rot": [r.roll, r.pitch, r.yaw], "scale": [t.scale3d.x, t.scale3d.y, t.scale3d.z]})
            if indices:
                comp.remove_instances(indices)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(removed, f)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    print("ROCKS_REMOVED", len(removed), "saved to", OUT)


# To undo: the removed instances (mesh, location, rotation, scale) are in the JSON above; re-add them with the Foliage tool, or
# restore Content/__ExternalActors__ and Content/Mountain.umap from a backup.


clear()
