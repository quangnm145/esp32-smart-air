# Ví dụ test riêng ZH03B trên ESP32-C3 / ESP32-S3

Project ESP-IDF độc lập dùng component [lib/zh03b](../../lib/zh03b/README.md). Nó chọn chế độ tự gửi, đọc 5 khung, chuyển sang hỏi–đáp, rồi hỏi và in PM1.0/PM2.5/PM10 liên tục. Dùng UART1 để không chiếm UART console. Mặc định target là ESP32-C3 theo board trong log thử nghiệm; có thể đổi sang ESP32-S3.

## Nối dây

| ZH03B | ESP32-C3 mặc định | ESP32-S3 mặc định |
| --- | --- | --- |
| Pin 1 VDD | Nguồn 5 V | Nguồn 5 V |
| Pin 2 GND | GND chung | GND chung |
| Pin 4 RXD | GPIO4 (UART1 TX) | GPIO17 (UART1 TX) |
| Pin 5 TXD | GPIO3 (UART1 RX) | GPIO16 (UART1 RX) |

Tín hiệu UART là **TTL 3.3 V**. Có thể đổi GPIO trong `idf.py menuconfig` → **ZH03B Example** theo chân thực tế của board. Trên ESP32-C3, GPIO12–17 thường dành cho SPI flash, vì vậy không dùng cặp GPIO16/17 mặc định của ESP32-S3. Tránh các chân đang dành cho flash, USB, UART console hoặc phần cứng khác trên board.

## Build và chạy

Mở terminal ESP-IDF, chuyển vào thư mục `example/zh03b`:

```powershell
idf.py set-target esp32c3
idf.py build
idf.py -p COM6 flash monitor
```

Đổi `COM6` thành cổng USB của **ESP32-C3**. Cổng COM của bộ USB-UART dùng lúc test Python có thể là cổng khác. Nếu dùng ESP32-S3, thay lệnh đầu thành `idf.py set-target esp32s3` và kiểm tra lại chân GPIO. Nếu board đã có `sdkconfig` đúng target thì không cần gọi lại `set-target`. Sau khi cấp nguồn, ví dụ chờ 30 giây trước khi đọc để giá trị ổn định.

Nếu log hiện `Detected size(4096k) larger than ... (2048k)`, chip đang có flash 4 MB nhưng `sdkconfig` cũ chọn 2 MB. Đây không phải lỗi UART; đổi **Serial flasher config → Flash size** sang 4 MB trong `idf.py menuconfig` rồi build lại. `sdkconfig.defaults` của ví dụ mới đã đặt 4 MB cho cấu hình mới.

Kết quả mong đợi trong log:

```text
ZH03B_EXAMPLE: STREAM: PM1.0=26 PM2.5=33 PM10=37 ug/m3
ZH03B_EXAMPLE: QUERY: PM1.0=28 PM2.5=35 PM10=39 ug/m3
```

Các số trên chỉ là ví dụ; giá trị thực phụ thuộc không khí. Nếu báo `ESP_ERR_TIMEOUT`, kiểm tra nguồn 5 V, GND chung, hai dây TX/RX và mức TTL 3.3 V. Xem [UART_COMMANDS.md](../../python/UART_COMMANDS.md) để tra cứu lệnh và checksum.
