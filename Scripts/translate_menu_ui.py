import unreal


# Translates the fixed texts of the main menu, settings menu, lobby HUD and the Mutable customization GUI widgets to
# Vietnamese, in the widget assets themselves (so the designer shows them too). The labels the customization GUI fills
# in at run time (parameter, section and option names from CO_Character) are translated by UUiTranslationSubsystem.
# Safe to re-run.
TEXTS = {
    "/Game/Lobby/UIElements/BasicMenu": {
        "TextBlock": "Xác nhận tạo hình", "SettingsBackButtonLabel": "Quay lại", "SettingsButtonLabel": "Cài đặt",
        "TextBlock_177": "Bắt đầu", "TextBlock_2": "Màn Infinite", "TextBlock_5": "Màn Showcase", "TextBlock_1": "Tùy chọn",
        "TextBlock_77": "Khung lưới", "TextBlock_4": "Thoát", "BrightnessHint": "Điều chỉnh độ sáng màn hình.",
        "BrightnessRange": "Kéo sang trái để tối, sang phải để sáng.", "BrightnessTitle": "Độ sáng", "SettingsTitle": "CÀI ĐẶT",
        "TextBlock_162": "MENU CHÍNH",
    },
    "/Game/SilkVillage/UI/WBP_SettingsMenu": {
        "Amluongtong": "Âm lượng tổng", "CAIDAT": "CÀI ĐẶT",
    },
    "/Game/Lobby/HUD": {name: None for name in ()},  # filled below by text
    "/Game/UI/DynamicallyGeneratedGUI_DGGUI/Layers_DGGUI": {"AddLayerText": "Thêm hình dán +"},
    "/Game/UI/DynamicallyGeneratedGUI_DGGUI/Projector_DGGUI": {"TextBlock_134": "Độ trong suốt"},
    "/Game/UI/DynamicallyGeneratedGUI_DGGUI/Section_DGGUI": {"SectionText": "Mục"},
}
# The lobby HUD repeats the same hints in several text blocks: translate by text.
BY_TEXT = {
    "Zoom in/out": "Phóng to/thu nhỏ", "Rotate Camera": "Xoay camera", "Navigate UI": "Di chuyển giao diện",
    "Modify Projector": "Chỉnh hình dán", "Apply Projection": "Áp hình dán",
}

helpers = unreal.MutableSampleEditorHelpers
changed = 0
for path, by_name in TEXTS.items():
    bp = unreal.EditorAssetLibrary.load_asset(path)
    bp.modify()
    for obj in helpers.get_nested_objects(bp):
        if not isinstance(obj, unreal.TextBlock):
            continue
        current = str(obj.get_text())
        new = by_name.get(obj.get_name()) or BY_TEXT.get(current)
        if new and new != current:
            obj.modify()
            obj.set_text(new)
            changed += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
print("MENU_TEXTS translated", changed)
