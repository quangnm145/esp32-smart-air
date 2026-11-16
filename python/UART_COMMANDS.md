# Giao tiếp UART với cảm biến ZH03B

Tóm tắt từ [ZH03B.PDF](../ZH03B.PDF), tài liệu Winsen phiên bản 2.1 (15/04/2019), chủ yếu trang 3–7. Các vị trí byte dưới đây **đánh số từ 0**. Lệnh và giá trị PM trong datasheet dùng byte cao trước (big-endian).

## Thông số và chân kết nối

| Mục | Giá trị |
| --- | --- |
| UART | 9600 baud, 8 data bit, không parity, 1 stop bit (8N1) |
| Mức tín hiệu UART | TTL 3.3 V; không phải RS-232 |
| Nguồn cảm biến | DC 4.9–5.5 V; pin 1 là VDD, pin 2 là GND |
| Chân UART cảm biến | Pin 4 RXD nhận lệnh; pin 5 TXD xuất dữ liệu |
| Chu kỳ đo | Khoảng 1 giây; thời gian ổn định sau cấp nguồn khoảng 30 giây |

Nối TXD cảm biến → RX bộ USB-UART, RXD cảm biến ← TX bộ USB-UART, và GND chung. Nếu chỉ đọc chế độ tự gửi thì không cần nối RXD cảm biến. Đối chiếu chiều đánh số chân với hình tại trang 4 của PDF trước khi cắm dây.

## Các lệnh gửi cho cảm biến

Mỗi lệnh gồm 9 byte. Byte cuối là checksum của byte 1 đến byte 7 theo công thức ở phần dưới. Chỉ cần gửi lệnh chuyển chế độ **một lần**; ở chế độ hỏi–đáp, gửi lệnh đọc mỗi lần cần một mẫu đo.

| Mục đích | 9 byte gửi (hex) | Phản hồi |
| --- | --- | --- |
| Chuyển sang tự gửi | `FF 01 78 40 00 00 00 00 47` | Sau đó đọc khung `42 4D` |
| Chuyển sang hỏi–đáp | `FF 01 78 41 00 00 00 00 46` | Sau đó gửi lệnh đọc |
| Đọc một lần trong chế độ hỏi–đáp | `FF 01 86 00 00 00 00 00 79` | Khung 9 byte `FF 86 ...` |
| Vào chế độ ngủ | `FF 01 A7 01 00 00 00 00 57` | Khung `FF A7 ...` báo thành công/thất bại |
| Thoát chế độ ngủ | `FF 01 A7 00 00 00 00 00 58` | Khung `FF A7 ...` báo thành công/thất bại |

Chế độ mặc định theo trang 5 là **tự gửi**, nên cảm biến mới cấp nguồn không cần lệnh kích hoạt. Datasheet không nêu phản hồi xác nhận riêng cho hai lệnh chuyển chế độ; hãy xác nhận bằng loại khung đo nhận được. Tài liệu cũng không nêu rõ chế độ có được lưu qua lần mất nguồn hay không.

## Khung dữ liệu tự gửi (trang 5)

Tổng 24 byte. Hai byte `00 14` ở vị trí 2–3 biểu thị 20 byte theo sau, bao gồm checksum. Dữ liệu bắt đầu bằng `42 4D`.

| Vị trí byte | Ý nghĩa |
| --- | --- |
| 0–1 | Header `42 4D` |
| 2–3 | Độ dài `00 14` |
| 4–9 | Dữ liệu dành riêng |
| 10–11 | PM1.0, µg/m³ |
| 12–13 | PM2.5, µg/m³ |
| 14–15 | PM10, µg/m³ |
| 16–21 | Dữ liệu dành riêng |
| 22–23 | Checksum 16 bit, byte cao trước |

Checksum: `sum(frame[0:22]) & 0xFFFF` phải bằng giá trị ở byte 22–23. Không diễn giải các byte *dành riêng* thành số đo khác chỉ dựa trên ví dụ dữ liệu. Dải giá trị PM theo datasheet là 0–1000 µg/m³.

Ví dụ trong datasheet: `42 4D 00 14 00 54 00 6E 00 7C 00 54 00 6E 00 7C 00 00 00 00 00 00 03 1F` → PM1.0 = 84, PM2.5 = 110, PM10 = 124 µg/m³; checksum = `031F`.

## Phản hồi hỏi–đáp (trang 6–7)

Sau khi chuyển sang chế độ hỏi–đáp, gửi lệnh đọc `FF 01 86 00 00 00 00 00 79`. Phản hồi dài 9 byte:

| Vị trí byte | Ý nghĩa |
| --- | --- |
| 0–1 | Header `FF 86` |
| 2–3 | PM2.5, µg/m³ |
| 4–5 | PM10, µg/m³ |
| 6–7 | PM1.0, µg/m³ |
| 8 | Checksum 8 bit |

**Thứ tự PM trong phản hồi hỏi–đáp khác khung tự gửi.** Ví dụ datasheet: `FF 86 00 85 00 96 00 65 FA` → PM1.0 = 101, PM2.5 = 133, PM10 = 150 µg/m³.

## Checksum của lệnh và phản hồi 9 byte

Không cộng byte đầu `FF`. Với `b[0] ... b[8]`, checksum là:

```text
b[8] = (-sum(b[1:8])) & 0xFF
```

Khi kiểm tra khung đầy đủ: `sum(b[1:9]) & 0xFF` phải bằng 0. Ví dụ lệnh đọc có byte 1–7 là `01 86 00 00 00 00 00`; tổng là `0x87`, nên checksum là `0x79`. **Khung 24 byte dùng phép cộng 16 bit khác**, như mô tả ở trên.

## Phản hồi cho lệnh ngủ/thức (trang 6)

Phản hồi gồm 9 byte, bắt đầu `FF A7`. Byte 2 là `01` khi thành công, `00` khi thất bại; byte 3–7 bằng `00` trong bảng của datasheet. Checksum lần lượt là `58` (thành công) hoặc `59` (thất bại). Tài liệu không mô tả thêm phản hồi riêng cho lệnh chuyển giữa tự gửi và hỏi–đáp.

## Chạy chương trình test trên COM6

Mở PowerShell tại thư mục `python`. Xem [README.md](README.md) để cài `pyserial` vào `.venv` nếu chưa có.

```powershell
# Chuyển sang tự gửi và đọc liên tục
.\.venv\Scripts\python.exe .\test_zh03b.py --set-active

# Chuyển sang hỏi–đáp và hỏi mỗi giây
.\.venv\Scripts\python.exe .\test_zh03b.py --query

# Hiện byte TX/RX để kiểm tra giao thức
.\.venv\Scripts\python.exe .\test_zh03b.py --query --dump
```

Chương trình hiện hỗ trợ lệnh chuyển chế độ và đọc PM; các lệnh ngủ/thức được ghi ở đây theo datasheet nhưng chưa có tùy chọn CLI tương ứng.
