import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
all_assets = ar.get_all_assets()
names = []
for a in all_assets:
    n = str(a.asset_name)
    cls = str(a.asset_class_path.asset_name)
    if cls in ("InputAction", "InputMappingContext"):
        names.append((n, str(a.package_name), cls))
for n in sorted(set(names)):
    print(n)
