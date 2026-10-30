# Báo cáo Settings và tối ưu hiệu năng — 27/10/2026

Project: LangToLua 5.7, Unreal Engine 5.7.

## Kết quả đã hoàn thành

- Settings dùng nền xanh đậm, viền vàng, chữ sáng, bố cục giữa màn hình; các mức chất lượng có nhãn tiếng Việt. Đã xem trong PIE ở cửa sổ 720p và 1080p.
- Icon bánh răng hiển thị đúng; sửa kích thước ảnh, căn đầy ô và khoảng đệm của nút. Bỏ thông báo Hello thử nghiệm.
- Khi mở Settings, đọc chất lượng đang dùng từ GameUserSettings. Thay đổi lựa chọn bằng chương trình không tự áp dụng cấu hình; lựa chọn của người chơi mới áp dụng.
- Minimap có nhịp cập nhật 0,1 giây. Đã xác nhận lại trên nhân vật chạy thật sau khi mở lại editor. Các hiệu ứng mây, khí quyển và hậu kỳ không cần thiết của minimap được tắt.
- Sửa xung đột ưu tiên cấu hình: bỏ ba dòng khóa Cloud ShadowMap, Virtual Shadow MaxPhysicalPages và ResolutionLodBiasDirectional trong DefaultEngine.ini. DefaultScalability.ini hiện điều khiển đúng theo từng mức. Các tối ưu bổ sung có sẵn trong project được giữ lại.

## Kiểm thử

| Kiểm tra | Kết quả |
|---|---|
| Ba Blueprint SettingsMenu, CoinHUD, ThirdPersonCharacter | BS_UpToDate, đã lưu |
| Thấp / Vừa: bóng mây | 0 / 0 |
| Cao / Rất cao: bóng mây | 1 / 1 |
| Số trang bóng đổ theo bốn mức | 512 / 1024 / 1536 / 2048 |
| Gọi sự kiện chọn chất lượng thực tế của widget | Áp dụng đúng cả bốn mức |
| Đổi lựa chọn bằng chương trình | Không tự đổi chất lượng |
| Thanh âm lượng | Giá trị 0,5 và 1,0; handler chạy không lỗi |
| Tiếp tục hành trình | Widget rời viewport |
| Mở lại editor | Cấu hình và tài nguyên được nạp lại đúng |

Âm thanh chưa được đánh giá bằng nghe trực tiếp. Không bấm Thoát trò chơi; giữ logic thoát hiện có.

## Hiệu năng đo được

Phép đo mới nhất: PIE trên Mountain, cửa sổ 1080p, cùng vị trí và hướng nhìn; tạm khóa đầu vào di chuyển/nhìn trong lúc đo, đợi ổn định 8 giây rồi lấy mẫu 10 giây mỗi mức.

| Mức | Trung bình khung hình | P95 |
|---|---:|---:|
| Rất cao | 51,20 ms | 55,61 ms |
| Vừa | 25,89 ms | 29,35 ms |

Đây là so sánh hai mức chất lượng trên cùng bản hiện tại, không phải mức tăng FPS của toàn game trước/sau sửa. Phép đo dùng khoảng thời gian callback Slate trong editor, không thay thế GPU trace hoặc benchmark Shipping. Một phép thử bóng mây trước đó giảm 18,37 → 10,78 ms tại vị trí khác; không cộng gộp hai kết quả.

Máy đang lưu lựa chọn Rất cao (3). Sau kiểm thử đã khôi phục lựa chọn này; để demo ưu tiên độ mượt, chọn **Vừa - Cân bằng**, hoặc **Thấp - Ưu tiên tốc độ** nếu cần. Chưa đạt bằng chứng bảo đảm 60 FPS trên toàn map.

## Bàn giao và giới hạn

- Thay đổi đã nằm trực tiếp trong project; không cần dán code.
- Bản Packaged có sẵn chưa được đóng gói lại trong đợt kiểm thử này. Cần package lại để bản exe nhận asset/config mới.
- Chưa kiểm thử toàn bộ nhiệm vụ, mọi vị trí bản đồ, mọi MetaHuman hoặc máy khác.
- Backup ban đầu: Backup_Settings_Performance_20261026. Backup cấu hình trước sửa xung đột mới nhất: Backup_Settings_Performance_20261027/Config.
- Các JSON cạnh báo cáo lưu số đo và kết quả kiểm thử. Không thay đổi dữ liệu nhiệm vụ hoặc save tiến độ để tối ưu.
