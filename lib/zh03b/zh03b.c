#include "zh03b.h"

#include <string.h>

#include "esp_timer.h"

#define ZH03B_BAUD_RATE      9600
#define ZH03B_RX_BUFFER_SIZE 256
#define ZH03B_TX_TIMEOUT_MS  100

static const uint8_t CMD_STREAM[9] = {0xFF, 0x01, 0x78, 0x40, 0, 0, 0, 0, 0x47};
static const uint8_t CMD_QUERY_MODE[9] = {0xFF, 0x01, 0x78, 0x41, 0, 0, 0, 0, 0x46};
static const uint8_t CMD_READ[9] = {0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79};
static const uint8_t CMD_SLEEP[9] = {0xFF, 0x01, 0xA7, 0x01, 0, 0, 0, 0, 0x57};
static const uint8_t CMD_WAKE[9] = {0xFF, 0x01, 0xA7, 0, 0, 0, 0, 0, 0x58};

typedef enum {
    FRAME_STREAM,
    FRAME_QUERY,
    FRAME_DORMANT_ACK,
} frame_kind_t;

static esp_err_t send_command(const zh03b_t *device, const uint8_t command[9])
{
    int written = uart_write_bytes(device->uart_num, command, 9);
    if (written != 9) {
        return ESP_FAIL;
    }
    return uart_wait_tx_done(device->uart_num, pdMS_TO_TICKS(ZH03B_TX_TIMEOUT_MS));
}

/* Slide by one byte after a bad candidate so an embedded header is not lost. */
static esp_err_t read_frame(const zh03b_t *device, frame_kind_t kind,
                            zh03b_measurement_t *value, bool *ack_success,
                            uint32_t timeout_ms)
{
    const size_t frame_size = kind == FRAME_STREAM ? 24 : 9;
    uint8_t buffer[24];
    size_t used = 0;
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;

    while (esp_timer_get_time() < deadline_us) {
        int64_t remaining_us = deadline_us - esp_timer_get_time();
        uint32_t wait_ms = (uint32_t)((remaining_us + 999) / 1000);
        if (wait_ms > 100) {
            wait_ms = 100;
        }
        TickType_t wait_ticks = pdMS_TO_TICKS(wait_ms);
        if (wait_ticks == 0) {
            wait_ticks = 1;
        }
        uint8_t byte;
        int count = uart_read_bytes(device->uart_num, &byte, 1, wait_ticks);
        if (count < 0) {
            return ESP_FAIL;
        }
        if (count == 0) {
            continue;
        }

        buffer[used++] = byte;
        if (used < frame_size) {
            continue;
        }

        bool valid = false;
        if (kind == FRAME_STREAM) {
            valid = zh03b_parse_stream_frame(buffer, frame_size, value);
        } else if (kind == FRAME_QUERY) {
            valid = zh03b_parse_query_frame(buffer, frame_size, value);
        } else {
            valid = zh03b_parse_dormant_ack(buffer, frame_size, ack_success);
        }
        if (valid) {
            return ESP_OK;
        }
        memmove(buffer, buffer + 1, frame_size - 1);
        used = frame_size - 1;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t zh03b_init(zh03b_t *device, const zh03b_config_t *config)
{
    if (device == NULL || config == NULL || config->uart_num <= UART_NUM_0 ||
        config->uart_num >= UART_NUM_MAX || config->tx_gpio < 0 ||
        config->rx_gpio < 0 || config->tx_gpio == config->rx_gpio ||
        (config->mode != ZH03B_MODE_STREAM && config->mode != ZH03B_MODE_QUERY)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uart_config_t uart_config = {
        .baud_rate = ZH03B_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_param_config(config->uart_num, &uart_config);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(config->uart_num, config->tx_gpio, config->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_driver_install(config->uart_num, ZH03B_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }

    device->uart_num = config->uart_num;
    device->mode = ZH03B_MODE_STREAM;
    device->initialized = true;
    device->dormant = false;
    err = zh03b_set_mode(device, config->mode);
    if (err != ESP_OK) {
        zh03b_deinit(device);
    }
    return err;
}

void zh03b_deinit(zh03b_t *device)
{
    if (device != NULL && device->initialized) {
        uart_driver_delete(device->uart_num);
        device->initialized = false;
    }
}

esp_err_t zh03b_set_mode(zh03b_t *device, zh03b_mode_t mode)
{
    if (device == NULL || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mode != ZH03B_MODE_STREAM && mode != ZH03B_MODE_QUERY) {
        return ESP_ERR_INVALID_ARG;
    }
    if (device->dormant) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = uart_flush_input(device->uart_num);
    if (err != ESP_OK) {
        return err;
    }
    err = send_command(device, mode == ZH03B_MODE_STREAM ? CMD_STREAM : CMD_QUERY_MODE);
    if (err == ESP_OK) {
        device->mode = mode;
    }
    return err;
}

esp_err_t zh03b_read(zh03b_t *device, zh03b_measurement_t *out, uint32_t timeout_ms)
{
    if (device == NULL || !device->initialized || device->dormant) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out == NULL || timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (device->mode == ZH03B_MODE_QUERY) {
        esp_err_t err = uart_flush_input(device->uart_num);
        if (err != ESP_OK) {
            return err;
        }
        err = send_command(device, CMD_READ);
        if (err != ESP_OK) {
            return err;
        }
        return read_frame(device, FRAME_QUERY, out, NULL, timeout_ms);
    }
    return read_frame(device, FRAME_STREAM, out, NULL, timeout_ms);
}

esp_err_t zh03b_set_dormant(zh03b_t *device, bool dormant, uint32_t timeout_ms)
{
    if (device == NULL || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (device->dormant == dormant) {
        return ESP_OK;
    }
    esp_err_t err = uart_flush_input(device->uart_num);
    if (err != ESP_OK) {
        return err;
    }
    err = send_command(device, dormant ? CMD_SLEEP : CMD_WAKE);
    if (err != ESP_OK) {
        return err;
    }
    bool success = false;
    err = read_frame(device, FRAME_DORMANT_ACK, NULL, &success, timeout_ms);
    if (err == ESP_OK && !success) {
        return ESP_FAIL;
    }
    if (err == ESP_OK) {
        device->dormant = dormant;
    }
    return err;
}
