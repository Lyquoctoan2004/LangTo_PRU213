import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
pkg = unreal.Name("/Game/Input/IMC_MouseLook")
refs = ar.get_referencers(pkg, unreal.AssetRegistryDependencyOptions())
print("REFERENCERS of IMC_MouseLook:")
for r in refs:
    print(" ", r)

pkg2 = unreal.Name("/Game/Input/Actions/IA_MouseLook")
refs2 = ar.get_referencers(pkg2, unreal.AssetRegistryDependencyOptions())
print("REFERENCERS of IA_MouseLook:")
for r in refs2:
    print(" ", r)
