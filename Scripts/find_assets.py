import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
all_assets = ar.get_all_assets()
names = []
for a in all_assets:
    n = str(a.asset_name)
    if "Destination" in n or "ClickMove" in n or "SetDestination" in n or "TopDown" in n:
        names.append((n, str(a.package_name), str(a.asset_class_path.asset_name)))
for n in sorted(set(names)):
    print(n)
