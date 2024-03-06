#pragma once

#include "sensor_data.h"

#define SSID_MAX_LEN 32
#define PASSWORD_MAX_LEN 64

typedef enum {
    BLE_GATT_SERVER_SSID_PASSWORD_SET_EVENT
} ble_gatt_server_event_t;

typedef void (*ble_gatt_server_callback_t)(ble_gatt_server_event_t);

void ble_gatt_server_init();
void ble_gatt_server_set_sensor_data(struct sensor_data sd_value_input);
void ble_gatt_server_notify();
void ble_gatt_server_register_callback(ble_gatt_server_callback_t callback);
