# Cấu trúc NPC – 4 model cho 7 vai trò

Story cần 7 vai: Trưởng làng + 6 nghệ nhân (nuôi tằm, ươm tơ, se tơ, nhuộm, dệt, may).
Hiện chỉ có 4 model NPC (`/Game/MeshyImports/...`). Gộp vai theo **chuỗi công đoạn liền kề**,
để mỗi NPC "dạy" 2 bước nối tiếp nhau, không phải đổi người giữa chừng trong cùng một cảnh.

| NPC (ID)            | Tên                        | Model (Skeletal Mesh)                     | Vai trò                          | Xưởng / khu vực                  | Xuất hiện chính |
| ------------------- | -------------------------- | ----------------------------------------- | -------------------------------- | -------------------------------- | --------------- |
| `NPC_Chief`         | **Bà Lò Thị Xuân**         | Elder_in_Traditional_Attire_Import0002    | Trưởng làng                      | Sân lễ hội (trung tâm làng)      | Ch1–Ch6         |
| `NPC_SilkMaster`    | **Ông Lường Văn Khánh**    | Man_in_Blue_Traditional_Attire_Import0001 | Nghệ nhân **nuôi tằm + ươm tơ**  | Nhà nuôi tằm + vườn dâu          | Ch1, Ch2, Ch3   |
| `NPC_ThreadDyer`    | **Cô Tòng Thị Mai**        | Vibrant_Highland_Heritage_Import0003      | Nghệ nhân **se tơ + nhuộm**      | Xưởng se tơ / bãi nhuộm bên suối | Ch3, Ch4        |
| `NPC_WeaverTailor`  | **Cô Quàng Thị Lụa**       | The_Crimson_Sash_Import0004               | Nghệ nhân **dệt + may**          | Xưởng dệt / nhà may              | Ch4, Ch5        |

Họ Thái (Lò, Lường, Tòng, Quàng) hợp với làng nhà sàn vùng núi có nghề nuôi tằm, dệt lụa.
Xưng hô: bà Xuân xưng "bà", ông Khánh xưng "ta", cô Mai và cô Lụa xưng "cô"; tất cả gọi người chơi là "cháu".
Model Trưởng làng là nữ, nên các câu trong tài liệu gốc gọi Trưởng làng là "ông" đã đổi thành "bà".

## Vì sao gộp như vậy

- **Nuôi tằm → ươm tơ**: cùng một vật liệu (kén). Cuối Ch1 câu "Ngày mai cháu sẽ học cách biến
  chiếc kén này thành sợi tơ" giờ do chính nghệ nhân nuôi tằm nói tiếp ở Ch2–Ch3, liền mạch hơn.
- **Se tơ → nhuộm**: sợi se xong đi nhuộm ngay. Ch4 "Trưởng làng đứng cùng nghệ nhân dệt … nghệ nhân
  nhuộm giải thích" vẫn là 2 người khác nhau (ThreadDyer + WeaverTailor), không mâu thuẫn.
- **Dệt → may**: Ch5 "Thợ dệt chỉ về phía căn nhà may" đổi thành "Đi theo ta sang góc may" – cùng NPC.
- Trưởng làng giữ riêng vì có mặt mọi chương và là người giao Main Quest.

## Thay đổi Quest 01 (Chapter 1)

Cũ: Gặp 6 nghệ nhân. Mới: **Gặp 3 nghệ nhân** (mỗi người giới thiệu 2 công đoạn):

1. Gặp nghệ nhân nuôi tằm & ươm tơ (NPC_SilkMaster) – bắt đầu mini-game Nuôi tằm.
2. Gặp nghệ nhân se tơ & nhuộm (NPC_ThreadDyer) – chỉ chào hỏi, xưởng khóa tới Ch3.
3. Gặp nghệ nhân dệt & may (NPC_WeaverTailor) – chỉ chào hỏi, xưởng khóa tới Ch4.

Dòng giới thiệu chuỗi nghề của Trưởng làng giữ nguyên
(Tằm → Kén → Tơ → Sợi → Nhuộm → Dệt → May), chỉ thêm: "Ở làng mình, mỗi nghệ nhân giữ hai công đoạn."

## Dân làng nền (không tương tác)

"Người dân đang chuẩn bị cho lễ hội" dùng lại 4 model ở xa / đứng quay lưng, không có prompt E.
Khi có thêm model, thay dần các bản nền này; NPC chính không đổi.
