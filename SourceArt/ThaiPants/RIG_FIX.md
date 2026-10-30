# ThaiPants rig correction

Updated asset: `/Game/Character/ThaiPants/SK_ThaiPants`.

The working mesh is `SK_ThaiPants` in `ThaiPants_rigfix.blend`. The separate, pre-existing `ThaiPants_Fit` object was not used for this update.

- Interpolated body skin weights over the nearest triangle, normalized to at most eight influences per vertex.
- Excluded opposite-leg influences below the crotch.
- Adjusted close-fitting vertices and corrected local clearance in two bent-leg poses. The largest pose correction was approximately 21.81 mm.
- Preserved the existing skeleton, UVs, material assignment, and garment length.
- Reimported `SK_ThaiPants.glb` and saved the Unreal skeletal mesh with an eight-influence limit and high-precision skin weights.

`rigfix_validation.json` contains before/after measurements. Tests use standing, stride, and high-step FK poses in Blender with the full body visible. These are signed nearest-surface vertex checks; they do not prove absence of all triangle intersections or cover every Unreal animation/corrective-bone pose. `validate_rig.py` reruns these checks in the saved Blender scene.

Backups: `ThaiPants_before_rigfix.blend` and `Backup_before_rigfix/` (previous source GLB and Unreal asset).
