# Test ZH03B trên COM6

ZH03B mặc định tự gửi dữ liệu UART ở 9600 baud, 8N1. Script kiểm tra header, độ dài và checksum rồi hiển thị PM1.0, PM2.5, PM10 (µg/m³).

Xem [UART_COMMANDS.md](UART_COMMANDS.md) để tra cứu khung dữ liệu, lệnh chuyển chế độ, hỏi–đáp, ngủ/thức và checksum theo datasheet.

## Kết nối

- ZH03B pin 1 (VDD) → nguồn 5 V; pin 2 (GND) → GND của nguồn và bộ USB-UART.
- ZH03B pin 5 (TXD, mức 3.3 V) → RX của bộ USB-UART đang hiện là COM6.
- Chế độ tự gửi không cần nối RXD của cảm biến. Không nối tín hiệu UART mức RS-232 trực tiếp vào cảm biến.

Nếu Windows ghi `Prolific USB-to-Serial`, tên này chỉ xác nhận driver/cổng COM; hãy kiểm tra **đầu ra thực tế của bộ chuyển đổi**. ZH03B cần USB-UART **TTL 3.3 V** có chân RX/TX/GND. Cáp RS-232 đầu DB9 không thể nối trực tiếp với chân UART của cảm biến.

## Chạy

Mở PowerShell tại thư mục `python`. Python đi kèm ESP-IDF có thể không cài sẵn `pip`, vì vậy tạo môi trường riêng cho chương trình:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install pyserial
.\.venv\Scripts\python.exe .\test_zh03b.py
```

Nếu `.venv` đã được tạo và đã cài `pyserial`, chỉ cần chạy lệnh cuối. Không thêm dấu `\` sau tên gói `pyserial`. Lệnh `python3` trên Windows có thể trỏ tới Microsoft Store thay vì Python đang dùng.

Tùy chọn: `--port COM6`, `--count 10` để dừng sau 10 khung, `--raw` để in byte gốc. Dừng bằng Ctrl+C. Nếu không nhận được khung hợp lệ, kiểm tra cổng COM có đang được chương trình khác dùng, dây TX/GND, nguồn và chế độ gửi của cảm biến. Chờ khoảng 30 giây sau khi cấp nguồn để giá trị ổn định.

Nếu quạt chạy nhưng chưa có số đo, đóng mọi Serial Monitor đang dùng COM6 rồi xem dữ liệu thô:

```powershell
.\.venv\Scripts\python.exe .\test_zh03b.py --dump
```

`RX: ...` cho thấy PC đã nhận byte UART; nếu chỉ hiện `No valid frame`, kiểm tra TXD pin 5 của ZH03B đã nối với RX của USB-UART và nối GND chung. Nếu có byte nhưng không ra giá trị PM, gửi lại vài dòng `RX:` để kiểm tra giao thức hoặc baud rate.

Nếu đã xác nhận bộ USB-UART dùng mức TTL 3.3 V và dây đúng nhưng cảm biến có thể đang ở chế độ hỏi–đáp, nối thêm TX của USB-UART → pin 4 RXD của ZH03B rồi chạy:

```powershell
.\.venv\Scripts\python.exe .\test_zh03b.py --set-active --dump
```

Tùy chọn `--set-active` gửi một lần lệnh `FF 01 78 40 00 00 00 00 47` theo trang 6 của datasheet, sau đó đọc dữ liệu. Không dùng tùy chọn này với bộ chuyển đổi RS-232.

Để chuyển sang chế độ hỏi–đáp và yêu cầu số đo mỗi giây, giữ cả hai dây UART TX/RX rồi chạy:

```powershell
.\.venv\Scripts\python.exe .\test_zh03b.py --query --dump
```

Chương trình gửi lệnh chế độ `FF 01 78 41 00 00 00 00 46`, rồi gửi yêu cầu `FF 01 86 00 00 00 00 00 79` mỗi giây. Phản hồi hợp lệ bắt đầu bằng `FF 86` và dài 9 byte. Muốn quay về chế độ tự gửi, chạy lại với `--set-active`.

Để xem cổng COM hiện có:

```powershell
.\.venv\Scripts\python.exe -m serial.tools.list_ports
```

Giao thức và chân kết nối theo [tài liệu ZH03B của Winsen](https://www.winsen-sensor.com/d/files/manual/zh03b.pdf).
