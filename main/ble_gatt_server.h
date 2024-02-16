#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

typedef enum {
	BLE_GATT_SERVER_SSID_PASSWORD_SET_EVENT
} ble_gatt_server_event_t;

typedef void (*ble_gatt_server_callback_t)(ble_gatt_server_event_t);

void ble_gatt_server_init();
void ble_gatt_server_set_th_value(uint8_t *th_value);
void ble_gatt_server_notify();
void ble_gatt_server_register_callback(ble_gatt_server_callback_t callback);

#endif
