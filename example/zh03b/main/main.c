#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "zh03b.h"

static const char *TAG = "ZH03B_EXAMPLE";

static void read_and_log(zh03b_t *sensor, const char *mode)
{
    zh03b_measurement_t value;
    esp_err_t err = zh03b_read(sensor, &value, 2500);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%s: PM1.0=%u PM2.5=%u PM10=%u ug/m3", mode,
                 (unsigned)value.pm1_0_ug_m3, (unsigned)value.pm2_5_ug_m3,
                 (unsigned)value.pm10_ug_m3);
    } else {
        ESP_LOGW(TAG, "%s: read failed: %s", mode, esp_err_to_name(err));
    }
}

void app_main(void)
{
    zh03b_t sensor = {0};
    const zh03b_config_t config = {
        .uart_num = UART_NUM_1,
        .tx_gpio = CONFIG_ZH03B_EXAMPLE_TX_GPIO,
        .rx_gpio = CONFIG_ZH03B_EXAMPLE_RX_GPIO,
        .mode = ZH03B_MODE_STREAM,
    };
    esp_err_t err = zh03b_init(&sensor, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART init failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "UART1 9600 8N1: ESP TX GPIO %d -> sensor RXD; ESP RX GPIO %d <- sensor TXD",
             config.tx_gpio, config.rx_gpio);
    ESP_LOGI(TAG, "Waiting 30 s for sensor stabilization after power-up");
    vTaskDelay(pdMS_TO_TICKS(30000));

    ESP_LOGI(TAG, "Automatic upload: reading five frames");
    for (int i = 0; i < 5; ++i) {
        read_and_log(&sensor, "STREAM");
    }

    err = zh03b_set_mode(&sensor, ZH03B_MODE_QUERY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not switch to Q&A: %s", esp_err_to_name(err));
        zh03b_deinit(&sensor);
        return;
    }
    ESP_LOGI(TAG, "Q&A: sending one read command per second");
    vTaskDelay(pdMS_TO_TICKS(100));
    while (true) {
        read_and_log(&sensor, "QUERY");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
