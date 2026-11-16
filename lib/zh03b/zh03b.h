#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZH03B_MODE_STREAM = 0,
    ZH03B_MODE_QUERY,
} zh03b_mode_t;

typedef struct {
    uint16_t pm1_0_ug_m3;
    uint16_t pm2_5_ug_m3;
    uint16_t pm10_ug_m3;
} zh03b_measurement_t;

typedef struct {
    uart_port_t uart_num;  /* Use a UART not shared with the ESP-IDF console. */
    int tx_gpio;          /* ESP32 TX -> ZH03B pin 4 RXD, 3.3 V TTL. */
    int rx_gpio;          /* ESP32 RX <- ZH03B pin 5 TXD, 3.3 V TTL. */
    zh03b_mode_t mode;
} zh03b_config_t;

typedef struct {
    uart_port_t uart_num;
    zh03b_mode_t mode;
    bool initialized;
    bool dormant;
} zh03b_t;

/* Owns the selected ESP-IDF UART driver until zh03b_deinit().
 * Sends the selected mode command; the sensor does not ACK mode changes.
 * Call from one task at a time. */
esp_err_t zh03b_init(zh03b_t *device, const zh03b_config_t *config);
void zh03b_deinit(zh03b_t *device);

/* Changes sensor mode and discards buffered bytes from the previous mode. */
esp_err_t zh03b_set_mode(zh03b_t *device, zh03b_mode_t mode);

/* STREAM: wait for a 24-byte upload frame. QUERY: send one read command and
 * wait for its 9-byte reply. timeout_ms covers the whole operation.
 * Returns ESP_ERR_TIMEOUT if no valid frame arrives in time. */
esp_err_t zh03b_read(zh03b_t *device, zh03b_measurement_t *out, uint32_t timeout_ms);

/* Sends dormant/wake command and validates its 9-byte ACK. After waking,
 * allow the sensor to stabilize before relying on PM values (30 s after
 * initial power-up per datasheet). */
esp_err_t zh03b_set_dormant(zh03b_t *device, bool dormant, uint32_t timeout_ms);

/* Pure protocol helpers, also useful for host-side tests. */
bool zh03b_parse_stream_frame(const uint8_t *frame, size_t length, zh03b_measurement_t *out);
bool zh03b_parse_query_frame(const uint8_t *frame, size_t length, zh03b_measurement_t *out);
bool zh03b_parse_dormant_ack(const uint8_t *frame, size_t length, bool *success);

#ifdef __cplusplus
}
#endif
