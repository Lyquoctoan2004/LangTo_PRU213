# Chỉnh độ lệch tóc Groom (Messy Bun / Wave Undercut) so với đầu nhân vật.
# Cách chạy trong UE: Tools > Execute Python Script... > chọn file này.
# Sau khi chạy: mở CO_Character > Compile > Play để xem.
#
# OFFSET (cm): dịch mesh nguồn so với SK_SourceNasim gốc.
#   Z âm  -> tóc đi LÊN trên đầu nhân vật   | Z dương -> tóc đi XUỐNG
#   Y âm  -> tóc đi về PHÍA TRƯỚC           | Y dương -> tóc đi RA SAU
# SCALE: >1 -> tóc co lại sát đầu hơn       | <1 -> tóc phồng ra xa đầu hơn
import unreal

HAIRS = {
    "MessyBun": dict(offset=(0.0, -0.43, -12.77), scale=1.0,
                     binding="/Game/Character/Grooms/Nasim/GB_UpdoMessyBun_Nasim"),
    "WaveUndercut": dict(offset=(0.0, -0.67, 7.55), scale=1.0,
                         binding="/Game/Character/Grooms/Nasim/GB_NomadWaveUndercut_Nasim"),
}

SRC = "/Game/Character/Grooms/Nasim/SK_SourceNasim"
HEAD_CENTER = unreal.Vector(0.0, 2.6, 160.6)  # tâm đầu của SK_SourceNasim (để scale quanh đầu)

EAL = unreal.EditorAssetLibrary
sme = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
src = unreal.load_asset(SRC)

_pending = []  # giữ delegate sống tới khi build xong


def _build_and_save(binding):
    """Build binding (async) và chỉ save khi build đã xong."""
    d = unreal.OnGroomBindingAssetBuildComplete()

    def _done(asset, success):
        unreal.EditorAssetLibrary.save_loaded_asset(binding, False)
        unreal.log("Hair fix: binding built (%s) + saved: %s" % (success, binding.get_name()))

    d.bind_callable(_done)
    _pending.append((d, _done))
    binding.build(d)

for name, cfg in HAIRS.items():
    dst = SRC + "_" + name
    if not EAL.does_asset_exist(dst):
        EAL.duplicate_asset(SRC, dst)
    sk = unreal.load_asset(dst)
    for lod in range(sme.get_lod_count(sk)):
        # luôn đọc từ mesh gốc để thay đổi không bị cộng dồn
        dm = unreal.DynamicMesh()
        rl = unreal.GeometryScriptMeshReadLOD(); rl.lod_index = lod
        dm = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
            src, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), rl)[0]
        s = cfg["scale"]
        if s != 1.0:
            unreal.GeometryScript_MeshTransforms.scale_mesh(dm, unreal.Vector(s, s, s), HEAD_CENTER)
        ox, oy, oz = cfg["offset"]
        unreal.GeometryScript_MeshTransforms.translate_mesh(dm, unreal.Vector(ox, oy, oz))
        wl = unreal.GeometryScriptMeshWriteLOD(); wl.lod_index = lod
        unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(
            dm, sk, unreal.GeometryScriptCopyMeshToAssetOptions(), wl)
    EAL.save_loaded_asset(sk, False)

    b = unreal.load_asset(cfg["binding"])
    b.set_editor_property("source_skeletal_mesh", sk)
    _build_and_save(b)
    unreal.log("Hair fix: %s offset=%s scale=%s" % (name, cfg["offset"], s))
