# ZH03B UART driver cho ESP-IDF

Component C đọc PM1.0, PM2.5, PM10 từ cảm biến Winsen ZH03B ở 9600 8N1. Hỗ trợ tự gửi (khung 24 byte), hỏi–đáp (khung 9 byte), chuyển chế độ và lệnh ngủ/thức. Mỗi khung được kiểm tra header, độ dài, checksum và dải PM 0–1000 µg/m³. Nguồn giao thức: [ZH03B.PDF](../../ZH03B.PDF) phiên bản 2.1; thiết kế hai chế độ và chu kỳ ngủ/thức được tham khảo từ [ESPHome ZH03B component](https://github.com/Bjk8kds/esphome-zh03b-sensor). Mã ở đây được viết cho ESP-IDF bằng C, không phụ thuộc ESPHome.

## Kết nối

| ZH03B | ESP32-S3 |
| --- | --- |
| Pin 1 VDD | Nguồn 5 V |
| Pin 2 GND | GND chung |
| Pin 4 RXD | GPIO TX của UART, mức TTL 3.3 V |
| Pin 5 TXD | GPIO RX của UART, mức TTL 3.3 V |

Chọn GPIO không trùng với BMP180, DHT11, LED hoặc chức năng khác của board. Dùng UART khác UART console. Cảm biến cần khoảng 30 giây sau khi cấp nguồn để giá trị đo ổn định. Với chế độ tự gửi, có thể bỏ dây TX từ ESP32 nếu chỉ đọc, nhưng API `zh03b_init()` gửi lệnh chọn chế độ nên cấu hình hiện tại yêu cầu cả TX và RX.

## Ví dụ gọi từ task

```c
#include "zh03b.h"

static void zh03b_task(void *arg)
{
    zh03b_t sensor = {0};
    zh03b_config_t config = {
        .uart_num = UART_NUM_1,
        .tx_gpio = 17, /* ESP32 TX -> ZH03B RXD */
        .rx_gpio = 16, /* ESP32 RX <- ZH03B TXD */
        .mode = ZH03B_MODE_QUERY, /* hoặc ZH03B_MODE_STREAM */
    };
    if (zh03b_init(&sensor, &config) != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }

    /* Nếu vừa cấp nguồn cho cảm biến, đợi ổn định trước khi dùng kết quả. */
    vTaskDelay(pdMS_TO_TICKS(30000));
    while (true) {
        zh03b_measurement_t pm;
        if (zh03b_read(&sensor, &pm, 2000) == ESP_OK) {
            /* pm.pm1_0_ug_m3, pm.pm2_5_ug_m3, pm.pm10_ug_m3 */
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    zh03b_deinit(&sensor);
}
```

Component được ESP-IDF tìm tự động nhờ `EXTRA_COMPONENT_DIRS` trong `CMakeLists.txt` gốc. Thêm `zh03b` vào `REQUIRES` của component gọi API này, ví dụ `main/CMakeLists.txt` nếu gọi từ `main`.

## API và hành vi

| Hàm | Hành vi |
| --- | --- |
| `zh03b_init()` | Cấu hình UART, cài driver, gửi lệnh chuyển sang chế độ trong config. Đối tượng `zh03b_t` cần khởi tạo bằng `{0}`. |
| `zh03b_read()` | Tự gửi: đợi khung hợp lệ tiếp theo. Hỏi–đáp: xóa dữ liệu cũ, gửi một lệnh đọc, đợi một phản hồi hợp lệ. |
| `zh03b_set_mode()` | Gửi lệnh chuyển chế độ và bỏ dữ liệu cũ. Cảm biến không gửi ACK riêng cho lệnh này. |
| `zh03b_set_dormant()` | Gửi lệnh ngủ/thức, kiểm tra ACK `FF A7`. Trả lỗi nếu cảm biến báo thất bại hoặc quá thời gian. |
| `zh03b_deinit()` | Gỡ driver UART do component cài. |

Tham số `timeout_ms` là thời gian tối đa đợi một khung hợp lệ; `ESP_ERR_TIMEOUT` nghĩa là chưa nhận được khung hợp lệ trong khoảng đó. Các hàm không an toàn khi được gọi đồng thời từ nhiều task trên cùng một `zh03b_t` hoặc cùng UART. Không chia sẻ UART này với driver khác.

Chế độ hỏi–đáp **không tự làm cảm biến ngủ**: muốn tiết kiệm điện, ứng dụng cần gọi `zh03b_set_dormant(&sensor, true, timeout_ms)` giữa các lần đo và gọi `zh03b_set_dormant(&sensor, false, timeout_ms)` trước khi đọc tiếp. Thư viện ESPHome tham khảo đợi khoảng 10 giây sau lệnh thức rồi mới yêu cầu số đo; ứng dụng có thể chọn chu kỳ phù hợp. Không gọi `zh03b_read()` khi đối tượng đang đánh dấu dormant.

`zh03b_set_mode()` chỉ xác nhận việc gửi lệnh qua UART thành công; hãy gọi `zh03b_read()` để xác nhận cảm biến thực sự trả đúng loại khung. Các byte dành riêng trong khung tự gửi không được diễn giải thành số đo.
