#include "zh03b.h"

static uint16_t be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static bool pm_in_range(const zh03b_measurement_t *value)
{
    return value->pm1_0_ug_m3 <= 1000 && value->pm2_5_ug_m3 <= 1000 &&
           value->pm10_ug_m3 <= 1000;
}

bool zh03b_parse_stream_frame(const uint8_t *frame, size_t length, zh03b_measurement_t *out)
{
    if (frame == NULL || out == NULL || length != 24 || frame[0] != 0x42 ||
        frame[1] != 0x4D || frame[2] != 0x00 || frame[3] != 0x14) {
        return false;
    }

    uint16_t checksum = 0;
    for (size_t i = 0; i < 22; ++i) {
        checksum = (uint16_t)(checksum + frame[i]);
    }
    if (checksum != be16(&frame[22])) {
        return false;
    }

    zh03b_measurement_t value = {
        .pm1_0_ug_m3 = be16(&frame[10]),
        .pm2_5_ug_m3 = be16(&frame[12]),
        .pm10_ug_m3 = be16(&frame[14]),
    };
    if (!pm_in_range(&value)) {
        return false;
    }
    *out = value;
    return true;
}

bool zh03b_parse_query_frame(const uint8_t *frame, size_t length, zh03b_measurement_t *out)
{
    if (frame == NULL || out == NULL || length != 9 || frame[0] != 0xFF || frame[1] != 0x86) {
        return false;
    }

    uint8_t checksum = 0;
    for (size_t i = 1; i < 9; ++i) {
        checksum = (uint8_t)(checksum + frame[i]);
    }
    if (checksum != 0) {
        return false;
    }

    zh03b_measurement_t value = {
        .pm1_0_ug_m3 = be16(&frame[6]),
        .pm2_5_ug_m3 = be16(&frame[2]),
        .pm10_ug_m3 = be16(&frame[4]),
    };
    if (!pm_in_range(&value)) {
        return false;
    }
    *out = value;
    return true;
}

bool zh03b_parse_dormant_ack(const uint8_t *frame, size_t length, bool *success)
{
    if (frame == NULL || success == NULL || length != 9 || frame[0] != 0xFF ||
        frame[1] != 0xA7 || frame[2] > 1) {
        return false;
    }
    for (size_t i = 3; i < 8; ++i) {
        if (frame[i] != 0) {
            return false;
        }
    }
    uint8_t checksum = 0;
    for (size_t i = 1; i < 9; ++i) {
        checksum = (uint8_t)(checksum + frame[i]);
    }
    if (checksum != 0) {
        return false;
    }
    *success = frame[2] == 1;
    return true;
}
